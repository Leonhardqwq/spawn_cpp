# include "../../inc/seed_finder.h"
#include <corecrt.h>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>


size_t qr64(std::string s){
    std::cout<<s;
    size_t x=0;char ch=getchar();
    while(ch<'0'||ch>'9'){ch=getchar();}
    while(ch>='0'&&ch<='9'){x=(x<<1)+(x<<3)+(ch^48);ch=getchar();}
    return x;
}
int qr(std::string s){
    std::cout<<s;
    bool f=0;int x=0;char ch=getchar();
    while(ch<'0'||ch>'9'){if(ch=='-')f=1;ch=getchar();}
    while(ch>='0'&&ch<='9'){x=(x<<1)+(x<<3)+(ch^48);ch=getchar();}
    return f?-x:x;
}


void check_continue(std::string s){
    std::cout<<s;
    int tmp;std::cin>>tmp;
}
Scene my_scene = ROOF;
uint64_t find_len = 0xffffffff;

void test_kmp(){
	final_results.clear();

    MultiKMPFinder finder(uint32_t(506150393),uint64_t(find_len),my_scene,{
     477211	,437257	,388121
    });

    finder.multi_thread_find_kmp();

    if(final_results.empty() || final_results[0].seeds.size()==0){
        printf("NOT FOUND\n");
        return;
    }
    printf("FOUND\n");
    finder.show_results(final_results, 1, 13, my_scene, 1000, 5);
    finder.output_csv("output.csv", final_results);
}
void test_score(){
	final_results.clear();
    MultiScoreSeedFinder finder(0,uint64_t(find_len),50,my_scene,
    {AHY_32,AXC_15, AQQ_16,AGL_7,AWW_8,ABC_12},
       {3,3,        1,1,1,1});
    finder.multi_thread_find_score();
    finder.show_results(final_results, 1, 13, my_scene, 1000, 50);
    finder.output_csv("output.csv", final_results);
}
void test_continue(){

    uint32_t start_s = qr64("start at[0,2^32):");
    find_len = qr64("num(0,2^32]:");
    my_scene = Scene(qr("scene(DE=0,NE=1,PE=2,RE=3):"));
    int n=qr("OR constraints num:");
    std::vector<std::vector<uint32_t>> true_c;
    printf("\nLZ0,CG1,TT2,DB3,\nTM4,GL5,WW6,QS7,\nBC8,HT9,XC10,QQ11,\nKG12,TT13,BJ14,FT15,\nTL16,BY17,HY18,\nQZ24,JB25\n");
    for(int i=0;i<n;i++){
        true_c.push_back({});
        int m=qr("  AND constraints num:");
        for(int j=0;j<m;j++){
            true_c[i].push_back(qr(""));
        }
    }    

	final_results.clear();
    MultiContinueSeedFinder finder(start_s,uint64_t(find_len),my_scene,true_c);
    finder.multi_thread_find_continue();
    // finder.show_results(final_results, 1, 13, my_scene, 1000, 50);
    finder.output_csv("output.csv", final_results);
}
void show(){
    BasicSeedFinder tmp;
    tmp.show_list(11583,1,13,my_scene,1000,1,true);
    std::cout<<tmp.str_has(11583)<<'\n'<<tmp.get_rng_seed(0x1E29AF0C, 1, 13, 1011);
}

int main(){
    
    //test_kk();
    //test_kmp();
    test_continue();
    //test_score();  
    //show();
    check_continue("\nEND");

    return 0;
}
