#include "../../inc/seed_finder.h"
#include "../../inc/json.hpp"

#include <corecrt.h>
#include <chrono>
#include <sstream>

using json = nlohmann::json;

Scene my_scene = POOL;
uint64_t find_len = 0xffffffff;

void test_kmp(){
    // 读取配置文件
    std::ifstream fin("seed_cracker_config.json");
    if(!fin){ 
        std::cerr << "Failed to open config file." << std::endl;
        return;
    }
    json j;
    fin>>j;
    my_scene = Scene(j["scene"]); 
    uint32_t start_level = uint32_t(j["start_flag"]) / 2;
    uint32_t user_id = j["user_id"];
    uint32_t mode_id = j["mode_id"];
    std::vector<uint32_t> masks = j["masks"].get<std::vector<uint32_t>>();

    // 暴力破解
	final_results.clear();
    MultiKMPFinder finder(uint32_t(0), uint64_t(find_len), my_scene, masks);
    finder.multi_thread_find_kmp();
    if(final_results.empty() || final_results[0].seeds.size()==0){
        printf("NOT FOUND\n");
        return;
    }
    printf("FOUND\n");

    // 处理结果
    // finder.show_results(final_results, 1, 13, my_scene, 1000, 8);
    auto rng_seeds = final_results[0].seeds;
    std::vector<uint32_t> real_seeds;
    for(auto rng_seed:rng_seeds){
        auto real_seed = finder.get_real_seed(rng_seed, user_id, mode_id, start_level);
        real_seeds.push_back(real_seed);
    }

    // 展示结果
    std::ofstream fout("output.csv");
    if (!fout) return;
    for (size_t i = 0; i < real_seeds.size(); ++i){
        std::ostringstream oss;
        oss << std::uppercase;
        oss << std::hex << std::setw(sizeof(uint32_t) * 2) << real_seeds[i];
        std::string s = oss.str();
        fout << s << "\n";
    }
    fout.close();
    // finder.output_csv("output.csv", final_results);
}

int main(){
    auto start = std::chrono::high_resolution_clock::now();
    test_kmp();
    auto end = std::chrono::high_resolution_clock::now(); 
    printf("Total time: %.2lf seconds\n", std::chrono::duration<double>(end - start).count());
    printf("Press Enter to exit...\n");
    fflush(stdout);
    std::cin.get();
    return 0;
}
