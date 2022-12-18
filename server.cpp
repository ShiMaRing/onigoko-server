#include "string"
#include "constants.h"
#include "vector"
#include "map"

using namespace std;


//玩家通用模板
class Player {
public:
    int id; //玩家id，唯一标识玩家
    int identity; //当前玩家是人还是鬼
    int x, y; //当前人的位置或者鬼的位置，在某一具体格子上
    //留有通信字段,例如地址端口等信息

public:
    Player(int id, int identity) : id(id), identity(identity) {
        x = 0;
        y = 0;
    }

    void setX(int x_) {
        Player::x = x_;
    }

    void setY(int y_) {
        Player::y = y_;
    }
};

//人类
class Human : public Player {
public:
    int mines; //人当前的地雷数量
    int lights;//当前的照明次数
    bool isLighting;//当前是否在照明
    bool isEscaped;//是否成功逃跑
    bool isDead; //当前玩家是否死亡
public:
    Human(int id, const int identity) : Player(id, identity) {
        mines = initMines;
        lights = initMines;//初始化照明数量以及地雷数量
        isLighting = false;
        isEscaped = false;
        isDead = false;
    }
};

//鬼
class Ghost : public Player {
public:
    //是否被晕眩
    bool isDizziness;

    Ghost(int id, const int identity) : Player(id, identity) {
        isDizziness = false;
    }
};

//表示每一个块数据，graph由width*height个块组成
class Block {
public:
    int blockType;//方块种类
    int playerId;//若玩家存在，则需要表示玩家的id
    int x, y;

    Block(int blockType) : blockType(blockType) {}
//表示方块的位置，玩家将会接收到当前的block列表，需要根据其中的信息更新自己的客户端界面
};

//地图类，保存游戏的当前状态,用来进行绘图操作，客户端将通过该信息更新游戏状态
class Game {
public:
    int id;              //房间id号
    int width;
    int height;
    vector<vector<Block>> blocks;
    vector<Player> players;

public:
    //初始化Graph,绘制地图,需要初始化各个block，随机生成边缘地块，钥匙，四个玩家位置
    void initGraph() {
        width = GraphWith;
        height = GraphHeight;
        blocks = vector<vector<Block>>(width, vector<Block>(height));
        //迷宫生成



    }


};

//游戏服务器,主要是接收用户通信并且更新内部的服务器状态,游戏内部允许同时允许多个游戏
class Server {
    map<int, Game> rooms; //维护各个房间状态





};


