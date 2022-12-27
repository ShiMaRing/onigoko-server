//定义server，负责通信以及维护内部的各项状态
//onigoko 鬼抓人游戏
//游戏初始为3个人，1个鬼，
//3个人需要在迷宫6把钥匙中找到4把，才能够开启最后的大门逃跑
//人在迷宫中视野有限，只能感受到周围一定范围的状态，人可以通过 开灯 动作，观察全局状态
//开灯次数有限，只有三次，同时只能开启三秒钟时间，开灯后同时会把自己的位置暴露给所有玩家，包括鬼
//人有一颗地雷，能够放置地雷，鬼触碰到地雷后将会被禁锢3秒钟
//鬼视野比人大一圈，鬼不能开灯，人开灯后，鬼能够感知到人的位置，鬼移动速度略快于人
//人被抓后淘汰，人收齐钥匙后，所有人感知到出口位置，出口位置刷两个
//2人以上逃跑人胜利，否则鬼胜利
//移速设计思路，每隔一段时间，客户端一秒采集60次，维护一个状态count，人在count>=15移动一次
//鬼10移动一次

#include "string"

using namespace std;

const int HUMAN = 0;
const int GHOST = 1;
const int ROAD = 2;
const int BARRIER = 3;
const int PRE_ROAD = 4;
const int CERTAIN_ROAD = 5;
const int CERTAIN_BARRIER = 6;
const int MINE = 7;
const int KEY = 8;
const int GATE = 9;
const int DEAD = 10;


const int initLighting = 2;
const int initMines = 1;
const int GraphWith = 67;
const int GraphHeight = 37;
const int ALL_KEYS_NUM = 6;
const int NEEDED_FOUND_KEYS = 4;





const int GAME_WAITING = 0;
const int GAME_STARTING = 1;
const int GAME_ENDED = 2;

//游戏当前状态


//消息类型
const int JOIN_ROOM = 51;
const int PLACE_MINE = 53;
const int OPEN_LIGHT = 54;
const int CLOSE_LIGHT = 55;
const int RECOVER_FROM_DIZZINESS = 56;
const int UPDATE = 57;//更新客户端资源
const int GAME_START = 58;//初始化地图资源
const int GAME_END = 59;//游戏结束
const int LEAVE_ROOM = 60;
const int HEART_BEAT = 61;
const int JOIN_FAIL = 62;
const int JOIN_SUCCESS = 63;
const int UP = 1001;
const int DOWN = 1002;
const int LEFT = 1003;
const int RIGHT = 1004;

const string Ghost = "ghost";
const string P1 = "p1";
const string P2 = "p2";
const string P3 = "p3";
