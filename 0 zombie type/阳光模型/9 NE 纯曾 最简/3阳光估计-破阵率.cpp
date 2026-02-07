#include "../../../inc/quick_seed_finder.h"
#include <corecrt.h>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

Scene my_scene = NIGHT;
std::string file_name = "../../../data/NE.bin";
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

double cost_model(uint32_t has){
    BasicSeedFinder tmp;					
    double ans = -34.46625;

    ans += -181.71875
    *  (tmp.check_has(has,AHY_32)? 1:-1);
    ans += -252.03125
    *  (tmp.check_has(has,AQQ_16)? 1:-1);
    ans += -102.65625
    *  (tmp.check_has(has,AGL_7)? 1:-1);
    ans += -71.71875
    *  (tmp.check_has(has,AWW_8)? 1:-1);
    ans += 54.53125
    *  (tmp.check_has(has,ABY_23)? 1:-1);
    ans += -46.09375
    *  (tmp.check_has(has,ATL_22)? 1:-1);
    ans += -33.28125
    *  (tmp.check_has(has,ACG_3)? 1:-1);

    ans += -54.21875
    *  ((
        tmp.check_has(has,AHY_32)
    ==  tmp.check_has(has,AGL_7)
    )? 1:-1);

    ans += 40.46875
    *  ((
        tmp.check_has(has,AGL_7)
    ==  tmp.check_has(has,AQQ_16)
    )? 1:-1);

    bool density = false;

    auto w = tmp.weight_has(has);
    if(w<10500) density = true;
    if(w<11500 && tmp.check_has(has,(1<<AHY_32)|(1<<AGL_7)|(1<<AQQ_16)|(1<<ATL_22)|(1<<ACG_3)|(1<<ABY_23)))
        density = true;

    ans += -28.59375
    *  ((
        tmp.check_has(has,AHY_32)
    ==  density
    )? 1:-1);

    return -ans;
}   
	
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
        double cost = cost_model_fix(array[seed_idx+i],-105);
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
    double ans = 8000;
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
void cost_print_around(uint32_t rng_seed,size_t len){
    BasicSeedFinder tmp;    
    bool pass = true;
    size_t show_len = 500000;
    double ans = 9990;
    uint32_t seed_idx = rng_seed*inv;
    for(size_t i=0;i<len;i++){
        double cost = cost_model(array[seed_idx+i]);
        ans-=cost;
        if(ans>9990) ans=9990;
        // printf("%.0lf\n",ans);  //作图只保留这句输出
        // std::cout<<tmp.str_has(array[seed_idx+i])<<'\n';
        if(ans<-500 and pass){
            pass = false;
            seed_idx = seed_idx+i-50;
            break;
        }    
    }
    int HG_num = 0,H_num=0,G_num=0;

    for(size_t i=0;i<50;i++){
        if(
            tmp.check_has(array[seed_idx+i],(1<<AGL_7)) 
        &&  tmp.check_has(array[seed_idx+i],(1<<AHY_32))
        )   HG_num++;
        else if(
            tmp.check_has(array[seed_idx+i],(1<<AGL_7)) 
        &&  !tmp.check_has(array[seed_idx+i],(1<<AHY_32))
        )   G_num++;
        else if(
            !tmp.check_has(array[seed_idx+i],(1<<AGL_7)) 
        &&  tmp.check_has(array[seed_idx+i],(1<<AHY_32))
        )   H_num++;
        
        
    }

    std::cout << std::hex << seed_idx*0x65-1-13-1000*0x65<<' ' ;
    printf("红橄%d 红%d 橄%d\n",HG_num,H_num,G_num);
}
void show(){
    freopen("output.txt", "w", stdout);
    load_data();
    uint32_t fix_const = 1+13+1000*0x65;
    cost_print(
    0xF35211BD
    /*
        样本 1
        0x78C1F49F

    */
    +fix_const,
    1000
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
    load_data();
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