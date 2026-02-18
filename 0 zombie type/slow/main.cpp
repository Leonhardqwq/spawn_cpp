# include "../../inc/seed_finder.h"
// #include <corecrt.h>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>

void check_continue(std::string s){
    std::cout<<s;
    int tmp;std::cin>>tmp;
}
Scene my_scene = ROOF;
uint64_t find_len = 0xfffff;


void test_kmp(){
	final_results.clear();

    MultiKMPFinder finder(uint32_t(506150393),uint64_t(find_len),my_scene,{
     // 477211	,437257	,388121
        15258,	355416,	205630,
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
    MultiScoreSeedFinder finder(0,uint64_t(find_len),25,my_scene,
    {
        AHY_32,AQQ_16, ABC_12
    }, {
        1, 1, 1
    });
    finder.multi_thread_find_score();
    finder.show_results(final_results, 1, 13, my_scene, 1000, 50);
    finder.output_csv("output.csv", final_results);
}
void test_continue(){
	final_results.clear();
    MultiContinueSeedFinder finder(0,uint64_t(find_len),my_scene,
    {{ABC_12,AHY_32,AQQ_16}},
    {
    });
    finder.multi_thread_find_continue();
    finder.show_results(final_results, 1, 13, my_scene, 1000, 50);
    finder.output_csv("output.csv", final_results);
}
void show(){
    BasicSeedFinder tmp;
    tmp.show_list(4113615470,1,13,my_scene,1000,1,true);
    std::cout<<tmp.str_has(11583)<<'\n'<<tmp.get_rng_seed(0x1E29AF0C, 1, 13, 1011);
}

int main(){
    
    //test_kmp();
    //test_continue();
    test_score();  
    //show();
    // check_continue("\nEND");

    return 0;
}
