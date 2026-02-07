#include "../../inc/quick_seed_finder.h"
#include <cstdint>

std::string allow_str_has(uint32_t has){
    auto allow_list = {
        AHY_32,       // 矿工
        AGL_7,        // 橄榄
    };

    std::string info="";
    int cnt=1;
    for(int i=0;i<19;i++)
        if(has & (1<<i))
            cnt++;
    for(int i:allow_list){
        if(has & (1<<i))
            info += ZombieName[i]; 
        info += ',';
    }

    return info+std::to_string(cnt)+" types\n";
}
void show_part(){
    ZombieTypeList ztl(NIGHT);
    uint32_t level = 1000;
    uint32_t level_num=33;
    uint32_t real_seed = 3633537880; //0x00727A06+13+1+level*SEED_STEP;

    for (uint32_t i=0;i<level_num;i++){
        printf("%df,",2 * (level +1+ i));
        std::string info=allow_str_has(ztl.get_list(real_seed+i*SEED_STEP));
        printf("%s",info.data());
    }

    std::ofstream file("output.csv");
    for (uint32_t i=0;i<level_num;i++)
        file<<2 * (level +1+ i)<<"f,"<<allow_str_has(ztl.get_list(real_seed+i*SEED_STEP));

    file.close();
}


int main(){
    show_part();

    return 0;
}