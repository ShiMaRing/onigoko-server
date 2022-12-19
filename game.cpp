//
// Created by LENOVO on 2022/12/19.
//

#include <ctime>
#include "game.h"
#include <cstdlib>
#include <random>
#include "queue"

Player::Player(int id, int identity) : id(id), identity(identity) {
}

void Player::BecomeHuman() {
    identity = HUMAN;
    mines = initMines; //人当前的地雷数量
    lights = initLighting;//当前的照明次数
}

void Player::BecomeGhost() {
    identity = GHOST;
}

Block::Block(int blockType) : blockType(blockType) {
    x = 0;
    y = 0;
}

Block::Block(int blockType, int x, int y) : blockType(blockType), x(x), y(y) {
}

Block::Block() {
}

//初始化状态
void Game::initGraph() {
    width = GraphWith;
    height = GraphHeight;
    blocks = vector<vector<Block>>(height, vector<Block>(width));
    default_random_engine e(time(0));
    //全部填充障碍物
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            blocks[i][j] = Block(BARRIER, i, j);
        }
    }
    //交叉填充路
    for (int p = 1; p < height - 1; p += 2) {
        for (int q = 1; q < width - 1; q += 2) {
            blocks[p][q].blockType = ROAD;
        }
    }
    //随机找一个起点，作为第一个逃生门
    srand((unsigned) time(NULL));
    int start_x = 1;
    int start_y = 1 + e() % (width - 2);

    while (blocks[start_x][start_y].blockType != ROAD) {
        start_y = 1 + e() % (width - 2);
    }
    //生成起点,并且把周围的点全部都加入至队列中
    blocks[start_x][start_y].blockType = CERTAIN_ROAD;
    vector<pair<int, int>> q;//队列，里面包含了所有需要处理的pre_road下标
    int direct[4][2] = {{-1, 0},
                        {1,  0},
                        {0,  1},
                        {0,  -1}};
    for (const auto &item: direct) {
        int x = start_x + item[0];
        int y = start_y + item[1];
        if (x > 0 && x < height - 1 && y > 0 && y < width - 1) {
            q.push_back({x, y});
            blocks[x][y].blockType = PRE_ROAD;//蓝色，可能会成为路
        }
    }
    //开始创造路
    while (!q.empty()) {
        //随机选择一个
        int randIdx = e() % q.size();

        pair<int, int> p = q[randIdx];
        q.erase(q.begin() + randIdx);
        //检查
        blocks[p.first][p.second].blockType = CERTAIN_BARRIER;//成为障碍物
        for (int k = 0; k < 4; k++) {
            int x = p.first + direct[k][0];
            int y = p.second + direct[k][1];
            if (p.first <= 0 || p.first >= height - 1 || p.second == 0 || p.second >= width - 1) {
                continue;
            }
            if (x > 0 && x < height - 1 && y > 0 && y < width - 1 && blocks[x][y].blockType == ROAD) {
                //如果还有路可以走
                blocks[p.first][p.second].blockType = CERTAIN_ROAD;//成为路
                blocks[x][y].blockType = CERTAIN_ROAD;

                for (int m = 0; m < 4; m++) {
                    int next_x = x + direct[m][0];
                    int next_y = y + direct[m][1];
                    if (next_x > 0 && next_x < height - 1 && next_y > 0 && next_y < width - 1 &&
                        blocks[next_x][next_y].blockType == BARRIER) {
                        blocks[next_x][next_y].blockType = PRE_ROAD;
                        q.push_back({next_x, next_y});
                    }
                }
            }
        }
    }
    int end_x = height - 2;
    int end_y = 1 + e() % (width - 2);
    while (blocks[end_x][end_y].blockType != CERTAIN_ROAD) {
        end_y = 1 + e() % (width - 2);
    }
    for (int i = 1; i < height - 1; ++i) {
        for (int j = 1; j < width - 1; ++j) {
            if (blocks[i][j].blockType == CERTAIN_ROAD) {
                blocks[i][j].blockType = ROAD;
            }
            if (blocks[i][j].blockType == CERTAIN_BARRIER) {
                blocks[i][j].blockType = BARRIER;
            }
        }
    }
    //设置门的位置,放置gate
    gate1.x = start_x;
    gate1.y = start_y;
    gate2.x = end_x;
    gate2.y = end_y;
    blocks[start_x][start_y].blockType = GATE;
    blocks[end_x][end_y].blockType = GATE;
    //key随机创造6把钥匙，至少找到4把才能开
    for (int i = 0; i < ALL_KEYS_NUM; ++i) {
        Point point = randRoadPoint();
        blocks[point.x][point.y].blockType = KEY;
    }
    //player放置,随机生成一个ghost
    int randGhostIdx = e() % players.size();
    for (int i = 0; i < players.size(); i++) {
        Point point = randRoadPoint();
        blocks[point.x][point.y].playerId = players[i].id;
        players[i].x = point.x;
        players[i].y = point.y;
        if (i == randGhostIdx) {
            players[i].BecomeGhost();
        } else {
            players[i].BecomeHuman();
        }
    }
}

void Game::setPlayers(const vector<Player> &players) {
    Game::players = players;
}

Game::Point Game::randRoadPoint() {
    default_random_engine e(time(0));
    int rand_x = 1 + e() % (height - 2);
    int rand_y = 1 + e() % (width - 2);
    while (blocks[rand_x][rand_y].blockType != ROAD) {
        rand_x = 1 + e() % (height - 2);
        rand_y = 1 + e() % (width - 2);
    }
    return Point{rand_x, rand_y};
}

//照明，观察全局位置，同时鬼得到位置
bool Game::Lighting(int playerId) {
    for (auto &item: players) {
        if (item.id == playerId) {
            //死亡或者已逃跑则不允许执行该操作,操作合法性由客户端进行判断
            if (item.lights <= 0 || item.isDead || item.isEscaped || item.identity == GHOST) {
                return false;
            }
            item.isLighting = true;
            item.lights--;
            return true;
        }
    }//玩家状态更新，需要进行广播
}

//埋雷，只有玩家能看到，鬼看不到，
Block Game::buriedMine(int playerId) {
    for (auto &item: players) {
        if (item.id == playerId) {
            //在当前位置埋雷
            int x = item.x;
            int y = item.y;
            if (blocks[x][y].blockType == MINE || item.mines <= 0 ||
                item.isDead || item.isEscaped || item.identity == GHOST) {
                return NULL; //当前位置已经有雷了，禁止埋雷,不需要更新
            }
            //否则开始埋雷
            blocks[x][y].blockType = MINE;//需要更新状态,
            return blocks[x][y];
        }
    }
}

//到时间后关闭照明,如果已经死亡或者已经逃跑就不需要执行
bool Game::closeLight(int playerId) {
    for (auto &item: players) {
        if (item.id == playerId) {
            if (item.isDead || item.isEscaped || item.identity == GHOST) {
                return false;
            }
            item.isLighting = false;
            return true;
        }
    }
}

//鬼从晕眩状态恢复
bool Game::recoverFromDizziness(int playerId) {
    for (auto &item: players) {
        if (item.id == playerId) {
            if (!item.isDizziness || item.identity == HUMAN) {
                return false;
            }
            item.isDizziness = false;
            return true;
        }
    }
}

//移动,核心游戏逻辑，一次逻辑单元为某一用户向某一方向移动了一个距离单位
Block Game::Move(int playerId, int direct) {
    //首先判断用户身份
    for (auto &item: players) {
        if (item.id == playerId) {
            //人已经死亡或者已经逃脱，鬼被晕眩，都不允许移动
            if (item.isDead || item.isEscaped || item.isDizziness) {
                return NULL;
            }
            int x = item.x;
            int y = item.y;
            int identity = item.identity;
            //先进行移动，之后进行逻辑判断
            switch (direct) {
                case UP:
                    x = x - 1;
                    break;
                case DOWN:
                    x = x + 1;
                    break;
                case LEFT:
                    y = y - 1;
                    break;
                case RIGHT:
                    y = y + 1;
                    break;
            }
            //检测
            if (blocks[x][y].blockType == BARRIER) {
                return NULL;
            }
            //碰撞检测
            for (int i = 0; i < players.size(); ++i) {
                if (players[i].x == x && players[i].y == y) {
                    //两个都是人类禁止重叠
                    if (players[i].identity == HUMAN && identity == HUMAN) {
                        return NULL;//重叠禁止
                    } else if (players[i].identity == GHOST && identity == HUMAN) {
                        //人移动主动撞鬼
                        item.isDead = true;
                    } else {
                        //鬼抓到人
                        players[i].isDead = true;
                    }
                    //更新新的值,客户端需要更新
                    item.x = x;
                    item.y = y;
                    blocks[x][y].blockType = DEAD;
                    //判断死亡人数是否超过两个，超过两个为鬼胜利
                    deadNums++;
                    if (deadNums >= 2) {
                        //鬼获得游戏胜利,更新游戏状态
                        isEnd = true;
                        isGhostWin = true;
                    }
                    //返回可能出现的死者
                    return blocks[x][y];
                }
            }
            //更新新的位置
            item.x = x;
            item.y = y;
            switch (item.identity) {
                case GHOST:
                    //鬼移动,可能碰上地雷
                    if (blocks[x][y].blockType == MINE) {
                        item.isDizziness = true;
                        blocks[x][y].blockType = ROAD;//地雷消失
                        return blocks[x][y];
                    }
                    break;
                case HUMAN:
                    //可能吃钥匙
                    if (blocks[x][y].blockType == KEY) {
                        foundKeys++;
                        blocks[x][y].blockType = ROAD;//钥匙消失
                        //可能会更新大门
                        if (foundKeys >= NEEDED_FOUND_KEYS) {
                            isDoorOpen = true;//此时需要更新大门
                        }
                        return blocks[x][y];
                    } else if (blocks[x][y].blockType == GATE) {//跑到大门处
                        item.isEscaped = true;
                        escapedNums++;
                        if (escapedNums >= 2) {
                            isEnd = true;
                            isGhostWin = false;
                        }
                        return NULL;
                    }
                    break;
            }
            break;
        }
    }

}


