#include "../../../inc/basic_setting.h"
#include <cstdint>
#include <string>

Scene my_scene = NIGHT;
//    24,25,旗帜僵博


std::string ans_str(uint32_t seed,uint32_t len){
    BasicSeedFinder tmp;
    ZombieTypeList ztl(my_scene);

    uint32_t rng_seed = seed+1+13+1000*0x65;
    std::string str="";

    for(int z=0;z<=18;z++){
        str += ZombieName[z];
        str += ",";
    }
    str+="权重和";
    str += ",";
    str += "\n";

    for(uint32_t i=0;i<len;i++){
        auto has = ztl.get_list(rng_seed+SEED_STEP*i);
        for(int z=0;z<=18;z++){
            str += tmp.check_has(has, Zombie(z))?"1":"0";
            str += ",";
        }        
        str += std::to_string(tmp.weight_has(has));
        str += "\n";
    }
    return str;
}
int main(){
    std::ofstream file("1output.csv");

    file<<ans_str(0x7B4B2E9E,16);

    file<<ans_str(0x75263B42,6);

    file<<ans_str(0x03C76D62,6);

    //uint32_t real_seed=0x75263B42; //h
    //uint32_t real_seed=0x03C76D62; //g
    //uint32_t real_seed=0x7B4B2E9E; //hg

    file.close();
    return 0;
}