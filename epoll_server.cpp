
#include "game.h"
#include <sys/epoll.h>
#include <list>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <thread>
#include <pthread.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include "include/json.hpp"
#include <semaphore.h>

using namespace std;
using json = nlohmann::json;

list<int> clients_list;

#define SERVER_IP "127.0.0.1"

#define SERVER_PORT 8889

#define EPOLL_SIZE 5000

#define BUF_SIZE 0XFFFF

#define JOIN_KEY 1234 //加入游戏的消息队列key

#define SERVER_SEG 456 //服务器级别的同步信号量，用来维护map资源

typedef struct MyOperation {
    int operationType;
    int clientFd;
    uint32_t playerId;
} OP;

//消息传递结构体
typedef struct msg {
    int mtype;
    OP op;
} msg;

//游戏服务器,主要是接收用户通信并且更新内部的服务器状态,允许同时运行多个房间
int sendOperation(Operation o, int fd);

//多线程共享资源，需要使用互斥锁维护资源
class Server {
public:
    map<int, Game *> rooms; //维护各个房间状态
    map<uint32_t, int> playerid_fd; //维护玩家id和fd的对应关系
    map<int, int> roomId_msgId; //维护房间id和消息队列id的对应关系
    pthread_mutex_t myMutex; //互斥锁,保护公共资源，game为私有资源
    int queue_id; //加入房间消息专用消息队列
public:
    Server() {
        //构造函数
        rooms = map<int, Game *>();
        playerid_fd = map<uint32_t, int>();
        myMutex = PTHREAD_MUTEX_INITIALIZER;//初始化互斥锁
        queue_id = msgget(JOIN_KEY, IPC_CREAT | 0666);
        if (queue_id == -1) { //创建消息队列失败
            perror("msgget");
            exit(1);
        }
    }

    void onMessage(Operation o, int fd) {
        //处理来自客户端的消息，并且需要返回OPeration
        //首先判断操作类型，是否是加入游戏
        switch (o.operationType) {
            case LEAVE_ROOM : {
                //离开房间
                Game *game = rooms[o.roomId];
                for (auto it = game->players.begin(); it != game->players.end(); it++) {
                    if (it->id == o.playerId) {
                        game->players.erase(it);
                        //释放文件描述符
                        playerid_fd.erase(o.playerId);
                        break;
                    }
                }
                //如果房间内没有人了，就删除房间
                if (game->players.empty()) {
                    rooms.erase(o.roomId);
                    free(game);
                } else {
                    //如果房间内还有人，就通知其他人
                    Operation operation = Operation();
                    operation.operationType = LEAVE_ROOM;
                    operation.playerId = o.playerId;
                    for (auto &p: game->players) {
                        sendOperation(operation, p.fd);
                    }
                }
                break;
            }
            default: {
                //根据玩家的id号，找到对应的房间，交给房间来处理，房间会返回block，server负责广播，前端会处理剩下的逻辑
                Game &game = *rooms[o.roomId];
                Operation *op = game.handle_message(o);
                //操作可能失败，需要注意判断
                if (op != nullptr) {
                    for (auto &p: game.players) {
                        sendOperation(*op, p.fd);
                    }
                    free(op);
                }
                //注意op
                break;
            }
        }
    }

    void leaveRoom(const uint32_t i) {
        //离开房间
        //加锁
        pthread_mutex_lock(&myMutex);
        for (auto it = rooms.begin(); it != rooms.end(); it++) {
            bool isFinded = false;
            for (auto it2 = it->second->players.begin(); it2 != it->second->players.end(); it2++) {
                if (it2->id == i) {
                    sem_wait(&it->second->sem);
                    isFinded = true;
                    bool isGameStart = false;
                    if (it->second->blocks.size()!=0) {
                        isGameStart = true;
                    }
                    //检查房间是否为空
                    if (it->second->players.size() == 1) {
                        it->second->players.erase(it2);
                        sem_post(&it->second->sem);
                        free(it->second);
                        rooms.erase(it);
                        //删除消息队列
                        msgctl(roomId_msgId[it->first], IPC_RMID, NULL);
                        pthread_mutex_unlock(&myMutex);//解锁
                        return;
                    }
                    //游戏是否已经开始
                    if (isGameStart) {
                        if (it2->identity == GHOST) {
                            //如果是鬼退出游戏，则人类直接胜利
                            Operation operation = Operation();
                            operation.operationType = GAME_END;
                            operation.message = "ghost leave room , human win!";
                            operation.roomId = it->second->id;
                            it->second->players.erase(it2);
                            pthread_mutex_unlock(&myMutex);//解锁
                            for (auto &p: it->second->players) {
                                sendOperation(operation, p.fd);
                            }
                            //删除所有玩家
                            it->second->players.clear();
                            sem_post(&it->second->sem);

                            return;
                        } else {
                            //死亡人数+1
                            it->second->deadNums++;
                            //检查死亡人数
                            if (it->second->deadNums >= 2) {
                                Operation operation = Operation();
                                operation.operationType = GAME_END;
                                operation.message = "Ghost win!";
                                operation.roomId = it->second->id;
                                it->second->players.erase(it2);
                                pthread_mutex_unlock(&myMutex);//解锁

                                for (auto &p: it->second->players) {
                                    sendOperation(operation, p.fd);
                                }
                                //删除所有玩家
                                it->second->players.clear();
                                sem_post(&it->second->sem);
                                //释放资源
                                return;
                            } else {
                                ostringstream buffer;
                                Operation operation = Operation();
                                operation.operationType = UPDATE;
                                buffer << it2->nickName << " leave room";
                                operation.message = buffer.str();
                                it2.operator*().isDead = true;
                                operation.players.push_back(*it2);
                                operation.roomId = it->second->id;

                                it->second->players.erase(it2);

                                pthread_mutex_unlock(&myMutex);//解锁

                                for (auto &p: it->second->players) {
                                    sendOperation(operation, p.fd);
                                }
                                sem_post(&it->second->sem);
                                return;
                            }
                        }
                    } else {
                        pthread_mutex_unlock(&myMutex);//解锁
                        //通知其他人，有人离开房间
                        Operation operation = Operation();
                        operation.operationType = LEAVE_ROOM;
                        operation.playerId = i;
                        operation.roomId = it->second->id;

                        it->second->players.erase(it2);

                        for (auto &p: it->second->players) {
                            sendOperation(operation, p.fd);
                        }
                        //如果是游戏结束，需要删除所有的玩家
                        if (operation.operationType==GAME_END) {
                            it->second->players.clear();
                        }
                        sem_post(&it->second->sem);
                        return;
                    }
                }
            }
        }
        pthread_mutex_unlock(&myMutex);
    };

};

//创建server
Server server = Server();

//维护全局游戏状态
int sendOperation(Operation o, int fd) {
    //将Operation转换为json字符串
    json j = o;
    string s = j.dump();
    //需要注意添加上消息长度字段
    uint32_t len = s.length();
    //转化格式
    uint32_t messageSizeNet = htonl(len);
    char buf[4 + len];
    //复制数据
    memcpy(buf, &messageSizeNet, 4);
    memcpy(buf + 4, s.c_str(), len);
    //发送数据
    int ret = send(fd, buf, len + 4, 0);
    printf("send message to client %d , message : %s", fd, s.c_str());
    if (ret == -1) {
        perror("send error");
        close(fd);
        clients_list.remove(fd);
    }

    return ret;
};

int setnonblocking(int sockfd) {
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK);
    return 0;
}

Operation json2operation(const char *json_text);

void addfd(int epollfd, int fd, bool enable_et) {
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN;
    if (enable_et)
        ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
    setnonblocking(fd);
}

int handle_message(int clientfd) {
    char buf[BUF_SIZE], message[BUF_SIZE];
    bzero(buf, BUF_SIZE);
    bzero(message, BUF_SIZE);

    //从客户端接受消息，为一串json字符串
    int len = recv(clientfd, buf, BUF_SIZE, 0);
    if (len <= 0) {
        close(clientfd);
        clients_list.remove(clientfd); //server remove the client
        printf("ClientID = %d closed.\n now there are %d client in the char room\n", clientfd,
               (int) clients_list.size());
        //清除玩家信息，根据玩家套接字找到玩家的id
        for (auto &item: server.playerid_fd) {
            if (item.second == clientfd) {
                server.leaveRoom(item.first);
                break;
            }
        }
    } else {
        //实现消息的分发
        Operation operation = json2operation(buf);
        msg m;
        OP op;
        op.clientFd = clientfd;
        op.playerId = operation.playerId;
        op.operationType = operation.operationType;
        m.op = op;
        m.mtype = operation.operationType;
        if (operation.operationType == JOIN_ROOM) {
            //获取创建房间的消息队列,发送消息
            msgsnd(server.queue_id, &m, sizeof(OP), 0);
            //输出日志
        } else {
            int queue_id = server.roomId_msgId[operation.roomId];
            msgsnd(queue_id, &m, sizeof(OP), 0);
        }
    }
    return len;
}

//json 字符串转operation结构体
Operation json2operation(const char *json_text) {
    printf("json_text:%s\n", json_text);
    json json = json::parse(json_text);
    int operationType = json["operationType"];
    uint32_t playerId = json["playerId"];
    int roomId = json["roomId"];
    Operation op;
    op.operationType = operationType;
    op.playerId = playerId;
    op.roomId = roomId;
    return op;
}

void *join_handler(void *arg);

void *msg_handler(void *arg);

//epoll 监听到后，将消息放入队列，程池中的线程处理消息
//然后将结果放入队列，然后由epoll监听到后，将结果发送给客户端
int main(int argc, char *argv[]) {
    struct sockaddr_in serverAddr;

    pthread_t join_thread;
    pthread_create(&join_thread, NULL, join_handler, NULL);

    serverAddr.sin_family = PF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        exit(-1);
    }
    if (bind(listen_fd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
        perror("bind");
        exit(-1);
    }
    if (listen(listen_fd, 5) < 0) {
        perror("listen");
        exit(-1);
    }

    int epfd = epoll_create(EPOLL_SIZE);
    if (epfd < 0) {
        perror("epfd");
        exit(-1);
    }
    static struct epoll_event events[EPOLL_SIZE];
    addfd(epfd, listen_fd, true);
    while (1) {
        int epoll_events_count = epoll_wait(epfd, events, EPOLL_SIZE, -1);
        if (epoll_events_count < 0) {
            perror("epoll");
            break;
        }
        for (int i = 0; i < epoll_events_count; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == listen_fd) {
                struct sockaddr_in client_address;
                socklen_t client_addr_len = sizeof(struct sockaddr_in);
                int client_fd = accept(listen_fd, (struct sockaddr *) &client_address, &client_addr_len);

                printf("client connection from: %s : % d(IP : port), client_fd = %d \n",
                       inet_ntoa(client_address.sin_addr),
                       ntohs(client_address.sin_port),
                       client_fd);

                addfd(epfd, client_fd, true);////把这个新的客户端添加到内核事件列表

                clients_list.push_back(client_fd);
                printf("Add new client_fd = %d to epoll\n", client_fd);
                printf("Now there are %d clients int the chat room\n", (int) clients_list.size());
                char message[BUF_SIZE];
                bzero(message, BUF_SIZE);
            } else {
                //这里接收到了客户端的消息，需要对消息进行读取，交给Server处理
                int ret = handle_message(sockfd);
                if (ret < 0) {
                    perror("error");
                    exit(-1);
                }
            }
        }
    }
    close(listen_fd);
    close(epfd);
    return 0;
}

void *join_handler(void *arg) {
    int queue_id = msgget(JOIN_KEY, 0666);
    if (queue_id < 0) {
        perror("msgget");
        exit(-1);
    }
    while (true) {
        msg m;
        int res = msgrcv(queue_id, &m, sizeof(OP), 0, 0);
        if (res == -1) {
            perror("join_handler exit");
        }
        Operation o;
        o.operationType = m.op.operationType;
        o.playerId = m.op.playerId;
        int clientfd = m.op.clientFd;
        server.playerid_fd[o.playerId] = clientfd;
        bool isFinded = false;
        //join handler接收到消息，输出日志


        for (auto &item: server.rooms) {

            if (item.second->players.size() < 4) {
                //将该用户加入房间，需要根据用户信息创建一个player，信号量
                sem_wait(&item.second->sem);
                Player player = Player();
                player.id = o.playerId;
                player.fd = clientfd;
                item.second->players.push_back(player);
                isFinded = true;

                //检查是否到了四个人
                if (item.second->players.size() == USER_COUNT) {
                    //开始游戏,需要初始化游戏,并且返回初始化信息
                    item.second->initGraph();
                    //返回初始化信息
                    Operation operation = Operation();
                    operation.operationType = GAME_START;
                    operation.players = item.second->players;

                    //将所有的房间地块信息返回，作为初始化地图数据
                    operation.blocks = vector<Block>();
                    for (int i = 0; i < item.second->height; ++i) {
                        for (int j = 0; j < item.second->width; ++j) {
                            operation.blocks.push_back(item.second->blocks[i][j]);
                        }
                    }
                    operation.roomId = item.second->id;
                    sem_post(&item.second->sem);
                    //广播给所有人
                    for (auto &p: item.second->players) {
                        sendOperation(operation, p.fd);
                    }

                } else {
                    //返回加入成功信息，对于当前房间内其他玩家，是有人加入了，对于刚加入的玩家，是加入成功
                    Operation operation = Operation();
                    for (auto &p: item.second->players) {
                        if (p.id == o.playerId) {
                            operation.operationType = JOIN_SUCCESS;
                            operation.players = item.second->players;
                            operation.roomId = item.second->id;
                            sendOperation(operation, p.fd);
                        } else {
                            operation.operationType = JOIN_ROOM;
                            sendOperation(operation, p.fd);
                        }
                    }
                    sem_post(&item.second->sem);
                }
                break;
            }
        }
        if (!isFinded) {
            //创建一个房间,还需要创建对应的消息队列以及启动新的线程

            Game *game = new Game();
            Player player = Player();
            player.id = o.playerId;
            player.fd = clientfd;
            game->players.push_back(player);
            //加锁
            pthread_mutex_lock(&server.myMutex);

            server.rooms[game->id] = game;
            //返回加入成功信息
            Operation operation = Operation();
            operation.operationType = JOIN_SUCCESS;
            operation.roomId = game->id;
            operation.players = game->players;
            int *buf = (int *) malloc(sizeof(int));
            *buf = game->id;
            //创建房间专属的消息队列
            int room_queue_id = msgget(game->id, IPC_CREAT | 0666);
            if (room_queue_id == -1) {
                perror("msgget");
                exit(-1);
            }
            server.roomId_msgId[game->id] = room_queue_id;
            //解锁
            pthread_mutex_unlock(&server.myMutex);
            pthread_t tid;
            pthread_create(&tid, NULL, msg_handler, buf);
            sendOperation(operation, clientfd);
        }
    }
}

void *msg_handler(void *arg) {
    //转化为房间id参数
    pthread_detach(pthread_self());
    int roomId = *(int *) arg;
    //获取房间消息队列
    int room_queue_id = server.roomId_msgId[roomId];
    while (true) {
        msg m;
        int res = msgrcv(room_queue_id, &m, sizeof(OP), 0, 0);
        if (res == -1) {
            perror("msg_handler");
            break;
        }
        Operation o;
        o.operationType = m.op.operationType;
        o.playerId = m.op.playerId;
        o.roomId = roomId;


        switch (o.operationType) {
            case LEAVE_ROOM : {
                //离开房间
                //加锁
                pthread_mutex_lock(&server.myMutex);
                Game *game = server.rooms[o.roomId];
                //信号量保护
                sem_wait(&game->sem);
                for (auto it = game->players.begin(); it != game->players.end(); it++) {
                    if (it->id == o.playerId) {
                        game->players.erase(it);
                        //释放文件描述符
                        server.playerid_fd.erase(o.playerId);
                        pthread_mutex_unlock(&server.myMutex);
                        break;
                    }
                }
                //如果房间内没有人了，就删除房间
                if (game->players.empty()) {
                    server.rooms.erase(o.roomId);
                    pthread_mutex_unlock(&server.myMutex);
                    //删除消息队列
                    msgctl(room_queue_id, IPC_RMID, NULL);
                    sem_post(&game->sem);
                    free(game);
                    break;
                } else {
                    pthread_mutex_unlock(&server.myMutex);
                    Operation operation = Operation();
                    operation.operationType = LEAVE_ROOM;
                    operation.playerId = o.playerId;
                    for (auto &p: game->players) {
                        sendOperation(operation, p.fd);
                    }
                }
                //释放信号量
                sem_post(&game->sem);
                break;
            }
            default: {
                pthread_mutex_lock(&server.myMutex);
                //根据玩家的id号，找到对应的房间，交给房间来处理，房间会返回block，server负责广播，前端会处理剩下的逻辑
                Game &game = *server.rooms[o.roomId];
                pthread_mutex_unlock(&server.myMutex);
                //信号量保护
                sem_wait(&game.sem);
                Operation *op = game.handle_message(o);
                //操作可能失败，需要注意判断
                if (op != nullptr) {
                    for (auto &p: game.players) {
                        sendOperation(*op, p.fd);
                    }
                    free(op);
                }
                //释放信号量
                sem_post(&game.sem);
                //注意op
                break;
            }
        }
    }
}