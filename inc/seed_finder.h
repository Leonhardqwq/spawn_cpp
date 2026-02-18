#pragma once
#include "basic_setting.h"
#include <thread>
#include <mutex>


unsigned int MAX_THREAD = std::thread::hardware_concurrency()/4+std::thread::hardware_concurrency()/2;

// unsigned int MAX_THREAD = std::thread::hardware_concurrency();
std::mutex mtx;


class ContinueSeedFinder : public BasicSeedFinder {
protected:
    const uint64_t MAX_COUNT = 100;
    const int LOWEST_STORE = 3;
    ZombieTypeList _zlt;
    std::vector<uint32_t> masks;
    std::vector<uint32_t> fmasks;//newnew

    Scene scene;
    uint32_t cur_seed;
    uint64_t num_total;
    uint64_t num_cur;
public:
    std::vector<SeedInfo> results;

    ContinueSeedFinder(uint32_t start_seed,uint64_t start_offset,uint64_t num,
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
        while(num_cur<num_total){
            int len = 0;
            while(num_cur<num_total){
                if(!check_constraints(_zlt.get_list(cur_seed)))
                    break;
                cur_seed+=SEED_STEP;
                num_cur++;
                len++;
            }
            bool found=false;

            if(len>LOWEST_STORE){
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
class MultiContinueSeedFinder: public BasicSeedFinder{
private:
    std::vector<std::thread> pool;

    const uint32_t interval = 50;
    uint32_t start_seed;
    uint64_t num_total;
    Scene scene;
    std::vector<std::vector<uint32_t>> true_constraints;
    std::vector<std::vector<uint32_t>> false_constraints;//newnew

public:
    MultiContinueSeedFinder(uint32_t start_s,uint64_t num_t,Scene sc,std::vector<std::vector<uint32_t>> true_cons
    ,std::vector<std::vector<uint32_t>> false_cons //newnew
    )
    :start_seed(start_s),num_total(num_t),scene(sc),true_constraints(true_cons) 
    ,false_constraints(false_cons) //newnew
    {pool.clear();}
    void single_thread_find_continue(uint64_t st,uint64_t num_assign){
        ContinueSeedFinder csf(this->start_seed,st,num_assign,this->scene,this->true_constraints
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


class ScoreSeedFinder : public BasicSeedFinder {
protected:
    const uint64_t MAX_COUNT = 100;
    const int LOWEST_STORE = 5;
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

    ScoreSeedFinder(uint32_t start_seed,uint64_t start_offset,uint64_t num,uint64_t num_l,
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
        int ans_score = 0;
        for(int i=0;i<constraints.size();i++){
            auto z = constraints[i];
            if(has&(1<<z))
                ans_score+=weight[z];
        }
        return ans_score;
    }
    void find(){
        std::queue<int> q_score;
        int sum_score = 0;
        for(int i=0;i<num_level;i++){
            int tmp_score = get_score(_zlt.get_list(cur_seed));
            q_score.push(tmp_score);
            sum_score+=tmp_score;
            cur_seed+=SEED_STEP;
        }
        while(num_cur<num_total){
            int tmp_score = get_score(_zlt.get_list(cur_seed));
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
class MultiScoreSeedFinder: public BasicSeedFinder{
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
    MultiScoreSeedFinder(uint32_t start_s,uint64_t num_t,uint64_t num_l,Scene sc,
        std::vector<uint32_t> true_cons,std::vector<int> true_w)
    :   start_seed(start_s),num_total(num_t),num_level(num_l),scene(sc),
        constraints(true_cons),weights(true_w) {pool.clear();}
    void single_thread_find_score(uint64_t st,uint64_t num_assign){
        ScoreSeedFinder ssf(this->start_seed,st,num_assign,this->num_level,this->scene,
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


class KMPFinder : public BasicSeedFinder {
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
    KMPFinder(uint32_t start_seed,uint64_t start_offset,uint64_t num,
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
                if(j<0 || check_match(_zlt.get_list(cur_seed), j)){
                    i++,j++,cur_seed+=SEED_STEP,num_cur++;
                    // std::cout<<i<<j<<'\n';
                }
                else {
                    j=next[j];
                }
                if((num_cur)%(num_total/100)==0)
                    printf("%.1lf%% %llu %u %llu\n",(100.0*num_cur/num_total),num_cur,cur_seed,results[0].seed_count);
            }

            if(j!=m) {
                return;
            }
            
            results[0].seed_count++;
            results[0].seeds.push_back(uint32_t(cur_seed-m*SEED_STEP));

        }
    }
};
class MultiKMPFinder: public BasicSeedFinder{
private:
    std::vector<std::thread> pool;

    const uint32_t interval = 50;
    uint32_t start_seed;
    uint64_t num_total;
    Scene scene;
    std::vector<uint32_t> masks;

public:
    MultiKMPFinder(uint32_t start_s,uint64_t num_t,Scene sc,    std::vector<uint32_t> mask)
    :start_seed(start_s),num_total(num_t),scene(sc),masks(mask) {pool.clear();}

    void single_thread_find_kmp(uint64_t st,uint64_t num_assign){
        KMPFinder kmpf(this->start_seed,st,num_assign,this->scene,this->masks);
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
