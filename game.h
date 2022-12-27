//
// Created by LENOVO on 2022/12/19.
//

#ifndef ONIGOKO_GAME_H
#define ONIGOKO_GAME_H

#include "constants.h"
#include "vector"
#include "map"
#include "include/json.hpp"
#include <semaphore.h> //信号量

using namespace std;


class Operation;


//玩家通用模板
class Player {
public:
    uint32_t id; //玩家id
    int identity; //当前玩家是人还是鬼
    int x;
    int y; //当前人的位置或者鬼的位置，在某一具体格子上
    //留有通信字段,例如地址端口等信息
    string nickName;
    int direct;
    int mines; //人当前的地雷数量
    int lights;//当前的照明次数
    bool isLighting;//当前是否在照明
    bool isEscaped;//是否成功逃跑
    bool isDead; //当前玩家是否死亡
    bool isDizziness;//是否被晕眩

public:
    Player(uint32_t id, int identity);

    Player();

    void BecomeHuman();

    void BecomeGhost();
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        Player, id, identity, x, y, nickName, direct, mines, lights, isLighting,
        isEscaped, isDead, isDizziness
);

//表示每一个块数据，graph由width*height个块组成
class Block {
public:
    int blockType;//方块种类
    int playerId;//若玩家存在，则需要表示玩家的id
    int x;
    int y;

    Block();

    explicit Block(int blockType);

    Block(int blockType, int x, int y);
//表示方块的位置，玩家将会接收到当前的block列表，需要根据其中的信息更新自己的客户端界面
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        Block, blockType, playerId, x, y
);

//地图类，保存游戏的当前状态,用来进行绘图操作，客户端将通过该信息更新游戏状态
class Game {
public:
    typedef struct {
        int x, y;
    } Point;
    int id;        //房间id号
    string state;  //房间状态
    int width;
    int height;
    int foundKeys;   //找到的钥匙数量
    int deadNums;    //死亡人数
    int escapedNums; //逃跑人数
    bool isDoorOpen; //逃生门是否开启
    bool isEnd;     //游戏是否结束
    bool isGhostWin;//是否是鬼胜利
    vector<vector<Block>> blocks; //地图
    vector<Player> players; //玩家
    Point gate1;//逃生门的位置
    Point gate2;
    sem_t sem; //信号量,保护当前的game对象


public:
    //初始化Graph,绘制地图,需要初始化各个block，随机生成边缘地块，钥匙，四个玩家位置
    void initGraph();

    Point randRoadPoint();

    //移动,指定方向,可能会更新block状态，例如踩到雷或者吃掉钥匙
    Block *Move(uint32_t playerId, int direct, vector<Player> &v, string &message);

    //开灯
    bool Lighting(uint32_t playerId);

    //埋雷，可能更新状态
    Block *buriedMine(uint32_t playerId);

    //关闭照明
    bool closeLight(uint32_t playerId);

    bool recoverFromDizziness(uint32_t playerId);

    Game();

    //根据玩家id操作玩家
    Operation *handle_message(Operation op);

    Player* getPlayerById(uint32_t id);

};


class Operation {
public:
    int roomId; //房间号
    uint32_t playerId; //用户号
    int operationType; //执行的操作类型
    vector<Player> players;//包含每一个玩家的状态
    vector<Block> blocks;//更新指定的地块的状态
    string message;//携带的消息
public:
    Operation();
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        Operation, roomId, playerId, operationType, players, blocks, message
);


#endif //ONIGOKO_GAME_H
