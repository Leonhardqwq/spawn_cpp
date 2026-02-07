#include <cstdint>
#include <fstream>
#include <iostream>
#include "../../inc/seed_finder.h"

Scene my_scene = POOL;
uint64_t find_len = 0x100; //0x100000000
std::string file_name = "../../data/temp_data/tmp.bin";

void save_data(){
    const size_t array_size = find_len; // 2^32
    uint32_t* array = new uint32_t[find_len];

    ZombieTypeList zlt(my_scene);
    uint32_t seed = 0;
    for(uint64_t i = 0; i < find_len; ++i){
        array[i] = zlt.get_list(seed);
        seed += 0x65;
    }

    std::ofstream file(file_name, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<char*>(array), array_size * sizeof(uint32_t));
        file.close();
    } else {
        std::cerr << "Unable to open file for writing." << std::endl;
    }

    delete[] array; // 释放内存
}

void load_data(){
    uint32_t* array = new uint32_t[find_len]; 
    std::ifstream file(file_name, std::ios::binary);
    if (file.is_open()) {
        file.read(reinterpret_cast<char*>(array), find_len * sizeof(uint32_t));
        if (!file) {
            std::cerr << "Error reading file. Only " << file.gcount() << " could be read." << std::endl;
        }
        file.close();
    } else {
        std::cerr << "Unable to open file for reading." << std::endl;
        delete[] array; 
    }
    for (int i=0;i<=25;i++){
        printf("%u\n",array[i]);
        BasicSeedFinder tmp;
        std::cout<<tmp.str_has(array[i]);
    }
    delete[] array;
}


int main() {
    save_data();
    // load_data();
    return 0;
}
