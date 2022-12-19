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
//player常量
const int HUMAN = 0;
const int GHOST = 1;


const int initLighting = 3;
const int initMines = 1;


//地图长宽
const int GraphWith = 77;
const int GraphHeight = 41;

//方块类别,六种,用不同的颜色表示即可
const int ROAD = 2;
const int BARRIER = 3;

//随机生成地图时使用，表示即将成为陆地
const int PRE_ROAD = 4;
//随机生成地图时使用，表示已经确定成为路
const int CERTAIN_ROAD = 5;
const int CERTAIN_BARRIER = 6;
const int MINE = 7;
const int KEY = 8;
const int GATE = 9;
const int DEAD = 10;

const int ALL_KEYS_NUM = 6;
const int NEEDED_FOUND_KEYS = 4;

const int UP = 1001;
const int DOWN = 1002;
const int LEFT = 1003;
const int RIGHT = 1004;





//游戏当前状态






