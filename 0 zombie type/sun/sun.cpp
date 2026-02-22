#include "../../inc/sun_model.hpp"
#include <iostream>

int main(){
    SunConfig config("sun_config.json");    
    // config.show_info();
    work(config);
    // std::cin.get();
    return 0;
}
