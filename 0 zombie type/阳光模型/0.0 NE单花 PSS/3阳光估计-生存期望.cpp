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
    printf("LOAD END\n");
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
size_t cost_test(uint32_t rng_seed){
    BasicSeedFinder tmp;    
    double ans = 9990;
    uint32_t seed_idx = rng_seed*inv;
    size_t len = 0;
    for(;;len++){
        double cost = cost_model(array[seed_idx+len]);
        // pss free
        // cost+=77.09077381;
        ans-=cost;
        if(ans>9990) ans=9990;
        if(ans<-500)
            return len;
    }
}
void cal(){
    // freopen("output.txt", "w", stdout);
    load_data();
    int N=20;
    size_t sum = 0;
    for(int i=0;i<N;i++){
        std::random_device rd;  // 非确定性随机数生成器
        std::mt19937 gen(rd()); // 使用random_device初始化Mersenne Twister生成器
        std::uniform_int_distribution<uint32_t> dis(0, UINT32_MAX);
        uint32_t randomValue = dis(gen);
        size_t cnt = cost_test(randomValue);
        sum+=cnt;
        // if(i%(N/100)==0)
        printf("%d:%llu,%lf \n",i,cnt,1.0*sum/(i+1));
    }
    std::cout<<1.0*sum/N<<'\n';
}


int main(){
    cal();
    // print_han();
    //show();

    return 0;
}