// Offline adapter for the production renderer; one input row is ms/ch/type/data1/data2.
#include "consola_s/PadLedReturn.h"
#include <iostream>
int main(){PadLedReturn leds;uint32_t ms;unsigned channel,type,a,b;
    while(std::cin>>ms>>channel>>type>>a>>b){
        if(type)leds.receive(channel,type,a,b,ms);
        else {leds.render(ms);std::cout<<ms;for(unsigned i=0;i<96;++i)std::cout<<' '<<unsigned(leds.colors()[i]);std::cout<<'\n';leds.sent();}
    }
}
