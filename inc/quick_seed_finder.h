#pragma once
#include "seed_finder.h"
#include <corecrt.h>
#include <cstdint>
#include <cstdio>
#include <iostream>

const uint64_t array_size = 0x100000000; // 2^32
#ifdef PVZ_QUICK_EXTERNAL_ARRAY
uint32_t* array = nullptr;
#else
uint32_t* array = new uint32_t[array_size];
#endif


class QuickScoreSeedFinder : public BasicSeedFinder {
protected:
    const uint64_t MAX_COUNT = 100;
    const int LOWEST_STORE = -1;
    ZombieTypeList _zlt;
    std::vector<uint32_t> constraints;
    int weight[26]={};

    Scene scene;
    uint32_t cur_seed;
    
    uint64_t num_total;
    uint64_t num_cur;

    uint64_t num_level;
    bool progress;

public:
    std::vector<SeedInfo> results;

    QuickScoreSeedFinder(uint32_t start_seed,uint64_t start_offset,uint64_t num,uint64_t num_l,
        Scene sc,std::vector<uint32_t> true_constraints,std::vector<int> true_weight,
        bool show_progress = true){
        scene = sc;
        constraints = true_constraints;
        for(int i=0;i<constraints.size();i++)
            weight[constraints[i]] = true_weight[i];
        cur_seed = uint32_t(start_seed+start_offset*SEED_STEP);
        num_total = num;
        num_cur = 0;
        num_level = num_l;
        progress = show_progress;
        results.clear();
        _zlt.~ZombieTypeList(); 
        new(&_zlt) ZombieTypeList(scene); 
    }
    int get_score(uint32_t has){
    ///*  normal
        int ans_score = 0;
        for(int i=0;i<constraints.size();i++){
            auto z = constraints[i];
            if(has&(1<<z))
                ans_score+=weight[z];
        }
        return ans_score;        
    //*/

    /*  NE 三花纯瓜
        int ans_score = 1;
        if(!check_has(has, AHY_32))  return 0;
        if(!check_has(has, AKG_17))  return 0;
        if(!check_has(has, AGL_7))   return 0;
        if(!check_has(has, AQQ_16))  return 0;

        if(check_has(has, ABY_23))  ans_score+=1;

        if(weight_has(has)<10000)   ans_score+=4;
        else if(weight_has(has)<11000)   ans_score+=3;
        else if(weight_has(has)<12000)   ans_score+=2;
        else if(weight_has(has)<13000)   ans_score+=1;

        return ans_score;   
    //*/

    /* DE 半花 难度
        int ans_score = 1;
        if(!check_has(has, AHY_32))  return 0;
        if(!check_has(has, AGL_7))   return 0;
        if(!check_has(has, ABC_12))  return 0;

        if(check_has(has, AQQ_16))  ans_score+=1;
        if(check_has(has, ACG_3))  ans_score+=1;
        
        if(weight_has(has)<10000)   ans_score+=4;
        else if(weight_has(has)<11000)   ans_score+=3;
        else if(weight_has(has)<12000)   ans_score+=2;
        else if(weight_has(has)<13000)   ans_score+=1;

        return ans_score;  
    //*/

    /* DE 半花 恢复
        int ans_score = 1;
        if(check_has(has, AHY_32))  return 0;
        
        if(check_has(has, AGL_7)){
            ans_score+=1;
        
            if(weight_has(has)<10000)   ans_score+=4;
            else if(weight_has(has)<11000)   ans_score+=3;
            else if(weight_has(has)<12000)   ans_score+=2;
            else if(weight_has(has)<13000)   ans_score+=1;

            if(check_has(has, ACG_3))  ans_score+=1;
        }   
        if(check_has(has, ABC_12))  ans_score+=1;
        if(check_has(has, AQQ_16))  ans_score+=1;

        return ans_score;  
    //*/

    }
    void find(){
        if(num_total==0 || num_level==0) return;
        std::queue<int> q_score;
        int sum_score = 0;
        uint32_t idx=uint32_t(cur_seed*inv);
        for(uint64_t i=0;i<num_level;i++){
            int tmp_score = get_score(array[idx++]);
            q_score.push(tmp_score);
            sum_score+=tmp_score;
        }
        while(num_cur<num_total){
            bool found=false;
            if(sum_score>LOWEST_STORE){
                for (auto &info : results) 
                if (info.metric == sum_score) {
                    found = true;
                    ++info.seed_count;
                    if (int(info.seed_count) <= MAX_COUNT)
                        info.seeds.push_back(cur_seed);
                    break;
                }
                if (!found) 
                    results.push_back(SeedInfo(sum_score, {cur_seed}, 1));
            }
            ++num_cur;
            if(progress && num_total>=100 && (num_cur)%(num_total/100)==0){
                printf("%.1lf%% %d %lld %u\n",(100.0*num_cur/num_total),sum_score,num_cur,cur_seed);
                fflush(stdout);
            }
            if(num_cur==num_total) break;

            int tmp_score = get_score(array[idx++]);
            int minus_score = q_score.front();
            q_score.push(tmp_score);q_score.pop();
            sum_score+=tmp_score-minus_score;
            cur_seed+=SEED_STEP;
        }
    }
    void show_tmp(){
        for(auto tmp:results){
            std::cout<<tmp.metric<<std::endl;
        }
        printf("end");
    }
};
class MultiQuickScoreSeedFinder: public BasicSeedFinder{
private:
    std::vector<std::thread> pool;

    uint32_t start_seed;
    uint64_t num_total;
    uint64_t num_level;
    Scene scene;
    std::vector<uint32_t> constraints;
    std::vector<int> weights;
    bool progress;

public:
    MultiQuickScoreSeedFinder(uint32_t start_s,uint64_t num_t,uint64_t num_l,Scene sc,
        std::vector<uint32_t> true_cons,std::vector<int> true_w,
        bool show_progress = true)
    :   start_seed(start_s),num_total(num_t),num_level(num_l),scene(sc),
        constraints(true_cons),weights(true_w),progress(show_progress) {pool.clear();}
    void single_thread_find_score(uint64_t st,uint64_t num_assign){
        QuickScoreSeedFinder ssf(this->start_seed,st,num_assign,this->num_level,this->scene,
            this->constraints,this->weights,this->progress);
        ssf.find();
        mtx.lock();
        for (auto now_info : ssf.results) {
            bool found = false;
            for(auto &info:final_results)
            if (info.metric == now_info.metric) {
                found = true;
                for(auto s:now_info.seeds)
                    info.seeds.push_back(s);
                std::sort(info.seeds.begin(), info.seeds.end());
                auto last = std::unique(info.seeds.begin(), info.seeds.end());
                info.seeds.erase(last, info.seeds.end());

                info.seed_count+=now_info.seed_count;
                break;
            }
            if (!found)     final_results.push_back(now_info);
        }
        mtx.unlock();
    }
    void multi_thread_find_score(){
        if(num_total==0) return;
        pool.clear();
        uint64_t workers=std::min<uint64_t>(std::max(1u,MAX_THREAD),num_total);
        uint64_t block=num_total/workers, remain=num_total%workers, st=0;
        for(uint64_t i=0;i<workers;i++){
            uint64_t num_assign=block+(i<remain);
            std::thread t([this,st,num_assign]{
                single_thread_find_score(st,num_assign);
            });
            pool.push_back(std::move(t));
            st+=num_assign;
        }
        for (auto &t: pool) t.join();
    }
};


class QuickContinueSeedFinder : public BasicSeedFinder {
protected:
    const uint64_t MAX_COUNT = 500;
    const int LOWEST_STORE = 0;
    ZombieTypeList _zlt;
    std::vector<uint32_t> masks;
    std::vector<uint32_t> fmasks;//newnew

    Scene scene;
    uint32_t cur_seed;
    uint64_t num_total;
    uint64_t num_cur;
    bool progress;
public:
    std::vector<SeedInfo> results;

    QuickContinueSeedFinder(uint32_t start_seed,uint64_t start_offset,uint64_t num,
        Scene sc,std::vector<std::vector<uint32_t>> true_constraints
        ,std::vector<std::vector<uint32_t>> false_constraints
        ,bool show_progress = true
        ){
        scene = sc;

        masks.clear();
        for(auto cons:true_constraints){
            uint32_t tmp_mask=0;
            for(auto z:cons)
                tmp_mask |= (1<<z);
            masks.push_back(tmp_mask);
        }
        cur_seed = uint32_t(start_seed+start_offset*SEED_STEP);
        num_total = num;
        num_cur = 0;
        progress = show_progress;
        results.clear();
        _zlt.~ZombieTypeList(); 
        new(&_zlt) ZombieTypeList(scene); 

        fmasks.clear();
        for(auto cons:false_constraints){
            uint32_t tmp_mask=0;
            for(auto z:cons)
                tmp_mask |= (1<<z);
            fmasks.push_back(tmp_mask);
        }

    }
    bool check_true(uint32_t has){
        for (auto mask:masks)
            if(check_has( has,  mask))
                return true;
        return masks.empty();
    }
    bool check_false(uint32_t has){
        for (auto fmask:fmasks)
            if( (has & fmask) == 0)
                return true;
        return fmasks.empty();
    }
    bool check_constraints(uint32_t has){
        return check_true(has)&&check_false(has);
    }
    void find(){
        uint32_t idx=uint32_t(cur_seed*inv);
        while(num_cur<num_total){
            int len = 0;
            while(num_cur<num_total){
                auto has = array[idx++];

                ///*  正常版本
                    if(!check_constraints(has))    break;
                //*/

                /*  PE 纯绿
                    if(check_has(has,AHY_32)){
                        if(check_has(has,ABY_23)||check_has(has,AGL_7)||check_has(has,ABC_12))
                            break;
                        if(len>=10)
                            break;
                    }
                    if(check_has(has,ABY_23)){
                        if(len>=8)  break;

                        if(check_has(has,AGL_7)||check_has(has,ABC_12))
                            if(len>=6)  break;
                    }
                    if(check_has(has,ABC_12) ){
                        if(len>=14) break;
                    }
                    if(check_has(has,AGL_7)&&check_has(has,ABC_12) )
                        if(len>=10) break;

                //*/

                /*  NE 纯绿
                    if(check_has(has,ABY_23)){
                        if(len>=8)  break;
                        if(check_has(has,AGL_7))
                            if(len>=6)  break;
                    }
                    if(check_has(has,AHY_32)){
                        if(check_has(has,ABY_23)||check_has(has,AGL_7))
                            break;
                        if(len>=10)
                            break;
                    }
                    
                //*/

                cur_seed+=SEED_STEP;
                num_cur++;
                len++;
            }

            bool found=false;
            if(len>=LOWEST_STORE){
                for (auto &info : results) 
                if (info.metric == len) {
                    found = true;
                    ++info.seed_count;
                    if (int(info.seed_count) <= MAX_COUNT)
                        info.seeds.push_back(uint32_t(cur_seed-SEED_STEP*len));
                    break;
                }
                if (!found) 
                    results.push_back(SeedInfo(len, {uint32_t(cur_seed-SEED_STEP*len)}, 1));
            }

            if(progress && num_total>=100 && (num_cur)%(num_total/100)==0){
                printf("%.1lf%% %d %lld %u\n",(100.0*num_cur/num_total),len,num_cur,uint32_t(cur_seed-SEED_STEP*len));
                fflush(stdout);
            }

            num_cur++;
            cur_seed+=SEED_STEP;
        }
    }

    void show_tmp(){
        for(auto tmp:results){
            std::cout<<tmp.metric<<std::endl;
        }
        printf("end");
    }

};
class MultiQuickContinueSeedFinder: public BasicSeedFinder{
private:
    std::vector<std::thread> pool;

    const uint32_t interval = 50;
    uint32_t start_seed;
    uint64_t num_total;
    Scene scene;
    std::vector<std::vector<uint32_t>> true_constraints;
    std::vector<std::vector<uint32_t>> false_constraints;//newnew
    bool progress;

public:
    MultiQuickContinueSeedFinder(uint32_t start_s,uint64_t num_t,Scene sc,std::vector<std::vector<uint32_t>> true_cons
    ,std::vector<std::vector<uint32_t>> false_cons //newnew
    ,bool show_progress = true
    )
    :start_seed(start_s),num_total(num_t),scene(sc),true_constraints(true_cons) 
    ,false_constraints(false_cons) //newnew
    ,progress(show_progress)
    {pool.clear();}
    void single_thread_find_continue(uint64_t st,uint64_t num_assign){
        QuickContinueSeedFinder csf(this->start_seed,st,num_assign,this->scene,this->true_constraints
        ,this->false_constraints //newnew
        ,this->progress
        );
        csf.find();
        mtx.lock();
        //printf("\n%lld,%lld\n\n",st,num_assign);
        for (auto now_info : csf.results) {
            bool found = false;
            for(auto &info:final_results)
            if (info.metric == now_info.metric) {
                found = true;
                for(auto s:now_info.seeds)
                    info.seeds.push_back(s);
                std::sort(info.seeds.begin(), info.seeds.end());
                auto last = std::unique(info.seeds.begin(), info.seeds.end());
                info.seeds.erase(last, info.seeds.end());
                info.seed_count+=now_info.seed_count;
                break;
            }
            if (!found)     final_results.push_back(now_info);
        }
        mtx.unlock();
    }
    void multi_thread_find_continue(){
        uint64_t block = 1 + num_total/MAX_THREAD, num_assign = block + interval;
        for (int i=0;i<MAX_THREAD;i++){
            uint64_t st = uint64_t(i*block);
            std::thread t([this,st,num_assign]{
                single_thread_find_continue(st,num_assign);
            });
            pool.push_back(std::move(t));
        }
        for (auto &t: pool) t.join();
    }
};


class QuickKMPFinder : public BasicSeedFinder {
protected:
    ZombieTypeList _zlt;
    std::vector<uint32_t> masks;
    std::vector<int> next;

    Scene scene;
    uint32_t cur_seed;
    uint64_t num_total;
    uint64_t num_cur;   //[0,numtotal)
    uint64_t candidate_total;
    bool progress;
public:
    std::vector<SeedInfo> results;
    QuickKMPFinder(uint32_t start_seed,uint64_t start_offset,uint64_t num,
        Scene sc,std::vector<uint32_t> mask,bool show_progress = true,
        uint64_t candidate_num = UINT64_MAX){
        scene = sc;
        masks.clear();masks = mask;
        cur_seed = uint32_t(start_seed+start_offset*SEED_STEP);
        num_total = num;
        num_cur = 0;
        candidate_total = candidate_num == UINT64_MAX
            ? (num >= masks.size() && !masks.empty() ? num-masks.size()+1 : 0)
            : candidate_num;
        progress = show_progress;
        results.clear();
        results.push_back(SeedInfo(0,{},0));
        // std::cout<<results.size()<<'\n';
        _zlt.~ZombieTypeList(); 
        new(&_zlt) ZombieTypeList(scene); 
        buildnext();
    }
    bool check_match(uint32_t has,size_t j){
        if(j>=masks.size())return false;
        return (has&0x7ffff)==masks[j];
    }
    void buildnext(){
        next.assign(masks.size(),0);
        for(size_t i=1,j=0;i<masks.size();++i){
            while(j && masks[i]!=masks[j]) j=next[j-1];
            if(masks[i]==masks[j]) ++j;
            next[i]=int(j);
        }
    }
    void find(){
        if(masks.empty() || candidate_total==0) return;
        uint32_t idx=uint32_t(cur_seed*inv);
        size_t j=0;
        for(uint64_t i=0;i<num_total;++i){
            uint32_t has=array[idx++];
            while(j && !check_match(has,j)) j=next[j-1];
            if(check_match(has,j)) ++j;
            if(j==masks.size()){
                uint64_t start=i+1-masks.size();
                if(start<candidate_total){
                    ++results[0].seed_count;
                    results[0].seeds.push_back(uint32_t(cur_seed+start*SEED_STEP));
                }
                j=next[j-1];
            }
            num_cur=i+1;
            if(progress && num_total>=100 && num_cur%(num_total/100)==0){
                printf("%.1lf%% %llu %u %llu\n",(100.0*num_cur/num_total),num_cur,
                    uint32_t(cur_seed+num_cur*SEED_STEP),results[0].seed_count);
                fflush(stdout);
            }
        }
    }
};
class MultiQuickKMPFinder: public BasicSeedFinder{
private:
    std::vector<std::thread> pool;

    uint32_t start_seed;
    uint64_t num_total;
    Scene scene;
    std::vector<uint32_t> masks;
    bool progress;

public:
    MultiQuickKMPFinder(uint32_t start_s,uint64_t num_t,Scene sc,
        std::vector<uint32_t> mask,bool show_progress = true)
    :start_seed(start_s),num_total(num_t),scene(sc),masks(std::move(mask)),
        progress(show_progress) {pool.clear();}

    void single_thread_find_kmp(uint64_t st,uint64_t core,uint64_t scan){
        QuickKMPFinder kmpf(this->start_seed,st,scan,this->scene,this->masks,
            this->progress,core);
        kmpf.find();
        if(kmpf.results[0].seed_count==0)    return;
        std::lock_guard<std::mutex> lock(mtx);
        if(final_results.empty())   final_results.push_back(kmpf.results[0]);
        else{
            final_results[0].seeds.insert(final_results[0].seeds.end(),
                kmpf.results[0].seeds.begin(),kmpf.results[0].seeds.end());
            final_results[0].seed_count+=kmpf.results[0].seed_count;
        }
    }
    void multi_thread_find_kmp(){
        if(num_total==0 || masks.empty()) return;
        pool.clear();
        uint64_t workers=std::min<uint64_t>(std::max(1u,MAX_THREAD),num_total);
        uint64_t block=num_total/workers, remain=num_total%workers, st=0;
        for(uint64_t i=0;i<workers;i++){
            uint64_t core=block+(i<remain);
            uint64_t scan=core+masks.size()-1;
            std::thread t([this,st,core,scan]{
                single_thread_find_kmp(st,core,scan);
            });
            pool.push_back(std::move(t));
            st+=core;
        }
        for (auto &t: pool) t.join();
    }
};

///*
class QuickSetFinder : public BasicSeedFinder {
protected:
    const uint64_t MAX_COUNT = 10;
    const int LOWEST_STORE = 1;
    uint32_t density_threshold;
    bool progress;

    ZombieTypeList _zlt;
    // 26=density
    uint32_t factor_mask;
    std::vector<uint32_t> factor_constraints;

    uint32_t mask_len = 0;
    uint32_t* idx_map = nullptr;

    uint32_t set_size = 0;
    struct tree_node{
        int num_type_subtree=0;
        int num_real_node=0;
    };
    tree_node* judge_tree = nullptr;
    // son1,son2 = fa*2+1,fa*2+2
    // fa = (son-1)/2

    std::queue<uint32_t> q_elem;


    Scene scene;
    uint32_t cur_seed;
    uint64_t num_total;
    uint64_t num_cur;
    uint64_t candidate_total;

public:
    std::vector<SeedInfo> results;

    QuickSetFinder( uint32_t start_seed,uint64_t start_offset,uint64_t num,
        Scene sc,
        uint32_t mask,
        std::vector<uint32_t> set,
        uint32_t density = 12000,
        bool show_progress = true,
        uint64_t candidate_num = UINT64_MAX)
        : density_threshold(density), progress(show_progress)
    {
        scene = sc;
        factor_constraints = set;
        factor_mask = mask;
        set_size = factor_constraints.size();

        auto x = factor_mask;
        while (x != 0) {
            x &= (x - 1); 
            mask_len++;
        }

        cur_seed = uint32_t(start_seed+start_offset*SEED_STEP);
        num_total = num;
        num_cur = 0;
        candidate_total = candidate_num == UINT64_MAX ? num : candidate_num;

        _zlt.~ZombieTypeList(); 
        new(&_zlt) ZombieTypeList(scene); 

        build_map();
        judge_tree = new tree_node[set_size]();
        while (!q_elem.empty())     q_elem.pop();

        results.clear();
        // results.push_back(SeedInfo(0,{},0));
    }
    ~QuickSetFinder(){
        delete[] idx_map;
        delete[] judge_tree;
    }
    uint32_t get_idx_init(uint32_t has){
        uint32_t ans_idx = 0;
        uint32_t k=1,j=1;
        for(uint32_t i=0;i<20;i++){
            if(factor_mask&j){
                if(has&j)   ans_idx |= k;
                k<<=1;
            }
            j<<=1;
        }
        if(factor_mask&j){
            if(has&j){
                ans_idx |= k;
            }
        }
        return ans_idx;
    }
    uint32_t get_idx(uint32_t has){
        uint32_t ans_idx = 0;
        uint32_t k=1,j=1;
        for(uint32_t i=0;i<20;i++){
            if(factor_mask&j){
                if(has&j)   ans_idx |= k;
                k<<=1;
            }
            j<<=1;
        }
        if(factor_mask&j){
            if(weight_has(has)<density_threshold){
                ans_idx |= k;
            }
        }
        return ans_idx;
    }
    void build_map(){
        uint32_t M = (1<<mask_len);
        idx_map = new uint32_t[M](); 
        for(int i=0;i<M;i++){
            idx_map[i]=0;
        }
        for(uint32_t i=0;i<set_size;i++){
            auto cons_has = factor_constraints[i];
            auto idx = get_idx_init(cons_has);
            idx_map[idx] = i+1;
        }
    }

    void q_push(uint32_t elem){
        q_elem.push(elem);
        if(elem!=0){
            auto son = elem-1;
            judge_tree[son].num_real_node ++;
            if(judge_tree[son].num_real_node == 1){
                while (true) {
                    judge_tree[son].num_type_subtree ++;
                    if(son == 0)    break;
                    son = (son-1)/2;
                }
            }
        }
    }
    void q_pop(){
        auto elem = q_elem.front();
        q_elem.pop();
        if(elem!=0){
            auto son = elem-1;
            judge_tree[son].num_real_node --;
            if(judge_tree[son].num_real_node == 0){
                while (true) {
                    judge_tree[son].num_type_subtree --;
                    if(son == 0)    break;
                    son = (son-1)/2;
                }
            }
        }
    }

    void find(){
        uint32_t idx=uint32_t(cur_seed*inv);
        while(num_cur<num_total){

            while (q_elem.size()!=judge_tree[0].num_type_subtree && !q_elem.empty())
                q_pop();
            
            while(num_cur<num_total){
                auto elem = idx_map[get_idx(array[idx++])];
                q_push(elem);

                if(q_elem.size()!=judge_tree[0].num_type_subtree)
                    break;
                
                cur_seed+=SEED_STEP;
                num_cur++;
            }
            
            auto len = q_elem.size()-1;
            bool found=false;
            if(len>=LOWEST_STORE && num_cur>=len && num_cur-len<candidate_total){
                for (auto &info : results) 
                if (info.metric == len) {
                    found = true;
                    ++info.seed_count;
                    auto seed = uint32_t(cur_seed-SEED_STEP*len);
                    if (int(info.seed_count) <= MAX_COUNT)
                        info.seeds.push_back(seed);
                    else if(progress && len == set_size){
                        printf("end\n");
                        return;
                    }
                    break;
                }
                if (!found) 
                    results.push_back(SeedInfo(len, {uint32_t(cur_seed-SEED_STEP*len)}, 1));
            }

            if(progress && num_total>=100 && (num_cur)%(num_total/100)==0)    {
                printf("%.1lf%% %d %lld %u\n",(100.0*num_cur/num_total),judge_tree[0].num_type_subtree,num_cur,uint32_t(cur_seed-SEED_STEP*len));
                fflush(stdout);
            }
            
            cur_seed+=SEED_STEP;
            num_cur++;
        }
    }
};

class MultiQuickSetFinder: public BasicSeedFinder{
private:
    std::vector<std::thread> pool;

    uint32_t start_seed;
    uint64_t num_total;

    Scene scene;
    uint32_t factor_mask;
    std::vector<uint32_t> factor_constraints;
    uint32_t density_threshold;
    bool progress;
public:
    MultiQuickSetFinder(uint32_t start_s,uint64_t num_t,Scene sc,
        uint32_t mask,
        std::vector<uint32_t> set,
        uint32_t density = 12000,
        bool show_progress = true)
    :   start_seed(start_s),num_total(num_t),scene(sc),
        factor_mask(mask),factor_constraints(set),
        density_threshold(density),progress(show_progress) {pool.clear();}
    void single_thread_find_score(uint64_t st,uint64_t core,uint64_t scan){
        QuickSetFinder ssf(this->start_seed,st,scan,this->scene,
            this->factor_mask,this->factor_constraints,
            this->density_threshold,this->progress,core);

        ssf.find();
        auto in_core=[&](uint32_t seed){
            uint64_t offset=uint32_t(uint64_t(seed-start_seed)*inv);
            return offset>=st && offset<st+core;
        };
        for(auto &info:ssf.results)
            info.seeds.erase(std::remove_if(info.seeds.begin(),info.seeds.end(),
                [&](uint32_t seed){return !in_core(seed);}),info.seeds.end());

        std::lock_guard<std::mutex> lock(mtx);
        for (auto now_info : ssf.results) {
            if(now_info.seeds.empty()) continue;
            bool found = false;
            for(auto &info:final_results)
            if (info.metric == now_info.metric) {
                found = true;
                for(auto s:now_info.seeds)
                    info.seeds.push_back(s);
                std::sort(info.seeds.begin(),info.seeds.end(),[&](uint32_t a,uint32_t b){
                    return uint32_t(uint64_t(a-start_seed)*inv)<uint32_t(uint64_t(b-start_seed)*inv);
                });
                auto last = std::unique(info.seeds.begin(), info.seeds.end());
                info.seeds.erase(last, info.seeds.end());
                info.seed_count += now_info.seed_count;
                break;
            }
            if (!found)     final_results.push_back(now_info);
        }

    }
    void multi_thread_find_score(){
        if(num_total==0 || factor_constraints.empty()) return;
        pool.clear();
        uint64_t workers=std::min<uint64_t>(std::max(1u,MAX_THREAD),num_total);
        uint64_t block=num_total/workers, remain=num_total%workers, st=0;
        for(uint64_t i=0;i<workers;i++){
            uint64_t core=block+(i<remain);
            uint64_t scan=core+factor_constraints.size()+1;
            std::thread t([this,st,core,scan]{
                single_thread_find_score(st,core,scan);
            });
            pool.push_back(std::move(t));
            st+=core;
        }
        for (auto &t: pool) t.join();
    }
};
//*/
