
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
#include "include/json.hpp"

using namespace std;
using json = nlohmann::json;

list<int> clients_list;

#define SERVER_IP "127.0.0.1"

#define SERVER_PORT 8888

#define EPOLL_SIZE 5000

#define BUF_SIZE 0xFFFF

//游戏服务器,主要是接收用户通信并且更新内部的服务器状态,允许同时运行多个房间
int sendOperation(Operation o, int fd);

class Server {
public:
    map<int, Game *> rooms; //维护各个房间状态
    map<uint32_t, int> playerid_fd; //维护玩家id和fd的对应关系

public:
    Server() {
        //构造函数
        rooms = map<int, Game *>();
        playerid_fd = map<uint32_t, int>();
    }

    void onMessage(Operation o, int fd) {
        //处理来自客户端的消息，并且需要返回OPeration
        //首先判断操作类型，是否是加入游戏
        switch (o.operationType) {
            case JOIN_ROOM : {
                //找一个房间,如果没有合适的房间就创建一个房间，如果有就开始
                playerid_fd[o.playerId] = fd;
                bool isFinded;
                for (auto &item: rooms) {
                    if (item.second->players.size() < 4) {
                        //将该用户加入房间，需要根据用户信息创建一个player
                        Player player = Player();
                        player.id = o.playerId;
                        item.second->players.push_back(player);
                        isFinded = true;
                        //检查是否到了四个人
                        if (item.second->players.size() == 2) {
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
                            //广播给所有人
                            for (auto &p: item.second->players) {
                                sendOperation(operation, playerid_fd[p.id]);
                            }
                            return;
                        } else {
                            //返回加入成功信息，对于当前房间内其他玩家，是有人加入了，对于刚加入的玩家，是加入成功
                            Operation operation = Operation();
                            for (auto &p: item.second->players) {
                                if (p.id == o.playerId) {
                                    operation.operationType = JOIN_SUCCESS;
                                    operation.players = item.second->players;
                                    operation.roomId = item.second->id;
                                    sendOperation(operation, playerid_fd[p.id]);
                                } else {
                                    operation.operationType = JOIN_ROOM;
                                    sendOperation(operation, playerid_fd[p.id]);
                                }
                            }
                        }
                        break;
                    }
                }
                if (!isFinded) {
                    //创建一个房间
                    Game *game = new Game();
                    Player player = Player();
                    player.id = o.playerId;
                    game->players.push_back(player);
                    rooms[game->id] = game;
                    //返回加入成功信息
                    Operation operation = Operation();
                    operation.operationType = JOIN_SUCCESS;
                    operation.roomId = game->id;
                    operation.players = game->players;
                    sendOperation(operation, fd);
                }
                break;
            }
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
                        sendOperation(operation, playerid_fd[p.id]);
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
                        sendOperation(*op, playerid_fd[p.id]);
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
        for (auto it = rooms.begin(); it != rooms.end(); it++) {
            for (auto it2 = it->second->players.begin(); it2 != it->second->players.end(); it2++) {
                if (it2->id == i) {
                    it->second->players.erase(it2);
                    //检查房间是否为空
                    if (it->second->players.empty()) {
                        rooms.erase(it);
                        break;
                    }
                }
            }
        }
    };

    int sendOperation(Operation o, int fd) {
        //将Operation转换为json字符串
        json j = o;
        string s = j.dump();
        //发送给客户端
        int ret = send(fd, s.c_str(), s.size(), 0);
        if (ret == -1) {
            cout << "send:" << s << "  fail" << endl;
            perror("send error");
            close(fd);
            clients_list.remove(fd);
        }
        //输出日志
        cout << "send:" << s << endl;
        return ret;
    };
};

//创建server
Server server = Server();

//维护全局游戏状态


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

    // receive message
    printf("read from client(clientID = %d)\n", clientfd);

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
        Operation operation = json2operation(buf);
        //传递给server
        server.onMessage(operation, clientfd);

    }
    return len;
}

//json 字符串转operation结构体
Operation json2operation(const char *json_text) {
    //输出字符串
    std::cout << json_text << std::endl;
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


int main(int argc, char *argv[]) {
    struct sockaddr_in serverAddr;
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

                // 服务端发送欢迎信息
                printf("welcome message\n");
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