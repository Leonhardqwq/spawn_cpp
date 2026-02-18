#pragma once
#include "basic_setting.h"
#include "json.hpp"

#include <fstream>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

using json = nlohmann::json;


class SunConfig {
public:
    Scene scene = NIGHT;

    int neg_value = -1;
    uint32_t density_threshold = 10500;

    uint32_t user_id = 1;
    uint32_t mode_id = 13;

    uint32_t level_start = 1000;
    uint32_t level_num = 50;

    double sun_start = 9990;
    double sun_end = -500;
    bool sun_cap = true;

    uint32_t mode = 1;

    uint32_t mode1_sim_num = 10000;
    uint32_t mode1_output_num = 500;

    uint32_t mode2_seed = 0xFFFFFFFF;

    std::vector<int> factor1;
    std::vector<int> factor2;
    std::vector<double> coef;

    SunConfig(const char* filename) {
        std::ifstream fin(filename);
        if(!fin){ 
            std::cerr << "无法打开 config.json" << std::endl;
            return;
        }
        json j;
        fin>>j;

        scene = Scene(j["scene"]);

        neg_value = j["neg_value"];
        density_threshold = j["density_threshold"];

        user_id = j["user_id"];
        mode_id = j["mode_id"];

        level_start = j["level_start"];
        level_num = j["level_num"];

        sun_start = j["sun_start"];
        sun_end = j["sun_end"];
        sun_cap = j["sun_cap"];

        mode = j["mode"];

        mode1_sim_num = j["mode1_sim_num"];
        mode1_output_num = j["mode1_output_num"];

        mode2_seed = j["mode2_seed"];

        // 0-18 zombie, 19 space, 20 density
        factor1 = j["factor1"].get<std::vector<int>>();
        factor2 = j["factor2"].get<std::vector<int>>();
        coef = j["coef"].get<std::vector<double>>();
    }

    void show_info(){
        std::cout << "scene: " << scene << std::endl;
        std::cout << "neg_value: " << neg_value << std::endl;
        std::cout << "density_threshold: " << density_threshold << std::endl;
        std::cout << "user_id: " << user_id << std::endl;
        std::cout << "mode_id: " << mode_id << std::endl;
        std::cout << "level_start: " << level_start << std::endl;
        std::cout << "level_num: " << level_num << std::endl;
        std::cout << "sun_start: " << sun_start << std::endl;
        std::cout << "sun_end: " << sun_end << std::endl;
        std::cout << "sun_cap: " << sun_cap << std::endl;
        std::cout << "mode: " << mode << std::endl;
        std::cout << "mode1_sim_num: " << mode1_sim_num << std::endl;
        std::cout << "mode1_output_num: " << mode1_output_num << std::endl;
        std::cout << "mode2_seed: " << mode2_seed << std::endl;

        std::cout << "factor1 * factor2 = coef: " << std::endl;
        for (int i = 0; i < factor1.size(); ++i) {
            auto f1 = factor1[i];
            auto f2 = factor2[i];
            auto c = coef[i];

            if (f1 < 19) std::cout << ZombieName[f1];
            else if (f1 == 19) std::cout << "空空";
            else if (f1 == 20) std::cout << "密度";
            std::cout << f1;

            std::cout << " * ";

            if (f2 < 19) std::cout << ZombieName[f2];
            else if (f2 == 19) std::cout << "空空";
            else if (f2 == 20) std::cout << "密度";
            std::cout << f2;

            std::cout << " = " << c << std::endl;
        }
    }

};

class SunModel {
public:
    SunConfig config;
    bool check_density;
    BasicSeedFinder bsf;
    ZombieTypeList ztl;

    SunModel(const SunConfig& config): config(config) {
        ztl.set_scene(config.scene);
        check_density = false;
        for (int i=0;i<config.coef.size();i++)
            if ((config.factor1[i]==20 || config.factor2[i]==20) && config.coef[i]!=0){
                check_density = true;
                break;
            }
    }

    double sun_model(uint32_t has) {
        has |= (1 << 19);
        if (check_density) {
            uint32_t w = bsf.weight_has(has);
            if (w < config.density_threshold) 
                has |= (1 << 20);
        }
        double ans = 0;
        for (int i = 0; i <config.coef.size(); i++){
            int f1 = config.factor1[i];
            int f2 = config.factor2[i];
            double c = config.coef[i];
            int v1 = bsf.check_has(has, (1 << f1))? 1:config.neg_value;
            int v2 = bsf.check_has(has, (1 << f2))? 1:config.neg_value;
            ans += v1 * v2 * c;
        }
        return ans;
    }

    double simu(uint32_t rng_seed) {
        double sun = config.sun_start;
        if (config.sun_cap && sun > 9990) sun = 9990;
        double ans = sun;
        for (uint32_t i = 0, s = rng_seed; i < config.level_num; i++, s += SEED_STEP) {
            sun += sun_model(ztl.get_list(s));
            if (config.sun_cap && sun > 9990) sun = 9990;
            if (sun < ans) ans = sun;
        }
        return ans;
    }

    void mode2() {
        freopen("output.txt", "w", stdout);
        double ans = config.sun_start;
        uint32_t real_seed = config.mode2_seed;
        uint32_t rng_seed = bsf.get_rng_seed(real_seed, config.user_id, config.mode_id, config.level_start);
        for (uint32_t i = 0, s = rng_seed; i < config.level_num; i++, s += SEED_STEP) {
            uint32_t has = ztl.get_list(s);
            double dlt = sun_model(has);
            ans += dlt;
            if (config.sun_cap && ans > 9990) ans = 9990;
            printf("%.0lf\n",ans);  //作图只保留这句输出
        }
    }

};

class rng {
private:
    std::mt19937 gen;
public:
    rng() : gen(std::random_device{}()) {}
    uint32_t randseed(){
        std::uniform_int_distribution<uint32_t> dis(0, UINT32_MAX);
        return dis(gen);
    }
};

struct RngSeedResult {
    uint32_t rng_seed;
    double lowest_sun;
    RngSeedResult(rng& rng, SunModel* model){
        rng_seed = rng.randseed();
        lowest_sun = model->simu(rng_seed);
    }
    bool operator<(const RngSeedResult& other) const {
        return lowest_sun < other.lowest_sun;
    }
};

std::vector<RngSeedResult> mode1_work(SunConfig& config, bool parallel = false) {
    auto N = config.mode1_sim_num;
    std::vector<RngSeedResult> results;
    results.reserve(N);
    if (!parallel) {
        // 串行计算
        int ans = 0;
        SunModel model(config);
        rng r;
        for (uint32_t i = 0; i < N; ++i){
            results.emplace_back(r, &model);
            if (results.back().lowest_sun < config.sun_end)
                ans++;
            if (i % (N/100) == 0 && i > 0) {
                printf("(%5.2lf%%) %5.5f%%\n", 100.0 * i / N, 100.0 * ans / i);
                fflush(stdout);
            }
        }
        printf("%5.5f%% = %d / %d\n", 100.0 * ans / N, ans, N);
        fflush(stdout);
    } else {
        // 并行计算
        int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;
        int per_thread = N / num_threads;
        int remain = N % num_threads;

        std::mutex results_mutex;
        std::atomic<int> ans(0);
        std::atomic<int> finished(0);
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            int this_N = per_thread + (i < remain ? 1 : 0);
            threads.emplace_back([&, this_N]() {
                std::vector<RngSeedResult> local_results;
                local_results.reserve(this_N);
                SunModel model(config);
                rng r;
                for (int j = 0; j < this_N; ++j) {
                    local_results.emplace_back(r, &model);
                    if (local_results.back().lowest_sun < config.sun_end)
                        ans++;
                    finished++; 
                }
                if (!local_results.empty()) {
                    std::lock_guard<std::mutex> lock(results_mutex);
                    results.insert(results.end(), local_results.begin(), local_results.end());
                }
            });
        }

        // 进度展示
        while (finished < N) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            int now = finished.load();
            if (now == 0)   continue;
            printf("(%5.2lf%%) %5.5f%%\n", 100.0 * now / N, 100.0 * ans.load() / now);
            fflush(stdout);
        }

        for (auto& t : threads) t.join();
        printf("%5.5f%% = %d / %d\n", 100.0 * ans.load() / N, ans.load(), N);
        fflush(stdout);
    }
    
    if (N > config.mode1_output_num) {
        std::nth_element(results.begin(), results.begin() + config.mode1_output_num, results.end());
        results.erase(results.begin() + config.mode1_output_num, results.end());
    }
    std::sort(results.begin(), results.end());

    return results;
}

void work(SunConfig& config) {
    if (config.mode == 1) {
        BasicSeedFinder bsf;
        auto results = mode1_work(config, 1);
        printf("Top %d results:\n", config.mode1_output_num);
        for (int i = 0; i < config.mode1_output_num && i < results.size(); ++i) {
            auto real_seed = bsf.get_real_seed(results[i].rng_seed, config.user_id, config.mode_id, config.level_start);
            printf("%08X, %.0f\n", real_seed, results[i].lowest_sun);
        }  
    }
    else if (config.mode == 2) {
        SunModel model(config);
        model.mode2();
    }
}
