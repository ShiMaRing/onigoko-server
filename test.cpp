//
// Created by xgs on 22-12-25.
//
#include <iostream>
#include "include/json.hpp"
#include "game.h"

using namespace std;
using json = nlohmann::json;

int main_test() {
    Player player;
    player.id = 1;
    printf("players id %d", player.id);
    Player *p = &player;
    Player &tmp = *p;
    tmp.id = 2;
    printf("players id %d", player.id);
    return 0;

}