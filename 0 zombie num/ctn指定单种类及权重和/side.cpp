#include "../../inc/quick_seed_finder.h"
#include <corecrt.h>
#include <cstdint>
#include <string>
#include <vector>


Scene my_scene = DAY;
std::string file_name = "../../data/DE.bin";

uint32_t THREAD_TEST_NUM=10000;

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

void load_data(){
    std::ifstream file(file_name, std::ios::binary);
    if (file.is_open()) {
        file.read(reinterpret_cast<char*>(array), array_size * sizeof(uint32_t));
        if (!file) std::cerr << "Error reading file. Only " << file.gcount() << " could be read." << std::endl;
        file.close();
    } else std::cerr << "Unable to open file for reading." << std::endl;
    printf("LOAD END\n");
}

void myshow(){
    load_data();
    BasicSeedFinder tmp;
    for(uint32_t i=0;i<16;i++)
    std::cout<<tmp.weight_has(array[i]);
}

void test_type(Zombie type){
    load_data();

    // 总数,权重
    std::vector<uint32_t> giga_list,weight_list;

    std::vector<std::thread> pool;
    size_t block = THREAD_TEST_NUM;
    for (size_t i=0;i<MAX_THREAD;i++){
        std::thread t([=,&i,&block, &giga_list,&weight_list]{
            for(size_t j=0;j<block;j++){
                auto mask = array[uint32_t(i*block+j)];

                BasicSeedFinder bsf;
                uint32_t weight = bsf.weight_has(mask);

                ZombieNumList znl(mask);
                uint32_t idx=1000;
                for(int k=0;k<znl.type_list.size();k++)
                    if(znl.type_list[k]==type){
                        idx = k;break;
                    }
                
                znl.gene();
                uint32_t giga_total=idx!=1000?znl.num[0][idx]:0;

                mtx.lock();
                giga_list.push_back(giga_total);
                weight_list.push_back(weight);
                mtx.unlock();
            }
        });
        pool.push_back(std::move(t));
    }
    for (auto &t: pool) t.join();

    output_csv("总数", giga_list);
    output_csv("权重", weight_list);

    file.close();
}

int main(){
    test_type(AKG_17);
    return 0;
}