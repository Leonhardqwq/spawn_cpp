#include "../../inc/seed_finder.h"
// #include <corecrt.h>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <vector>

Scene my_scene = ROOF;
// 24,25,旗帜僵博

// 展示出怪
void show(uint32_t seed,uint32_t level_num,bool use_rng){
    BasicSeedFinder tmp;
    tmp.show_list(seed,1,13,my_scene,1000,level_num,use_rng);
    printf("\n");
}
void show_level(uint32_t seed,uint32_t level_num,bool use_rng){
    BasicSeedFinder tmp;
    tmp.show_list_level(seed,1,13,my_scene,13,level_num,use_rng);
    printf("\n");
}

int main(){
    show(0x86157A03,25,0);

    /*
    for (auto seed : std::vector<uint32_t>{
497187848,1099099807,1495883864,2249655348,2249655449
    }) {
        show(seed,25,1);
    }
    //*/

    // show_level(263859731,10,1);

    return 0;
}