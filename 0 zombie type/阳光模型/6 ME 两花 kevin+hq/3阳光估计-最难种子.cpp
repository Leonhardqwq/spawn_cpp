#include "../../../inc/quick_seed_finder.h"
#include <atomic>
#include <cmath>
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

///*  Kevin       
double cost_model(uint32_t has){
    BasicSeedFinder tmp;					
    double ans = -1368.534612;
    if(tmp.check_has(has,AGL_7))
        ans+= 308.118443;    
    if(tmp.check_has(has,AQQ_16))
        ans+=823.299127;
    if(tmp.check_has(has,ABY_23))
        ans+= 287.110106;
    if(tmp.check_has(has,AHY_32))
        ans+= 1174.302342;
    
    ans-=46;    //瓜损
    return ans;
} // */


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
        double cost = cost_model_fix(array[seed_idx+i],-155);
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
    double ans = 9990;
    uint32_t seed_idx = rng_seed*inv;
    for(size_t i=0;i<len;i++){
        double cost = cost_model_fix(array[seed_idx+i],-0);
        ans-=cost;
        if(ans>9990) ans=9990;
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
    uint32_t(749846303)*uint32_t(0x65)

    ,1500
    //545
    );

}

void find_seed(){
    // freopen("output.txt", "w", stdout);
    struct Interval{
        double sum;
        uint32_t p_start,p_end;
        size_t cnt_start,cnt_end;
        Interval(double s,uint32_t st,uint32_t en,size_t cnt_st,size_t cnt_en)
        :sum(s),p_start(st),p_end(en),cnt_start(cnt_st),cnt_end(cnt_en){

        }
    };
    load_data();
    Interval ans(0,0,0,0,0);
    Interval res(-INFINITY,0,0,0,0);

    while(true){
        if(ans.sum<0){
            ans.sum = cost_model(array[uint32_t(ans.p_end)]);
            ans.p_start = ans.p_end;
            ans.cnt_start = ans.cnt_end;

            if(ans.cnt_start>array_size)
                break;
        }
        else {
            ans.sum += cost_model(array[uint32_t(ans.p_end)]);
        }
        if (res.sum<ans.sum){
            res.sum = ans.sum;
            res.p_start = ans.p_start;
            res.p_end = ans.p_end;
        }
        ans.p_end++;
        ans.cnt_end++;

        if(ans.cnt_end%(array_size/100)==0) std::cout<< ans.cnt_end/(array_size/100) <<'\n';
    }
    printf("%lf,%u,%u",res.sum,res.p_start,res.p_end);
    int tmp;
    std::cin>>tmp;
}

void find_seed_test(size_t M){
    // freopen("output.txt", "w", stdout);
    struct Interval{
        double sum;
        uint32_t p_start,p_end;
        size_t cnt_start,cnt_end;
        Interval(double s,uint32_t st,uint32_t en,size_t cnt_st,size_t cnt_en)
        :sum(s),p_start(st),p_end(en),cnt_start(cnt_st),cnt_end(cnt_en){

        }
    };
    load_data();
    Interval ans(0,0,0,0,0);
    Interval res(-INFINITY,0,0,0,0);

    while(true){
        if(ans.sum<0){
            ans.sum = cost_model(array[ans.p_end]);
            ans.p_start = ans.p_end;
            ans.cnt_start = ans.cnt_end;

            if(ans.cnt_start>M)
                break;
        }
        else {
            ans.sum += cost_model(array[ans.p_end]);
        }
        if (res.sum < ans.sum){
            res.sum = ans.sum;
            res.p_start = ans.p_start;
            res.p_end = ans.p_end;
        }
        ans.p_end++;
        ans.cnt_end++;

        if(ans.p_end>=M)    ans.p_end-=M;

        if(ans.cnt_end%(M/100)==0) std::cout<< ans.cnt_end/(M/100) <<'\n';
    }
    printf("%lf,%u,%u",res.sum,res.p_start,res.p_end);
    int tmp;
    std::cin>>tmp;
}

int main(){
    // find_seed();
    // find_seed_test(10000);
    // show();

    std::cout<< 2720032571-1-13-1000*0x65;

    return 0;
}