//
// Created by LENOVO on 2022/12/18.
//

#include "vector"
#include "game.h"
#ifndef ONIGOKO_OPERATION_H
#define ONIGOKO_OPERATION_H
using namespace std;
//通信数据封装
//客户端操作集合:
/* 申请创房间，
 *
 *
 *
 *
 *
 * */

//服务端响应集合：
/*
 *
 *
 *
 *
 * */

class Operation {
    int roomId; //房间号
    int playerId; //用户号
    int operationType; //执行的操作
    vector<Player> player;//包含每一个玩家的状态
    vector<Block> blocks;//更新指定的地块的状态
};


#endif //ONIGOKO_OPERATION_H
