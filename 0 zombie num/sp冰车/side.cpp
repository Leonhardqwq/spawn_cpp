#include "../../inc/seed_finder.h"
#include <cstdint>
#include <string>
#include <vector>

uint32_t MASK = 447560;

uint32_t TEST_NUM=0x00100;//0x10000;

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



void test_BC(){
    // 总共冰车数，i波冰车数
    std::vector<uint32_t> zomboni_list[21];

    std::vector<std::thread> pool;
    uint64_t block = 1 + TEST_NUM/MAX_THREAD;
    for (int i=0;i<MAX_THREAD;i++){
        std::thread t([=,&block, &zomboni_list]{
            ZombieNumList znl(MASK);
            uint32_t idx=0;
            for(int i=0;i<znl.type_list.size();i++)
                if(znl.type_list[i]==ABC_12){
                    idx = i;break;
                }
            for(int i=0;i<block;i++){
                znl.gene();
                mtx.lock();
                for(int j=0;j<=20;j++)
                    zomboni_list[j].push_back(uint32_t(znl.num[j][idx]));
                mtx.unlock();
            }
        });
        pool.push_back(std::move(t));
    }
    for (auto &t: pool) t.join();

    output_csv("total", zomboni_list[0]);
    for(int i=1;i<=20;i++)
        output_csv("wave"+std::to_string(i), zomboni_list[i]);
    file.close();
}


int main(){
    test_BC();
    return 0;
}