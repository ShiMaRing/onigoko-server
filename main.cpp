#include <iostream>
#include "game.h"
using namespace std;
//服务器端负责通信，维护状态等操作，启动
int main() {
    Game game;
    game.initGraph();
    for (int i = 0; i < game.height; ++i) {
        for (int j = 0; j < game.width; ++j) {
            if (game.blocks[i][j].blockType == BARRIER) {
                printf("#");
            } else {
                printf("*");
            }
        }
        printf("\n");
    }
}
