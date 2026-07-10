#include "../../inc/quick_seed_finder.h"
#include <corecrt.h>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

Scene my_scene = POOL;
std::string file_name = "../../data/PE.bin";
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
    },
    {
        {AFT_21,}
    });
    finder.multi_thread_find_continue();
    // finder.show_results(final_results, 1, 13, my_scene, 1000, 25);
    finder.output_csv("data_ori.csv", final_results);
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
    std::ifstream file(std::string("data_ori.csv"));
    std::vector<std::vector<uint32_t>>data;
    std::string line;
    std::cout<<"rowId:\n";
    while (std::getline(file, line)) {
        std::vector<std::string> fields = split(line, ',');
        std::vector<uint32_t> list;
        int rowId = std::stoll(fields[0]);
        std::cout << rowId <<"\n";
        for (size_t i = 1; i < fields.size(); ++i) {
            uint32_t value = std::stoll(fields[i]);
            list.push_back(value);            
        }
        data.push_back(list);
    }
    file.close();
    return data;
};
// 只考虑饱和后的len长度
std::vector<uint32_t> get_data_len(uint32_t len){
    std::ifstream file(std::string("data_ori.csv"));
    std::vector<uint32_t>data;
    std::string line;
    while (std::getline(file, line)) {
        std::vector<std::string> fields = split(line, ',');
        std::vector<uint32_t> list;
        uint32_t rowId = std::stoll(fields[0]);
        if(rowId<len)    break;
        for (size_t i = 1; i < fields.size(); ++i) {
            uint32_t value = std::stoll(fields[i]);
            for (uint32_t j=0;j<=rowId-len;j++){
                data.push_back(uint32_t(value+SEED_STEP*j));
            }
        }
    }
    file.close();
    return data;
};

// 检验读取数据是否正确
void data_test(){
    load_data();
    BasicSeedFinder bsf;

    auto data = get_data();
    std::cout<<"row size:\n";
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
        // std::cout<<'\n';
    }    
};


bool check_del(uint32_t rng_seed){
    BasicSeedFinder bsf;
    ZombieTypeList ztl(my_scene);
/*   normal
    int cnt = 0;
    for(int i=0;i<8;i++){
        auto has = ztl.get_list_level(uint32_t(rng_seed-(8-i)*SEED_STEP), i);

        if(
            bsf.check_has(has,AHY_32)
        )
        {
            cnt++;
        }
    }
    return false;
    return cnt>0;
//  */

/*   ME半花
    int cnt_tl = 0;
    for(int i=0;i<8;i++){
        auto has = ztl.get_list_level(uint32_t(rng_seed-(8-i)*SEED_STEP), i);
        if(bsf.check_has(has,ATL_22)){
            cnt_tl++;
        }
    }
    int cnt_ft = 0;
    for(int i=0;i<8;i++){
        auto has = ztl.get_list_level(uint32_t(rng_seed-(8-i)*SEED_STEP), i);
        if(bsf.check_has(has,AFT_21)){
            cnt_ft++;
        }
    }
    int cnt_qq = 0;
    for(int i=0;i<8;i++){
        auto has = ztl.get_list_level(uint32_t(rng_seed-(8-i)*SEED_STEP), i);
        if(bsf.check_has(has,AQQ_16)){
            cnt_qq++;
        }
    }
    return cnt_tl>0 || cnt_ft<1 || cnt_qq>0;
//  */

/*   ME半花 井六炮
    int cnt_ft = 0;
    for(int i=0;i<8;i++){
        auto has = ztl.get_list_level(uint32_t(rng_seed-(8-i)*SEED_STEP), i);
        if(bsf.check_has(has,AFT_21)){
            cnt_ft++;
        }
    }
    int cnt_qq = 0;
    for(int i=0;i<10;i++){
        auto has = ztl.get_list_level(uint32_t(rng_seed-(8-i)*SEED_STEP), i);
        if(bsf.check_has(has,AQQ_16)){
            cnt_qq++;
        }
    }
    return cnt_ft<1 || cnt_qq>0;
//  */
// /*  PE最长开局无梯子
    int cnt_ft = 0;
    for(int i=0;i<8;i++){
        auto has = ztl.get_list_level(uint32_t(rng_seed-(8-i)*SEED_STEP), i);
        if(bsf.check_has(has,AFT_21)){
            cnt_ft++;
        }
    }
    return cnt_ft>0;
//  */
};

void output_csv(std::string filename,std::vector<uint32_t>& data){
    std::ofstream file(filename);
    file<<"0";
    for(auto s:data){file<<","<<s;}
    file.close();
};

// 删除前8次选卡不合条件的种子
void del_seeds(){
    // load_data();
    auto data = get_data_len(30);

    data.erase(std::remove_if(data.begin(), data.end(), check_del), data.end());

    output_csv("output.csv", data);
}
void show(){
    BasicSeedFinder tmp;
    tmp.show_list(4082342995,1,13,my_scene,1000,25,true);
    // std::cout<<tmp.str_has(11583);
}

int main(){
    // test_continue();

    data_test();
    del_seeds();

    return 0;
}