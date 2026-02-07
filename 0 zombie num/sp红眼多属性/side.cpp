#include "../../inc/seed_finder.h"
#include <cstdint>
#include <string>
#include <vector>

uint32_t MASK = 409703;

uint32_t TEST_NUM=0x1000;//0x10000;

std::ofstream file("side_output.csv");

void output_csv(std::string name,std::vector<uint32_t> vec){
    file << name;file << ",";  

    for (int j = 0; j < vec.size(); ++j) {
        file << vec[j];
        if (j < vec.size() - 1)
            file << ",";  
    }
    file<<'\n';        
}

void test_HY(){
    // 转白时刻，红总数，旗帜总数,夹零
    std::vector<uint32_t> turnwhite_list,giga_list,giga_flag_list,no9_list,giga_half_list ;

    std::vector<std::thread> pool;
    uint64_t block = 1 + TEST_NUM/MAX_THREAD;
    for (int i=0;i<MAX_THREAD;i++){
        std::thread t([=,&block, &turnwhite_list,&giga_list,&giga_flag_list,&no9_list,&giga_half_list]{
            ZombieNumList znl(MASK);
            uint32_t idx=0;
            for(int i=0;i<znl.type_list.size();i++)
                if(znl.type_list[i]==AHY_32){
                    idx = i;break;
                }
            for(int i=0;i<block;i++){
                znl.gene();
                int turn = 19, giga_total=znl.num[0][idx];
                int giga_flag=znl.num[10][idx]+znl.num[20][idx];                
                for(;turn>=1;turn--){
                    if(turn==10){
                        if(znl.num[9][idx]) break;
                        else continue;
                    } 
                    else if(znl.num[turn][idx]) break;
                }

                int giga_half = 0;
                for(int j = 1;j<=9;j++)
                    giga_half += znl.num[j][idx];
                
                mtx.lock();
                turnwhite_list.push_back(uint32_t(turn));
                giga_list.push_back(uint32_t(giga_total));
                giga_flag_list.push_back(uint32_t(giga_flag));
                no9_list.push_back(znl.num[9][idx]==0?0:1);
                giga_half_list.push_back(giga_half);
                mtx.unlock();
            }
        });
        pool.push_back(std::move(t));
    }
    for (auto &t: pool) t.join();

    output_csv("最后波次", turnwhite_list);
    output_csv("总数", giga_list);
    output_csv("旗帜总数", giga_flag_list);
    output_csv("不夹零", no9_list);
    output_csv("半场总数", giga_half_list);
    file.close();
}

int main(){
    test_HY();
    return 0;
}