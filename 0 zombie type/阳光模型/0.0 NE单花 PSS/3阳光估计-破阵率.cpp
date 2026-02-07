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
    if(!tmp.check_has(has,1<<AHY_32) && !tmp.check_has(has,1<<AGL_7))
        return -700;
    double ans = -315.9977;
    if(tmp.check_has(has,1<<AHY_32))
        ans+=962.4081;
    if(tmp.check_has(has,1<<AGL_7))
        ans+=891.3600;
    if(tmp.check_has(has,1<<AWW_8))
        ans+=161.4457;
    if(tmp.check_has(has,1<<ABY_23))
        ans+=-34.0937;
    ans+=-1.0*tmp.weight_has(has)/10000*762.4807;
    return ans<-1000?-1000:ans;
}
bool cost_test(uint32_t rng_seed,size_t len){
    BasicSeedFinder tmp;    
    bool pass = true;
    size_t show_len = 500000;
    double ans = 9990;
    uint32_t seed_idx = rng_seed*inv;
    for(size_t i=0;i<len;i++){
        double cost = cost_model(array[seed_idx+i]) ;   
        // 增加消耗
        cost+=144.394 ;    //77.091，34.274 

        ans-=cost;
        if(ans>9990) ans=9990;
        //if(i<show_len)
            // printf("%3llu:%5.0lf%6.0lf\n",i,-cost,ans);
            // printf("%.0lf\n",ans);  //作图只保留这句输出
        // std::cout<<tmp.str_has(array[seed_idx+i])<<'\n';
        if(ans<-500 and pass){
            pass = false;
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
    double ans = 9990;
    uint32_t seed_idx = rng_seed*inv;
    for(size_t i=0;i<len;i++){
        double cost = cost_model(array[seed_idx+i]);
        ans-=cost;
        if(ans>9990) ans=9990;
        printf("%.0lf\n",ans);  //作图只保留这句输出
        // std::cout<<tmp.str_has(array[seed_idx+i])<<'\n';
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
    bool ans=
    cost_test(
    0xBB2DA563
    /*
        0x7B4B2E9E
        //hg    
        0x75263B42
        //h
        0x03C76D62
        //g

        3277850098
        165034440
        2783441874
        1239756174
        1500628337
        2032531589
        3110361619
        1127761184
        3308093741
        3377953408
        2783181294
        792406419
        594085677
        4171400175
        1388928425
        3726575558
        2828301411
        899900565
        3714960590
        384686339
        1221277174
        4187342350
        3919147145

    */

    +fix_const,
    10000000
    );
}
void cal(){
    freopen("output.txt", "w", stdout);
    load_data();
    int N=10000,cnt=0;
    for(int i=0;i<N;i++){
        std::random_device rd;  // 非确定性随机数生成器
        std::mt19937 gen(rd()); // 使用random_device初始化Mersenne Twister生成器
        std::uniform_int_distribution<uint32_t> dis(0, UINT32_MAX);
        uint32_t randomValue = dis(gen);

        if(!cost_test(randomValue,100)){
            cnt++;
            // std::cout<<randomValue<<'\n';
            // cost_print(randomValue,10000);
            // cost_print_around(randomValue,10000);

        }
        //if(i%(N/10)==0)std::cout<<i<<'\n';
    }
    std::cout<<std::dec<<cnt<<'\n';

    /*
    
    BasicSeedFinder tmp;
    uint32_t seed_idx = uint32_t((0x7b4b2e9e + 14 + 1000*0x65) * inv);
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
    // */

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
    // show_broken_seed();
    // print_han();
    //show();

    return 0;
}