#include "seed_finder.h"
#include <corecrt.h>
#include <cstdint>
#include <cstdio>
#include <iostream>

const size_t array_size = 0x100000000; // 2^32
uint32_t* array = new uint32_t[array_size];
const uint64_t inv = 2083697005;


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

public:
    std::vector<SeedInfo> results;

    QuickScoreSeedFinder(uint32_t start_seed,uint64_t start_offset,uint64_t num,uint64_t num_l,
        Scene sc,std::vector<uint32_t> true_constraints,std::vector<int> true_weight){
        scene = sc;
        constraints = true_constraints;
        for(int i=0;i<constraints.size();i++)
            weight[constraints[i]] = true_weight[i];
        cur_seed = uint32_t(start_seed+start_offset*SEED_STEP);
        num_total = num;
        num_cur = 0;
        num_level = num_l;
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
        std::queue<int> q_score;
        int sum_score = 0;
        uint32_t idx=uint32_t(cur_seed*inv);
        for(int i=0;i<num_level;i++){
            int tmp_score = get_score(array[idx++]);
            q_score.push(tmp_score);
            sum_score+=tmp_score;
            cur_seed+=SEED_STEP;
        }
        while(num_cur<num_total){
            int tmp_score = get_score(array[idx++]);
            int minus_score = q_score.front();
            q_score.push(tmp_score);q_score.pop();
            sum_score+= tmp_score-minus_score;

            cur_seed+=SEED_STEP;
            num_cur++;

            bool found=false;
            if(sum_score>LOWEST_STORE){
                for (auto &info : results) 
                if (info.metric == sum_score) {
                    found = true;
                    ++info.seed_count;
                    if (int(info.seed_count) <= MAX_COUNT)
                        info.seeds.push_back(uint32_t(cur_seed-SEED_STEP*num_level));
                    break;
                }
                if (!found) 
                    results.push_back(SeedInfo(sum_score, {uint32_t(cur_seed-SEED_STEP*num_level)}, 1));
            }
            if((num_cur)%(num_total/100)==0){
                printf("%.1lf%% %d %lld %u\n",(100.0*num_cur/num_total),sum_score,num_cur,uint32_t(cur_seed-SEED_STEP*num_level));
                fflush(stdout);
            }
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

    const uint32_t interval = 5;
    uint32_t start_seed;
    uint64_t num_total;
    uint64_t num_level;
    Scene scene;
    std::vector<uint32_t> constraints;
    std::vector<int> weights;

public:
    MultiQuickScoreSeedFinder(uint32_t start_s,uint64_t num_t,uint64_t num_l,Scene sc,
        std::vector<uint32_t> true_cons,std::vector<int> true_w)
    :   start_seed(start_s),num_total(num_t),num_level(num_l),scene(sc),
        constraints(true_cons),weights(true_w) {pool.clear();}
    void single_thread_find_score(uint64_t st,uint64_t num_assign){
        QuickScoreSeedFinder ssf(this->start_seed,st,num_assign,this->num_level,this->scene,
            this->constraints,this->weights);
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
        uint64_t block = 1 + num_total/MAX_THREAD, num_assign = block + num_level + interval;
        for (int i=0;i<MAX_THREAD;i++){
            uint64_t st = uint64_t(i*block);
            std::thread t([this,st,num_assign]{
                single_thread_find_score(st,num_assign);
            });
            pool.push_back(std::move(t));
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
public:
    std::vector<SeedInfo> results;

    QuickContinueSeedFinder(uint32_t start_seed,uint64_t start_offset,uint64_t num,
        Scene sc,std::vector<std::vector<uint32_t>> true_constraints
        ,std::vector<std::vector<uint32_t>> false_constraints
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

            if((num_cur)%(num_total/100)==0){
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

public:
    MultiQuickContinueSeedFinder(uint32_t start_s,uint64_t num_t,Scene sc,std::vector<std::vector<uint32_t>> true_cons
    ,std::vector<std::vector<uint32_t>> false_cons //newnew
    )
    :start_seed(start_s),num_total(num_t),scene(sc),true_constraints(true_cons) 
    ,false_constraints(false_cons) //newnew
    {pool.clear();}
    void single_thread_find_continue(uint64_t st,uint64_t num_assign){
        QuickContinueSeedFinder csf(this->start_seed,st,num_assign,this->scene,this->true_constraints
        ,this->false_constraints //newnew
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
public:
    std::vector<SeedInfo> results;
    QuickKMPFinder(uint32_t start_seed,uint64_t start_offset,uint64_t num,
        Scene sc,std::vector<uint32_t> mask){
        scene = sc;
        masks.clear();masks = mask;
        cur_seed = uint32_t(start_seed+start_offset*SEED_STEP);
        num_total = num;
        num_cur = 0;
        results.clear();
        results.push_back(SeedInfo(0,{},0));
        // std::cout<<results.size()<<'\n';
        _zlt.~ZombieTypeList(); 
        new(&_zlt) ZombieTypeList(scene); 
        buildnext();
    }
    bool check_match(uint32_t has,int j){
        if(j>=masks.size())return false;
        if(j<0) return true;
        return (has&0x7ffff)==masks[j];
    }
    void buildnext(){
        size_t m=masks.size(),j=0;
        int* ne = new int[m]; int t = ne[0]=-1;
        while(j<m-1){
            if(0>t || masks[t]==masks[j]){
                if(masks[++t]!=masks[++j])
                    ne[j]=t;
                else ne[j]=ne[t];
            }
            else t = ne[t];
        } 
        next.clear();
        for(int i=0;i<m;i++)
            next.push_back(ne[i]);
        // for(auto tmp:next) printf("%d ",tmp);
        
    }
    void find(){
        while(num_cur<num_total){
            size_t n = num_total;
            size_t i=0;
            long long m = masks.size();
            int j=0;
            while(j<m && i<n && num_cur<num_total){
                if(j<0 || check_match(array[uint32_t(cur_seed*inv)], j)){
                    i++,j++,cur_seed+=SEED_STEP,num_cur++;
                    // std::cout<<i<<j<<'\n';
                }
                else {
                    j=next[j];
                }
                if((num_cur)%(num_total/100)==0){
                    printf("%.1lf%% %llu %u %llu\n",(100.0*num_cur/num_total),num_cur,cur_seed,results[0].seed_count);
                    fflush(stdout);
                }
            }

            if(j!=m) {
                return;
            }
            
            results[0].seed_count++;
            results[0].seeds.push_back(uint32_t(cur_seed-m*SEED_STEP));

        }
    }
};
class MultiQuickKMPFinder: public BasicSeedFinder{
private:
    std::vector<std::thread> pool;

    const uint32_t interval = 50;
    uint32_t start_seed;
    uint64_t num_total;
    Scene scene;
    std::vector<uint32_t> masks;

public:
    MultiQuickKMPFinder(uint32_t start_s,uint64_t num_t,Scene sc,    std::vector<uint32_t> mask)
    :start_seed(start_s),num_total(num_t),scene(sc),masks(mask) {pool.clear();}

    void single_thread_find_kmp(uint64_t st,uint64_t num_assign){
        QuickKMPFinder kmpf(this->start_seed,st,num_assign,this->scene,this->masks);
        kmpf.find();
        if(kmpf.results[0].seed_count==0)    return;
        mtx.lock();
        if(final_results.empty())   final_results.push_back(kmpf.results[0]);
        else{
            for(auto s:kmpf.results[0].seeds){
                final_results[0].seeds.push_back(s);
                final_results[0].seed_count++;

                std::sort(final_results[0].seeds.begin(), final_results[0].seeds.end());
                auto last = std::unique(final_results[0].seeds.begin(), final_results[0].seeds.end());
                final_results[0].seeds.erase(last, final_results[0].seeds.end());
            }
        }
        mtx.unlock();
    }
    void multi_thread_find_kmp(){
        uint64_t block = 1 + num_total/MAX_THREAD, num_assign = block + interval;
        for (int i=0;i<MAX_THREAD;i++){
            uint64_t st = uint64_t(i*block);
            std::thread t([this,st,num_assign]{
                single_thread_find_kmp(st,num_assign);
            });
            pool.push_back(std::move(t));
        }
        for (auto &t: pool) t.join();
    }
};

///*
class QuickSetFinder : public BasicSeedFinder {
protected:
    const uint64_t MAX_COUNT = 10;
    const int LOWEST_STORE = 1;
    const uint32_t DENSITY = 13000;

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

public:
    std::vector<SeedInfo> results;

    QuickSetFinder( uint32_t start_seed,uint64_t start_offset,uint64_t num,
        Scene sc,
        uint32_t mask,
        std::vector<uint32_t> set)
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

        _zlt.~ZombieTypeList(); 
        new(&_zlt) ZombieTypeList(scene); 

        build_map();
        judge_tree = new tree_node[set_size]();
        while (!q_elem.empty())     q_elem.pop();

        results.clear();
        // results.push_back(SeedInfo(0,{},0));
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
            if(weight_has(has)<DENSITY){
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
            if(len>=LOWEST_STORE){
                for (auto &info : results) 
                if (info.metric == len) {
                    found = true;
                    ++info.seed_count;
                    if (int(info.seed_count) <= MAX_COUNT)
                        info.seeds.push_back(uint32_t(cur_seed-SEED_STEP*len));
                    else if(len == set_size){
                        printf("end\n");
                        return;
                    }
                    break;
                }
                if (!found) 
                    results.push_back(SeedInfo(len, {uint32_t(cur_seed-SEED_STEP*len)}, 1));
            }

            if((num_cur)%(num_total/100)==0)    {
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

    const uint32_t interval = 50;
    uint32_t start_seed;
    uint64_t num_total;

    Scene scene;
    uint32_t factor_mask;
    std::vector<uint32_t> factor_constraints;
public:
    MultiQuickSetFinder(uint32_t start_s,uint64_t num_t,Scene sc,
        uint32_t mask,
        std::vector<uint32_t> set)
    :   start_seed(start_s),num_total(num_t),scene(sc),
        factor_mask(mask),factor_constraints(set) {pool.clear();}
    void single_thread_find_score(uint64_t st,uint64_t num_assign){
        QuickSetFinder ssf(this->start_seed,st,num_assign,this->scene,
            this->factor_mask,this->factor_constraints);

        ssf.find();
        mtx.lock();
        /* 
        if(final_results.empty())   final_results.push_back(ssf.results[0]);
        else{

            for(auto s:ssf.results[0].seeds){
                final_results[0].seeds.push_back(s);
                final_results[0].seed_count++;

                std::sort(final_results[0].seeds.begin(), final_results[0].seeds.end());
                auto last = std::unique(final_results[0].seeds.begin(), final_results[0].seeds.end());
                final_results[0].seeds.erase(last, final_results[0].seeds.end());
            }
        }
        */
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
                break;
            }
            if (!found)     final_results.push_back(now_info);
        }
        mtx.unlock();

    }
    void multi_thread_find_score(){
        uint64_t block = 1 + num_total/MAX_THREAD, num_assign = block + interval;
        for (int i=0;i<MAX_THREAD;i++){
            uint64_t st = uint64_t(i*block);
            std::thread t([this,st,num_assign]{
                single_thread_find_score(st,num_assign);
            });
            pool.push_back(std::move(t));
        }
        for (auto &t: pool) t.join();
    }
};
//*/
