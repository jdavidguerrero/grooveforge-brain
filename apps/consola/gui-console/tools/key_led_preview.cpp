#include <iostream>
#include "consola_s/KeyLedReturn.h"
int main(int argc,char** argv){
    if(argc==2 && std::string_view(argv[1])=="--chain"){
        for(const auto& key:key_leds::CHAIN)
            std::cout<<key.ref<<','<<key.x_mm<<','<<key.y_mm<<'\n';
        return 0;
    }
    KeyLedReturn leds;
    uint32_t now;unsigned channel,type,cc,value;
    while(std::cin>>now>>channel>>type>>cc>>value){
        if(channel)leds.receive(channel,type,cc,value,now);
        else if(leds.render(now)){
            std::cout<<now<<' '<<leds.current_ua();
            for(unsigned i=0;i<KeyLedReturn::BYTES;++i)std::cout<<' '<<unsigned(leds.colors()[i]);
            std::cout<<'\n';
        }
    }
}
