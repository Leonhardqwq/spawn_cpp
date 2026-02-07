#include "../../inc/quick_seed_finder.h"
#include <corecrt.h>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

class SpZombieRowCount:public ZombieNumList{
private:
    int row_len;
    Zombie sp_zombie;

    std::vector<double> dancer_wp,norm_wp;
    std::vector<int> last_pick,second_last_pick;
    int double_weight_select(std::vector<double> weights){
        std::random_device rd;
        std::mt19937 gen(rd());
        // std::uniform_real_distribution<> dis(0, std::accumulate(weights.begin(), weights.end(), 0));
        std::uniform_real_distribution<> dis(0, std::accumulate(weights.begin(), weights.end(), 0.0));

        double randomValue = dis(gen);
        double cumulativeWeight = 0;
        for (size_t i = 0; i < weights.size(); ++i) {
            cumulativeWeight += weights[i];
            if (randomValue <= cumulativeWeight) 
                return i;
        }
        return -1;
    }
public:
    std::vector<int> sp_num;

    SpZombieRowCount(uint32_t mask,int r_l,Zombie sp_z)
    :ZombieNumList(mask),row_len(r_l),sp_zombie(sp_z){
        dancer_wp.clear();
        norm_wp.clear();
        for(int i =0;i<row_len;i++){
            norm_wp.push_back(double(1.0/row_len));
            if(i==0 or i==row_len-1)
                dancer_wp.push_back(0.0);
            else
                dancer_wp.push_back(double(1.0/(row_len-2)));
            last_pick.push_back(0);
            second_last_pick.push_back(0);
        }
    }

    void init(){
        num_clear();
        sp_num.clear();
        for(int i =0;i<row_len;i++){
            sp_num.push_back(0);
            last_pick.push_back(0);
            second_last_pick.push_back(0);
        }

    }

    void update(Zombie z_selected){
        std::vector<double> smooth_weight;
        std::vector<double> wp= (z_selected==AWW_8)?dancer_wp:norm_wp;

        for(int i = 0;i<row_len;i++){
            double
                p_last=(6.0*last_pick[i]*wp[i]+6.0*wp[i]-3.0 )/4,
                p_second_last=(1.0*second_last_pick[i]*wp[i]+wp[i]-1)/4,
                tmp = p_last+p_second_last;
            if (tmp>100)   tmp = 100;
            else if(tmp<0.01)  tmp=0.01;

            smooth_weight.push_back(tmp*wp[i]);
        }
        auto row = double_weight_select(smooth_weight);
        if(row==-1) printf("ERROR");

        for(int i = 0;i<row_len;i++){
            if(i==row){
                second_last_pick[i] = last_pick[i];
                last_pick[i] = 0;
            }
            else{
                last_pick[i]++;
                second_last_pick[i]++;
            }
        }

        if(z_selected == sp_zombie){
            sp_num[row] ++;
        }
    }
    void calc(){
        init();

        for(int wave=1;wave<=20;wave++)
        if(wave==10 || wave==20){
            num[0][0]+=9,num[wave][0]+=9;
            for(int j=0;j<9;j++){
                update(APJ_0);
            }
            for(int j=0;j<50-9;){
                auto idx = weight_select(weight_flag_list);
                if(idx==-1)continue;
                num[wave][idx]++;num[0][idx]++;j++;

                update(Zombie(type_list[idx]));
            }
        }
        else{
            for(int j=0;j<50;){
                auto idx = weight_select(weight_list);
                if(idx==-1) continue;
                if(type_list[idx]==ABJ_20 || (type_list[idx]==AHY_32 && num[0][idx]>=50))   continue;
                num[wave][idx]++;num[0][idx]++;j++;

                update(Zombie(type_list[idx]));
            }
        }
    }
    void show_num(){
        for(int i = 0 ;i<row_len;i++){
            printf("%d ",sp_num[i]);
        }
        /*
        for(int i = 0;i<type_list.size();i++){
            if(type_list[i]==uint32_t(sp_zombie)){
                printf("%d\n",num[0][i]);
                break;
            }
        }        
        */
        printf("\n");

    }
};

Scene my_scene = NIGHT;

uint32_t THREAD_TEST_NUM=2500;

std::ofstream file("side_output.csv");

void output_csv(std::string name,std::vector<uint32_t> vec){
    file << name;file << ",";  

    for (int j = 0; j < vec.size(); ++j) {
        file << vec[j];
        if (j < vec.size() - 1)
            file << ",";  
    }
    file<<'\n';        
}

std::vector<uint32_t> get_total_num(Zombie sp,int row_len,uint32_t start){
    std::vector<uint32_t> total_num;

    for(int i = 0;i<row_len;i++)
        total_num.push_back(0);
    for(uint32_t i =0;i<THREAD_TEST_NUM;i++){
        ZombieTypeList ztl(my_scene);
        auto mask = ztl.get_list((start+i)*SEED_STEP);
        SpZombieRowCount szrc(mask,row_len,sp);
        szrc.calc();
        for(int j = 0;j<row_len;j++)
            total_num[j]+=szrc.sp_num[j];
        // szrc.show_num();
    }
    return total_num;
}

void multi_total_num(Zombie type,int row_len){
    std::vector<uint32_t> total_list;
    for(int i = 0;i<row_len;i++)
        total_list.push_back(0);

    std::vector<std::thread> pool;
    size_t block = THREAD_TEST_NUM;
    for (size_t k=0;k<MAX_THREAD;k++){
        std::thread t([=,&total_list]{
            std::vector<uint32_t> tmp =  get_total_num(type,row_len,k*block);

            mtx.lock();
            for(int i = 0;i<row_len;i++)
                total_list[i]+=tmp[i] ;

            mtx.unlock();
        });
        pool.push_back(std::move(t));
    }
    for (auto &t: pool) t.join();

    output_csv("总数", total_list);

    file.close();
}

int main(){
    multi_total_num(AKG_17,6);
    return 0;
}