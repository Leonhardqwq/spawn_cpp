#include "../../inc/seed_finder.h"
#include <cstdint>
#include <string>
#include <vector>

uint32_t MASK = 455681;

uint32_t TEST_NUM=0x10000;//0x10000;

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

void test_type(Zombie type){
    // 总数
    std::vector<uint32_t> giga_list;

    std::vector<std::thread> pool;
    uint64_t block = 1 + TEST_NUM/MAX_THREAD;
    for (int i=0;i<MAX_THREAD;i++){
        std::thread t([=,&block, &giga_list]{
            ZombieNumList znl(MASK);
            uint32_t idx=0;
            for(int i=0;i<znl.type_list.size();i++)
                if(znl.type_list[i]==type){
                    idx = i;break;
                }
            for(int i=0;i<block;i++){
                znl.gene();
                int giga_total=znl.num[0][idx];
                mtx.lock();
                giga_list.push_back(uint32_t(giga_total));
                mtx.unlock();
            }
        });
        pool.push_back(std::move(t));
    }
    for (auto &t: pool) t.join();

    output_csv("总数", giga_list);
    file.close();
}

int main(){
    test_type(AKG_17);
    return 0;
}