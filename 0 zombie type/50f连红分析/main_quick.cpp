#include "../../inc/quick_seed_finder.h"
#include <corecrt.h>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

Scene my_scene = DAY;
std::string file_name = "../../data/DE.bin";
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
void test_continue(){
    load_data();

	final_results.clear();
    MultiQuickContinueSeedFinder finder(0,uint64_t(find_len),my_scene,
    {
    {AHY_32,},
    },{});
    finder.multi_thread_find_continue();
    finder.show_results(final_results, 1, 13, my_scene, 1000, 25);
    finder.output_csv("output.csv", final_results);
}


std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
};
std::vector<std::vector<uint32_t>> get_data(){
    std::ifstream file(file_name[11]+std::string("Egiga.csv"));
    std::vector<std::vector<uint32_t>>data;
    std::string line;
    while (std::getline(file, line)) {
        std::vector<std::string> fields = split(line, ',');
        std::vector<uint32_t> list;
        int rowId = std::stoll(fields[0]);
        std::cout << rowId <<"***\n";
        for (size_t i = 1; i < fields.size(); ++i) {
            uint32_t value = std::stoll(fields[i]);
            list.push_back(value);            
        }
        data.push_back(list);
    }
    file.close();
    return data;
};
std::vector<uint32_t> get_data25(){
    std::ifstream file(file_name[11]+std::string("Egiga.csv"));
    std::vector<uint32_t>data;
    std::string line;
    while (std::getline(file, line)) {
        std::vector<std::string> fields = split(line, ',');
        std::vector<uint32_t> list;
        uint32_t rowId = std::stoll(fields[0]);
        if(rowId<25)    break;
        for (size_t i = 1; i < fields.size(); ++i) {
            uint32_t value = std::stoll(fields[i]);
            for (uint32_t j=0;j<=rowId-25;j++){
                data.push_back(uint32_t(value+SEED_STEP*j));
            }
        }
    }
    file.close();
    return data;
};
void data_test(){
    load_data();
    BasicSeedFinder bsf;

    auto data = get_data();
    for(int i=0;i<data.size();i++){
        std::cout<<data[i].size()<<'\n';
        for(int j=0;j<data[i].size();j++){
            uint32_t seed = data[i][j]*inv;
            int ans=0;
            while(bsf.check_has(array[seed],(1<<AHY_32))){
                ans++;seed++;
            }
            // std::cout<<ans<<' ';
        }
        std::cout<<'\n';
    }    
};
int score(uint32_t rng_seed){
    BasicSeedFinder bsf;
    uint32_t idx= uint32_t(rng_seed*inv);

    int ans = 0;
    for(uint32_t i=0;i<25;i++){
        uint32_t has = array[uint32_t(idx+i)];
        if(bsf.check_has(has, (1<<AGL_7)))  ans++;
        if(bsf.check_has(has, (1<<ABC_12)))  ans++;
        // if(bsf.check_has(has, (1<<AQQ_16))) ans++;
    }
    return ans;
};
void output_csv(std::string filename,std::vector<uint32_t>& data){
    std::ofstream file(filename);
    file<<"0";
    for(auto s:data){file<<","<<s;}
    file.close();
};
void test_score(){
    load_data();
    auto data = get_data25();
    auto compare = [](uint32_t a, uint32_t b) {
        return score(a) > score(b);
    };
    std::sort(data.begin(), data.end(), compare);
    output_csv("output.csv", data);
}

void show(){
    BasicSeedFinder tmp;
    tmp.show_list(4082342995,1,13,my_scene,1000,25,true);
    // std::cout<<tmp.str_has(11583);
}
int main(){
    test_score();
    // show();

    // test_continue();
    return 0;
}