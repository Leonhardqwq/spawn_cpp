#include "../../../inc/seed_finder.h"
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

Scene my_scene = NIGHT;
std::string file_name = "../../../data/NE.bin";
//    24,25,旗帜僵博


double cost_model(uint32_t has){
    BasicSeedFinder tmp;	
    double ans = -31.50875;
    ans += -38.28125
    *  (tmp.check_has(has,AHY_32)? 1:-1);
    ans += -180.46875
    *  (tmp.check_has(has,AGL_7)? 1:-1);
    ans += -252.34375
    *  (tmp.check_has(has,AQQ_16)? 1:-1);
    ans += -38.28125
    *  (tmp.check_has(has,AWW_8)? 1:-1);
    ans += 61.71875
    *  (tmp.check_has(has,ABY_23)? 1:-1);
    ans += -39.84375
    *  (tmp.check_has(has,ACG_3)? 1:-1);

    ans += 60.15625
    *  ((
        tmp.check_has(has,AGL_7)
    ==  tmp.check_has(has,AQQ_16)
    )? 1:-1);

    ans += -52.34375
    *  ((
        tmp.check_has(has,AGL_7)
    ==  tmp.check_has(has,ACG_3)
    )? 1:-1);


    bool density = false;

    auto w = tmp.weight_has(has);
    if(w<10500) density = true;
    if(w<11500 && tmp.check_has(has,(1<<AHY_32)|(1<<AGL_7)|(1<<AQQ_16)|(1<<ATL_22)|(1<<ACG_3)|(1<<ABY_23)))
        density = true;

    ans += -80.46875
    *  (density? 1:-1);

    return -ans;
}   
	
double cost_model_fix(uint32_t has,double fix){
    return cost_model(has)+fix;
}
bool cost_test(uint32_t rng_seed,size_t len){
    ZombieTypeList ztl(my_scene);
    BasicSeedFinder tmp;    
    bool pass = true;
    size_t show_len = 500000;
    double ans = 9990;
    for(size_t i=0;i<len;i++){
        double cost = cost_model_fix(ztl.get_list(rng_seed + SEED_STEP*i), -30);
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
    ZombieTypeList ztl(my_scene);
    BasicSeedFinder tmp;    
    bool pass = true;
    size_t show_len = 500000;
    double ans = 2000;
    for(size_t i=0;i<len;i++){
        double cost = cost_model_fix(ztl.get_list(rng_seed + SEED_STEP*i),-0);
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
    uint32_t fix_const = 1+13+1000*0x65;
    cost_print(
     0xF35211BD
    /*
        样本 1
        0x78C1F49F
        0x4F69A205 
        0xF35211BD
    */
    +fix_const,
    25
    );
}
void cal(){
    // freopen("output.txt", "w", stdout);
    int N=10000,cnt=0;
    for(int i=0;i<N;i++){
        std::random_device rd;  // 非确定性随机数生成器
        std::mt19937 gen(rd()); // 使用random_device初始化Mersenne Twister生成器
        std::uniform_int_distribution<uint32_t> dis(0, UINT32_MAX);
        uint32_t randomValue = dis(gen);
        if(!cost_test(randomValue,5000)){
            cnt++;
            // printf("#");
            // std::cout<<randomValue<<'\n';
            // cost_print(randomValue,10000);
            // cost_print_around(randomValue,10000);
        }
        // if(i%(N/10)==0)std::cout<<i<<'\n';
    }
    std::cout<<std::dec<<cnt<<'\n';
}
void show_broken_seed(){
    freopen("output.txt", "w", stdout);
    uint32_t fix_const = 1+13+1000*0x65;
    cost_print(
    3140431865,
    1000
    );
}


int main(){
    cal();
    // show();
    // show_broken_seed();

    return 0;
}