//
// Created by LENOVO on 2022/12/18.
//

#include "vector"
#include "game.h"
#include "include/json.hpp"

#ifndef ONIGOKO_OPERATION_H
#define ONIGOKO_OPERATION_H
using namespace std;
using json = nlohmann::json;
//通信数据封装
//客户端操作集合:
/* 申请创房间，请求无参数，响应为房间号，玩家号
 * 申请加入房间，请求参数为房间号，响应为加入结果，玩家号(成功)
 * 移动人物，请求为房间号，玩家号
 * 放置地雷，请求为房间号，玩家号
 * 照明，请求为房间号，玩家号
 * 解除照明，请求为房间号，玩家号
 * 解除晕眩，请求为房间号，玩家号
 * */

//服务端响应集合：
/*  移动人物，将会更新玩家状态，block列表
 *  游戏开始，更新玩家状态，携带需要更新的block列表
 *  游戏结束，胜利方
 *  放置地雷，照明 返回当前的玩家状态
 *  解除照明，更新当前玩家状态
 * */

//客户端服务端沟通消息体
class Operation {
public:
    int roomId; //房间号
    uint32_t playerId; //用户号
    int operationType; //执行的操作类型
    vector<Player> player;//包含每一个玩家的状态
    vector<Block> blocks;//更新指定的地块的状态
    bool isSuccess;//操作是否成功
    string message;//携带的消息
public:
    Operation();
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        Operation, roomId, playerId, operationType, player, blocks, isSuccess, message
);

#endif //ONIGOKO_OPERATION_H
