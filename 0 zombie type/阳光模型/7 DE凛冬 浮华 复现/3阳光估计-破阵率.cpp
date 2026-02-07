#include "../../../inc/quick_seed_finder.h"
#include <corecrt.h>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

Scene my_scene = DAY;
std::string file_name = "../../../data/DE.bin";
uint64_t find_len = 0xffffffff;
//    24,25,旗帜僵博

void load_data(){
    std::ifstream file(file_name, std::ios::binary);
    if (file.is_open()) {
        file.read(reinterpret_cast<char*>(array), array_size * sizeof(uint32_t));
        if (!file) std::cerr << "Error reading file. Only " << file.gcount() << " could be read." << std::endl;
        file.close();
    } else std::cerr << "Unable to open file for reading." << std::endl;
    // printf("LOAD END\n");
}

/*  new
double cost_model(uint32_t has){
    BasicSeedFinder tmp;					
    double ans = 855.567438;

    ans += -109.689344
    *  (tmp.check_has(has,ACG_3)? 1:-1);
    ans += 224.965079
    *  (tmp.check_has(has,ATT_4)? 1:-1);
    ans += -103.366077
    *  (tmp.check_has(has,AWW_8)? 1:-1);
    ans += -268.922286
    *  (tmp.check_has(has,ABC_12)? 1:-1);
    ans += -436.231242
    *  (tmp.check_has(has,AQQ_16)? 1:-1);
    ans += -270.558115
    *  (tmp.check_has(has,AKG_17)? 1:-1);
    ans += -111.426991
    *  (tmp.check_has(has,ABJ_20)? 1:-1);
    ans += 271.433532
    *  (tmp.check_has(has,AHY_32)? 1:-1);

    ans += -621.966916
    *  ((
        tmp.check_has(has,AHY_32)
    ==  tmp.check_has(has,AGL_7)
    )? 1:-1);

    ans += -105.307749
    *  ((
        tmp.check_has(has,AHY_32)
    ==  tmp.check_has(has,ATT_4)
    )? 1:-1);

    ans += 133.53886
    *  ((
        tmp.check_has(has,AGL_7)
    ==  tmp.check_has(has,AQQ_16)
    )? 1:-1);

    ans += -115.531934
    *  ((
        tmp.check_has(has,AGL_7)
    ==  tmp.check_has(has,ABC_12)
    )? 1:-1);

    return -ans;
} //*/
/*  single
double cost_model(uint32_t has){
    BasicSeedFinder tmp;
    double ans = 3453.642045;
    if(tmp.check_has(has,1<<AHY_32))
        ans+=-617.121144;
    if(tmp.check_has(has,1<<AGL_7))
        ans+=-1274.580593;    
    if(tmp.check_has(has,1<<ABC_12))
        ans+=-736.085032;
    if(tmp.check_has(has,1<<ACG_3))
        ans+=-181.503655;
    if(tmp.check_has(has,1<<AQQ_16))
        ans+=-571.754394;
    if(tmp.check_has(has,1<<AKG_17))
        ans+=-622.508958;
    if(tmp.check_has(has,1<<ABY_23))
        ans+=-69.233245;
    return -ans;
}   //*/

///*  cumulative
double cost_model(uint32_t has){
    BasicSeedFinder tmp;
    double ans = 2879.880098;
    if(tmp.check_has(has,1<<AHY_32))
        ans+=-251.01554;
    if(tmp.check_has(has,1<<AGL_7))
        ans+=-1226.788907;    
    if(tmp.check_has(has,1<<ABC_12))
        ans+=-484.753567;
    if(tmp.check_has(has,1<<ACG_3))
        ans+=-537.51147;
    if(tmp.check_has(has,1<<AQQ_16))
        ans+=-624.794681;
    if(tmp.check_has(has,1<<AKG_17))
        ans+=-278.707121;
    if(tmp.check_has(has,1<<ABY_23))
        ans+=312.502084;
    return -ans;
}   //*/

double cost_model_fix(uint32_t has,double fix){
    return cost_model(has)+fix;
}
bool cost_test(uint32_t rng_seed,size_t len){
    BasicSeedFinder tmp;    
    bool pass = true;
    size_t show_len = 500000;
    double ans = 9990;
    uint32_t seed_idx = rng_seed*inv;
    for(size_t i=0;i<len;i++){
        double cost = cost_model_fix(array[seed_idx+i],-0);
        ans-=cost;
        if(ans>9990) ans=9990;
        if(ans<-500 and pass){
            pass = false;
            break;
            // uint32_t tmp_seed = i<50?seed_idx:seed_idx+i-50;
            // std::cout<<tmp_seed<<'\n';
        }    
    }
    return pass;
}
void cost_print(uint32_t rng_seed,size_t len){
    BasicSeedFinder tmp;    
    bool pass = true;
    size_t show_len = 500000;
    double ans = 0;
    uint32_t seed_idx = rng_seed*inv;
    for(size_t i=0;i<len;i++){
        double cost = cost_model_fix(array[seed_idx+i],-0);
        ans-=cost;
        // if(ans>9990) ans=9990;
        printf("%.0lf\n",ans);  //作图只保留这句输出
        if(ans<-500 and pass){
            pass = false;
        }    
    }
}

void show(){
    freopen("output.txt", "w", stdout);
    load_data();
    uint32_t fix_const = 1+13+1000*0x65;
    cost_print(
    0x2EEFD113
    /*
        样本 1
        0x78C1F49F
        样本 2
        0x1D14D48D

        花崎
        0x2EEFD113
    */
    +fix_const,
    5000
    );
}
void cal(){
    // freopen("output.txt", "w", stdout);
    load_data();
    int N=10000,cnt=0;
    for(int i=0;i<N;i++){
        std::random_device rd;  // 非确定性随机数生成器
        std::mt19937 gen(rd()); // 使用random_device初始化Mersenne Twister生成器
        std::uniform_int_distribution<uint32_t> dis(0, UINT32_MAX);
        uint32_t randomValue = dis(gen);
        if(!cost_test(randomValue,5000)){
            cnt++;
        }
        // if(i%(N/10)==0)std::cout<<i<<'\n';
    }
    std::cout<<std::dec<<cnt<<'\n';
}

int main(){
    cal();
    // show();

    return 0;
}