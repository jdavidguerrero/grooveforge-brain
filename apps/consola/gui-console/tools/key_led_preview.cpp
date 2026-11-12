#include <iostream>
#include "consola_s/KeyLedReturn.h"
int main(){
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
