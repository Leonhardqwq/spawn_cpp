#include "../../../inc/quick_seed_finder.h"
#include <cstdint>
#include <string>

Scene my_scene = DAY;
std::string file_name = "../../../data/DE.bin";
uint64_t find_len = 0xffffffff;
//    24,25,旗帜僵博

void load_data(){
    std::ifstream file(file_name, std::ios::binary);
    if (file.is_open()) {
        file.read(reinterpret_cast<char*>(array), array_size * sizeof(uint32_t));
        if (!file) std::cerr << "Error reading file. Only " << file.gcount() << " could be read." << std::endl;
        file.close();
    } else std::cerr << "Unable to open file for reading." << std::endl;
    printf("LOAD END\n");
}
std::string ans_str(uint32_t seed,uint32_t len){
    BasicSeedFinder tmp;
    uint32_t rng_seed = seed+1+13+1000*0x65;
    uint32_t seed_idx = rng_seed*inv;
    std::string str="";

    for(int z=0;z<=18;z++){
        str += ZombieName[z];
        str += ",";
    }
    str+="权重和";
    str += ",";
    str += "\n";

    for(uint32_t i=0;i<len;i++){
        for(int z=0;z<=18;z++){
            str += tmp.check_has(array[seed_idx+i], Zombie(z))?"1":"0";
            str += ",";
        }        
        str += std::to_string(tmp.weight_has(array[seed_idx+i]));
        str += "\n";
    }
    return str;
}
int main(){
    load_data();
    std::ofstream file("1output.csv");

    file<<ans_str(0x2EEFD113,51);


    file.close();
    return 0;
}