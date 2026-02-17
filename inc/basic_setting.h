#include <cstdio>
#include <iostream>

#include <cstdint>
#include <string>
#include <vector>
#include <queue>

#include <random>
#include <algorithm>


#include <fstream>

const uint32_t SEED_STEP = 0x65;


// 19 普僵  20 雪人  21  22  23 无内容
// 24 旗帜  25 僵博
enum Zombie {
    ALZ_2 = 0,    // 路障
    ACG_3,        // 撑杆
    ATT_4,        // 铁桶
    ADB_5,        // 读报


    ATM_6,        // 铁门
    AGL_7,        // 橄榄
    AWW_8,        // 舞王
    AQS_11,       // 潜水


    ABC_12,       // 冰车
    AHT_14,       // 海豚
    AXC_15,       // 小丑
    AQQ_16,       // 气球


    AKG_17,       // 矿工
    ATT_18,       // 跳跳
    ABJ_20,       // 蹦极
    AFT_21,       // 扶梯


    ATL_22,       // 投篮
    ABY_23,       // 白眼
    AHY_32,        // 红眼

    APJ_0,  


    AXR_19
};
enum Scene {
    DAY,
    NIGHT,
    POOL,
    ROOF
};
const std::string ZombieName[21] = {
    "路障",   "撑杆",  "铁桶",  "读报",  "铁门", 
    "橄榄",   "舞王",  "潜水",  "冰车",  "海豚", 
    "小丑", "气球", "矿工", "跳跳", "蹦极", 
    "扶梯", "投篮", "白眼", "红眼",
    "普僵", "雪人"
};

class ZombieTypeList{
private:
    // rng
    uint32_t buf[0x270];
    uint32_t idx;
    void rng_init(uint32_t seed){
        buf[0] = seed;
        for (int i = 1; i < 0x18e; ++i) 
            buf[i] = (buf[i - 1] ^ (buf[i - 1] >> 30)) * 0x6C078965 + i;
        idx = 0;
    }
    uint32_t rng_gen(int max){
        uint32_t tmp;

        tmp = (buf[idx] & 0x80000000) | (buf[idx + 1] & 0x7FFFFFFF);
        buf[idx] = (tmp >> 1) ^ buf[(idx + 0x18D) % 0x270];
        if (tmp & 1)    buf[idx] ^= 0x9908B0DF;
        
        buf[idx + 0x18D + 1] = (buf[idx + 0x18D] ^ (buf[idx + 0x18D] >> 30)) * 0x6C078965 + idx + 0x18D + 1;

        tmp = buf[idx];
        tmp ^= (tmp >> 11);
        tmp ^= ((tmp & 0xFF3A58AD) << 7);
        tmp ^= ((tmp & 0xFFFFDF8C) << 15);
        tmp ^= (tmp >> 18);
        idx = idx + 1;
        return (tmp & 0x7FFFFFFF) % max;
    }

    //scene allow
    const uint32_t allow_mask[4] = {
        0x307fd7f,
        0x307fc7f,
        0x307ffff,
        0x307ed3f
    };

    //gen_type->bit_num
    const uint32_t conv_id[33] = {
        31, 24, 
        0, 1, 2,
        3, 4, 5, 6, 
        31,31, 
        7,8, 
        31, 
        9,10, 11, 12, 13,
        31,
        14, 15, 16, 17, 
        31, 25,
        31, 31, 31, 31, 31, 31,
        18
    };
    Scene scene=DAY;

public:
    ZombieTypeList(){}
    ZombieTypeList(Scene initialScene) : scene(initialScene) {}
    void set_scene(Scene sc){   scene = sc; }
    uint32_t get_list(uint32_t rng_seed){
        if (rng_seed == 0) rng_seed = 0x1105;
        rng_init(rng_seed);
        uint32_t has = 0;
        if (rng_gen(5) == 0) 
            has |= (1 << ADB_5);
        else 
            has |= (1 << ALZ_2);

        int j = 0;
        while (j < 9) {
            uint32_t type = rng_gen(33);
            uint32_t nid = conv_id[type];
            if (has & (1 << nid)) continue;
            if (allow_mask[scene] & (1 << nid)) {
                has |= (1 << nid);
                j++;
            }
        }
        return has;
    }

    uint32_t get_list_level(uint32_t rng_seed,uint32_t level){
        if(level==0)    return (1<<ALZ_2)|(1<<ATT_4);

        if (rng_seed == 0) rng_seed = 0x1105;
        rng_init(rng_seed);
        uint32_t has = 0;
        if (rng_gen(5) == 0) 
            has |= (1 << ADB_5);
        else 
            has |= (1 << ALZ_2);

        // 种类上限=min(9,level+1)  j=max(0,8-level)
        // level 8 及以后 和常规种子完全相同
        int j = (level<8) ? (8-level) : 0;
        while (j < 9) {
            uint32_t type = rng_gen(33);
            uint32_t nid = conv_id[type];
            if (has & (1 << nid)) continue;

            // level<2 无 BY BC level<5 无 HY
            if(level<2 and (nid==ABY_23 or nid==ABC_12)) continue;
            if(level<5 and nid==AHY_32) continue;
            
            if (allow_mask[scene] & (1 << nid)) {
                has |= (1 << nid);
                j++;
            }
        }
        return has;
    }
};



const uint32_t ZombieWeight[21] = {
    1000,   2000,   3000,   1000,   3500, 
    2000,   1000,   2000,   2000,   1500, 
    1000,   2000,   1000,   1000,   1000, 
    1000,   1500,   1500,   1000,
    400,    //PJ
    1       //XR
};  //红(旗帜波) 6000

class ZombieNumList{
protected:
    uint32_t has=0;
    std::vector<uint32_t> weight_list;
    std::vector<uint32_t> weight_flag_list;
    int weight_select(std::vector<uint32_t> weights){
        std::random_device rd;
        std::mt19937 gen(rd());
        // std::uniform_real_distribution<> dis(0, std::accumulate(weights.begin(), weights.end(), 0));
        std::uniform_int_distribution<uint32_t> dis(1, std::accumulate(weights.begin(), weights.end(), static_cast<uint32_t>(0)));

        uint32_t randomValue = dis(gen);
        uint32_t cumulativeWeight = 0;
        for (size_t i = 0; i < weights.size(); ++i) {
            cumulativeWeight += weights[i];
            if (randomValue <= cumulativeWeight) 
                return i;
        }
        return -1;
    }
    void num_clear(){        for(int i=0;i<21;i++)for(int j=0;j<num[i].size();j++) num[i][j]=0;    }
public:
    std::vector<uint32_t> type_list;
    std::vector<uint16_t> num[21];  // wave idx
    ZombieNumList(uint32_t mask){   
        has = mask&(0x7ffff);   
        num_clear();

        // 普僵
        type_list.push_back(19);
        weight_list.push_back(ZombieWeight[19]);
        weight_flag_list.push_back(ZombieWeight[19]);
        for(int j=0;j<21;j++){  num[j].push_back(0);  }

        for(uint32_t i=0;i<19;i++){
            if( has & (1 << i) ){
                type_list.push_back(i);
                weight_list.push_back(ZombieWeight[i]);
                if(i==AHY_32)   weight_flag_list.push_back(6000);
                else            weight_flag_list.push_back(ZombieWeight[i]);
                for(int j=0;j<21;j++){  num[j].push_back(0);  }
            }
        }
        // 雪人
        type_list.push_back(20);
        weight_list.push_back(ZombieWeight[20]);
        weight_flag_list.push_back(ZombieWeight[20]);
        for(int j=0;j<21;j++){  num[j].push_back(0);  }

    }
    void gene(){
        num_clear();
        for(int wave=1;wave<=20;wave++)
        if(wave==10 || wave==20){
            num[0][0]+=8,num[wave][0]+=8;
            for(int j=0;j<50-9;){
                auto idx = weight_select(weight_flag_list);
                if(idx==-1)continue;
                num[wave][idx]++;num[0][idx]++;j++;
            }
        }
        else{
            for(int j=0;j<50;){
                auto idx = weight_select(weight_list);
                if(idx==-1) continue;
                if(type_list[idx]==ABJ_20 || (type_list[idx]==AHY_32 && num[0][idx]>=50))   continue;
                num[wave][idx]++;num[0][idx]++;j++;
            }
        }
    }
    void show(){
        printf("波次:");
        for(int wave=0;wave<=20;wave++)
            printf("%3d",wave);
        printf("\n");    

        for(int j=0;j<type_list.size();j++){
            if(j==0)    printf("普僵:");
            else if(j==type_list.size()-1)   printf("雪人:");
            else printf("%s:",ZombieName[type_list[j]].c_str());
                
            for(int wave=0;wave<=20;wave++){
                printf("%3d",num[wave][j]);
            }
            printf("\n");
        }
        printf("\n");
    }
};


struct SeedInfo {
    int metric = 0;       
    std::vector<uint32_t> seeds;        
    uint64_t seed_count = 0;                 

    SeedInfo(int m, const std::vector<uint32_t> &s, uint64_t cnt) : metric(m), seeds(s), seed_count(cnt) {}

    bool operator<(const SeedInfo &other) const{
        return (metric < other.metric);
    }
};
std::vector<SeedInfo> final_results;

class BasicSeedFinder{
public:
    uint32_t get_real_seed(uint32_t rng_seed, uint32_t id, uint32_t mode,uint32_t level){
        return uint32_t(rng_seed-id-mode-level*SEED_STEP);
    }
    uint32_t get_rng_seed(uint32_t real_seed,   uint32_t id,uint32_t mode,uint32_t level){
        return uint32_t(real_seed+id+mode+level*SEED_STEP);
    }
    bool check_has(uint32_t has,uint32_t mask){return (has & mask)==mask;}
    bool check_has(uint32_t has,Zombie zombie){return check_has(has,1<<zombie);}
    bool check_not_has(uint32_t has,uint32_t fmask){return (has & fmask) == 0;}
    bool check_not_has(uint32_t has,Zombie zombie){return check_not_has(has,1<<zombie);}

    uint32_t weight_has(uint32_t has){
        uint32_t ans=401;
        for(int i=0;i<=AHY_32;i++)
            if(check_has(has,(1<<i)) and i!=ABJ_20)
                ans+=ZombieWeight[i];
        return ans;
    }
    std::string str_has(uint32_t has){
        std::string info="普僵,";
        int cnt=1;
        for(int i=0;i<19;i++)
            if(has & (1<<i)){
                info += ZombieName[i]+",";
                cnt++;
            }
        return info+std::to_string(weight_has(has))+"w,"+std::to_string(cnt)+" types\n";
    }


    void show_list(uint32_t real_seed,uint32_t id,uint32_t mode,Scene sc,uint32_t level,uint32_t level_num){
        ZombieTypeList ztl(sc);
        printf("\n种子'0x%08X'在%d~%df出怪情况为:\n",
            real_seed , level * 2 + 1,(level + level_num) * 2);
        for (uint32_t i=0;i<level_num;i++){
            printf("%d~%df:",2 *( level + i)+1,2 * (level +1+ i));
            std::string info=str_has(ztl.get_list(get_rng_seed(
                real_seed,  id,  mode,  level+i)));
            printf("%s",info.data());
        }
    }
    void show_list(uint32_t seed,uint32_t id,uint32_t mode,Scene sc,uint32_t level,uint32_t level_num,bool use_rng){
        if(use_rng){
            uint32_t real_seed = get_real_seed( seed,  id,  mode,  level);
            show_list(real_seed, id, mode, sc, level, level_num);
        }
        else show_list(seed, id, mode, sc, level, level_num);
    }

    void show_list_level(uint32_t real_seed,uint32_t id,uint32_t mode,Scene sc,uint32_t level,uint32_t level_num){
        ZombieTypeList ztl(sc);
        printf("\n种子'0x%08X'在%d~%df出怪情况为:\n",
            real_seed , level * 2 + 1,(level + level_num) * 2);
            
        for (uint32_t i=0;i<level_num;i++){
            printf("%d~%df:",2 *( level + i)+1,2 * (level +1+ i));
            std::string info=str_has(ztl.get_list_level(
                get_rng_seed(real_seed,  id,  mode,  level+i)
                ,level+i
            ));
            printf("%s",info.data());
        }
    }
    void show_list_level(uint32_t seed,uint32_t id,uint32_t mode,Scene sc,uint32_t level,uint32_t level_num,bool use_rng){
        if(use_rng){
            uint32_t real_seed = get_real_seed( seed,  id,  mode,  level);
            show_list_level(real_seed, id, mode, sc, level, level_num);
        }
        else show_list_level(seed, id, mode, sc, level, level_num);
    }
     

    void show_results( std::vector<SeedInfo>& resu,uint32_t id,uint32_t mode,Scene sc,uint32_t level,uint32_t level_num){
        if(resu.empty()){
            return;
        }
        std::sort(resu.begin(), resu.end());
        auto tmp = resu.end()-1;
        std::cout<<tmp->metric<<std::endl;
        if(tmp->seeds.empty())  return;
        uint32_t seed = get_real_seed(tmp->seeds.at(0),  id,  mode,  level);
        show_list(seed, id,  mode,  sc,  level,  level_num);
    }
    void output_csv(std::string filename,std::vector<SeedInfo>& resu){
        std::ofstream file(filename);
        std::sort(resu.begin(), resu.end());
        auto iter = resu.end()-1;
        int upp = std::min(uint64_t(20),uint64_t(resu.size()));
        // 显示种子上限
        for(int i=0;i<upp;i++){
            file<<iter->metric;
            file<<",";
            auto vec = iter->seeds;
            for (int j = 0; j < vec.size(); ++j) {
                file << vec[j];
                if (j < vec.size() - 1)
                    file << ",";  
            }
            file<<'\n';    
            iter--;
        }
        file.close();
    }
};
