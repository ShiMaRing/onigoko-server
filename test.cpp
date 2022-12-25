//
// Created by xgs on 22-12-25.
//
#include <iostream>
#include "include/json.hpp"
#include "game.h"
#include "operation.h"

using namespace std;
using json = nlohmann::json;
int main(){
    Player player;
    player.id=10;
    player.direct=3;
    Block block;
    block.blockType=3;
    block.playerId=3;
    Operation operation;
    operation.blocks.push_back(block);
    operation.player.push_back(player);
    operation.isSuccess= true;
    json json;
    to_json(json, operation);  // "generated" function
    std::cout << json.dump(1) << std::endl;
    return 0;



}