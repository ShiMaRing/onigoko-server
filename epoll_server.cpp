
#include "game.h"
#include "operation.h"
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
#include "include/json.hpp"
using namespace std;
using json = nlohmann::json;

list<int> clients_list;

#define SERVER_IP "127.0.0.1"

#define SERVER_PORT 8888

#define EPOLL_SIZE 5000

#define BUF_SIZE 0xFFFF

#define SERVER_WELCOME "Welcome you join  to the chat room! Your chat ID is: Client #%d"

#define SERVER_MESSAGE "ClientID %d say >> %s"

#define EXIT "EXIT"

#define CAUTION "There is only one int the char room!"

//游戏服务器,主要是接收用户通信并且更新内部的服务器状态,允许同时运行多个房间
class Server {
public:
    map<int, Game> rooms; //维护各个房间状态

public:
    Server() {
        //构造函数
        rooms=map<int,Game>();
    }

    void onMessage(Operation o) {
        //处理来自客户端的消息
    }


};

//维护全局游戏状态
Server server;

int setnonblocking(int sockfd) {
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK);
    return 0;
}


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

    if (len <= 0)
    {
        close(clientfd);
        clients_list.remove(clientfd); //server remove the client
        printf("ClientID = %d closed.\n now there are %d client in the char room\n", clientfd,
               (int) clients_list.size());
    } else
    {

    }
    return len;
}

//json 字符串转operation结构体
Operation json2operation(const char* json_text){




}


int main_server(int argc, char *argv[]) {
    //创建server
    server=Server();

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

        printf("epoll_events_count = %d\n", epoll_events_count);
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
                sprintf(message, SERVER_WELCOME, client_fd);
                int ret = send(client_fd, message, BUF_SIZE, 0);
                if (ret < 0) {
                    perror("send error");
                    exit(-1);
                }
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