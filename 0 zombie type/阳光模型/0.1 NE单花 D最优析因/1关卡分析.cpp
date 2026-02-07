#include "../../../inc/basic_setting.h"
#include <cstdint>
#include <string>

Scene my_scene = DAY;
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

    // file<<ans_str(0xF35211BD,25);
    file<<ans_str(0x1D14D48D,10);


    file.close();
    return 0;
}