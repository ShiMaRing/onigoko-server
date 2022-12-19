#include "string"
#include "game.h"
#include "operation.h"

//游戏服务器,主要是接收用户通信并且更新内部的服务器状态,允许同时运行多个房间
class Server {
public:
    map<int, Game> rooms; //维护各个房间状态

public:
    void onMessage(Operation o){
        //处理来自客户端的消息


    }

};


