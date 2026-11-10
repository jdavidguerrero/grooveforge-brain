#include "consola_s/PadLedReturn.h"
#include <stdexcept>
#include <iostream>
int main(){try{
    PadLedReturn leds;
    if(!leds.dirty())throw std::runtime_error("initial snapshot missing");leds.sent();
    if(leds.receive(1,0x90,48,127)||leds.dirty())throw std::runtime_error("channel1 music altered LEDs");
    leds.receive(16,0xb0,20,127);leds.receive(16,0xb0,21,12);leds.receive(16,0xb0,22,4);
    if(leds.dirty())throw std::runtime_error("RGB setup must wait for pad apply");
    leds.receive(16,0x90,48,1);
    if(!leds.dirty()||leds.colors()[0]!=31||leds.colors()[1]!=12||leds.colors()[2]!=4)throw std::runtime_error("pad apply/brightness clamp");
    leds.sent();leds.receive(16,0x90,48,1);if(leds.dirty())throw std::runtime_error("duplicate LED dirtied snapshot");
    leds.receive(16,0x90,79,1);if(leds.colors()[93]!=31)throw std::runtime_error("last grid pad");
    leds.receive(16,0x80,48,100);if(leds.colors()[0])throw std::runtime_error("note off must black pad");
    leds.receive(16,0x90,79,0);if(leds.colors()[93])throw std::runtime_error("velocity zero must black pad");
    if(leds.receive(16,0x90,80,1))throw std::runtime_error("button not a grid pad");
    leds.sent();leds.reconnect();if(!leds.dirty())throw std::runtime_error("reconnect snapshot missing");
    std::cout<<"DRUM/SYNTH LED return: channel isolation, RGB apply/dedup/off, 32 pads, brightness clamp, reconnect snapshot: PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
