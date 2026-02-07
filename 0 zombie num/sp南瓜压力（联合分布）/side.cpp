#include "../../inc/seed_finder.h"
#include <cstdint>
#include <string>
#include <vector>

uint32_t MASK = 423176;

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

void test_NGT(){
    // 总共红眼数，白眼数，矿工数
    std::vector<uint32_t> zomboni_list[3];

    std::vector<std::thread> pool;
    uint64_t block = 1 + TEST_NUM/MAX_THREAD;
    for (int i=0;i<MAX_THREAD;i++){
        std::thread t([=,&block, &zomboni_list]{
            ZombieNumList znl(MASK);

            uint32_t idx[3]={};
            for(int i=0;i<znl.type_list.size();i++)
                if(znl.type_list[i]==AHY_32)
                    idx[0] = i;
                else if(znl.type_list[i]==ABY_23)
                    idx[1] = i;
                else if(znl.type_list[i]==AKG_17)
                    idx[2] = i;
                
            for(int i=0;i<block;i++){
                znl.gene();
                mtx.lock();
                for(int j=0;j<=2;j++)
                    zomboni_list[j].push_back(uint32_t(znl.num[0][idx[j]]));
                mtx.unlock();
            }
        });
        pool.push_back(std::move(t));
    }
    for (auto &t: pool) t.join();

    output_csv("HY", zomboni_list[0]);
    output_csv("BY", zomboni_list[1]);
    output_csv("KG", zomboni_list[2]);

    file.close();
}
int main(){
    test_NGT();
    return 0;
}