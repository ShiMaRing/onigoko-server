//
// Created by LENOVO on 2022/12/19.
//

#ifndef ONIGOKO_GAME_H
#define ONIGOKO_GAME_H

#include "constants.h"
#include "vector"
#include "map"

using namespace std;


//玩家通用模板
class Player {
public:
    int id; //玩家id，唯一标识玩家，room-id + player-id
    int identity; //当前玩家是人还是鬼
    int x, y; //当前人的位置或者鬼的位置，在某一具体格子上
    //留有通信字段,例如地址端口等信息


    int mines; //人当前的地雷数量
    int lights;//当前的照明次数
    bool isLighting;//当前是否在照明
    bool isEscaped;//是否成功逃跑
    bool isDead; //当前玩家是否死亡
    bool isDizziness;//是否被晕眩
public:
    Player(int id, int identity);

    void BecomeHuman();

    void BecomeGhost();
};


//表示每一个块数据，graph由width*height个块组成
class Block {
public:
    int blockType;//方块种类
    int playerId;//若玩家存在，则需要表示玩家的id
    int x, y;

    Block();

    Block(int blockType);

    Block(int blockType, int x, int y);
//表示方块的位置，玩家将会接收到当前的block列表，需要根据其中的信息更新自己的客户端界面
};

//地图类，保存游戏的当前状态,用来进行绘图操作，客户端将通过该信息更新游戏状态
class Game {
public:
    typedef struct {
        int x, y;
    } Point;
    int id;        //房间id号
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

public:
    void setPlayers(const vector<Player> &players);

    //初始化Graph,绘制地图,需要初始化各个block，随机生成边缘地块，钥匙，四个玩家位置
    void initGraph();

    Point randRoadPoint();

    //移动,指定方向,可能会更新block状态，例如踩到雷或者吃掉钥匙
    Block Move(int playerId, int direct);

    //开灯
    bool Lighting(int playerId);

    //埋雷，可能更新状态
    Block buriedMine(int playerId);

    //关闭照明
    bool closeLight(int playerId);

    bool recoverFromDizziness(int playerId);
};

#endif //ONIGOKO_GAME_H
