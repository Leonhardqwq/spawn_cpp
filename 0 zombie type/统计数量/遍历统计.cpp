#include "../../inc/quick_seed_finder.h"
#include <corecrt.h>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

Scene my_scene = POOL;
std::string file_name = "../../data/PE.bin";
uint64_t find_len = 0xffffffff;

// uint32_t MASK = (1<<ABC_12)|(1<<AKG_17);
// uint32_t MASK = 0b110110100110110;
uint32_t MASK = (1<<AHY_32);

void load_data(){
    std::ifstream file(file_name, std::ios::binary);
    if (file.is_open()) {
        file.read(reinterpret_cast<char*>(array), array_size * sizeof(uint32_t));
        if (!file) std::cerr << "Error reading file. Only " << file.gcount() << " could be read." << std::endl;
        file.close();
    } else std::cerr << "Unable to open file for reading." << std::endl;
    printf("LOAD END\n");
}

void count_num(){
    size_t cnt=0;
    for(size_t i=0;i<=find_len;i++){
        if((array[i]&MASK)==MASK)
            cnt++;
    }
    std::cout<<cnt<<'\n'<<1.0*cnt/(1+find_len)<<'\n'<<(1+find_len);
}

void count_len(){
    size_t l=0,r=0,ans=0;
    while(l<array_size && r<array_size){
        if((array[r]&MASK)==MASK && r<array_size){
            r++;
        }
        else{
            ans = ans>(r-l)?ans:(r-l);
            r++;l=r;
        }
    }
    std::cout<<ans;
}

int main(){
    load_data();
    // count_num();
    count_len();

    return 0;
}