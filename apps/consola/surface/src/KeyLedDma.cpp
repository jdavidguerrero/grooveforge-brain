#if defined(GF_CONSOLA_UNO) && defined(__IMXRT1062__)
/* Timing and QTIMER/XBAR setup adapted from OctoWS2811_imxrt.cpp.
 * Copyright (c) 2020 Paul Stoffregen, PJRC.COM, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <Arduino.h>
#include <DMAChannel.h>
#include "consola_s/KeyLedDma.h"
#include "consola_s/KeyLedReturn.h"

extern "C" void xbar_connect(unsigned int input,unsigned int output);
namespace {
constexpr unsigned BITS=KeyLedReturn::COUNT*24;
constexpr uint32_t MASK=1u<<3; // T4.1 pin13 = GPIO2_IO03 (normally fast GPIO7).
DMAMEM uint32_t zeros[BITS] __attribute__((aligned(32)));
DMAMEM uint32_t mask[8] __attribute__((aligned(32)));
DMAChannel rise(false),zero(false),fall(false);
uint32_t started_at=0;
bool submitted=false;
void configure(DMAChannel& dma,volatile uint32_t& dest,unsigned request,bool buffered){
    dma.begin();
    if(buffered)dma.sourceBuffer(zeros,sizeof(zeros));
    else dma.source(mask[0]);
    dma.destination(dest);
    dma.transferCount(BITS);
    dma.disableOnCompletion();
    dma.triggerAtHardwareEvent(request);
}
}
void key_led_dma::begin(){
    // DMAMEM is NOLOAD on T4.x: initialize the constant source at runtime.
    for(auto& word:mask)word=0;
    mask[0]=MASK;
    // Only pin13 moves from fast GPIO7 to DMA-addressable GPIO2.
    pinMode(13,OUTPUT);digitalWrite(13,LOW);
    IOMUXC_GPR_GPR27 &= ~MASK;
    GPIO2_GDIR |= MASK;
    GPIO2_DR_CLEAR=MASK;
    TMR4_ENBL &= ~7;
    const uint16_t period=uint16_t(F_BUS_ACTUAL/800000);
    const uint16_t t0=uint16_t(F_BUS_ACTUAL*30ull/100000000);
    const uint16_t t1=uint16_t(F_BUS_ACTUAL*75ull/100000000);
    TMR4_SCTRL0=TMR_SCTRL_OEN|TMR_SCTRL_FORCE|TMR_SCTRL_MSTR;
    TMR4_CSCTRL0=TMR_CSCTRL_CL1(1)|TMR_CSCTRL_TCF1EN;
    TMR4_CNTR0=0;TMR4_LOAD0=0;TMR4_COMP10=period;TMR4_CMPLD10=period;
    TMR4_CTRL0=TMR_CTRL_CM(1)|TMR_CTRL_PCS(8)|TMR_CTRL_LENGTH|TMR_CTRL_OUTMODE(3);
    TMR4_SCTRL1=TMR_SCTRL_OEN|TMR_SCTRL_FORCE;
    TMR4_CNTR1=0;TMR4_LOAD1=0;TMR4_COMP11=t0;TMR4_CMPLD11=t0;
    TMR4_CTRL1=TMR_CTRL_CM(1)|TMR_CTRL_PCS(8)|TMR_CTRL_COINIT|TMR_CTRL_OUTMODE(3);
    TMR4_SCTRL2=TMR_SCTRL_OEN|TMR_SCTRL_FORCE;
    TMR4_CNTR2=0;TMR4_LOAD2=0;TMR4_COMP12=t1;TMR4_CMPLD12=t1;
    TMR4_CTRL2=TMR_CTRL_CM(1)|TMR_CTRL_PCS(8)|TMR_CTRL_COINIT|TMR_CTRL_OUTMODE(3);
    CCM_CCGR2 |= CCM_CCGR2_XBAR1(CCM_CCGR_ON);
    xbar_connect(XBARA1_IN_QTIMER4_TIMER0,XBARA1_OUT_DMA_CH_MUX_REQ30);
    xbar_connect(XBARA1_IN_QTIMER4_TIMER1,XBARA1_OUT_DMA_CH_MUX_REQ31);
    xbar_connect(XBARA1_IN_QTIMER4_TIMER2,XBARA1_OUT_DMA_CH_MUX_REQ94);
    XBARA1_CTRL0=XBARA_CTRL_STS1|XBARA_CTRL_EDGE1(3)|XBARA_CTRL_DEN1|
                  XBARA_CTRL_STS0|XBARA_CTRL_EDGE0(3)|XBARA_CTRL_DEN0;
    XBARA1_CTRL1=XBARA_CTRL_STS0|XBARA_CTRL_EDGE0(3)|XBARA_CTRL_DEN0;
    configure(rise,GPIO2_DR_SET,DMAMUX_SOURCE_XBAR1_0,false);
    configure(zero,GPIO2_DR_CLEAR,DMAMUX_SOURCE_XBAR1_1,true);
    configure(fall,GPIO2_DR_CLEAR,DMAMUX_SOURCE_XBAR1_2,false);
    arm_dcache_flush(mask,sizeof(mask));
    // Initial latch time is also enforced without waiting in setup.
    started_at=micros();
}
bool key_led_dma::submit(const uint8_t* rgb,uint32_t now_us){
    if(uint32_t(now_us-started_at)<1080)return false;
    if(submitted&&(!rise.complete()||!zero.complete()||!fall.complete()))return false;
    TMR4_ENBL &= ~7;
    unsigned bit=0;
    constexpr unsigned GRB[]={1,0,2};
    for(unsigned i=0;i<KeyLedReturn::COUNT;++i)
        for(unsigned c:GRB)
            for(unsigned b=0;b<8;++b)zeros[bit++]=(rgb[i*3+c]&(0x80>>b))?0:MASK;
    arm_dcache_flush(zeros,sizeof(zeros));
    TMR4_SCTRL0=TMR_SCTRL_OEN|TMR_SCTRL_FORCE|TMR_SCTRL_MSTR;
    TMR4_SCTRL1=TMR_SCTRL_OEN|TMR_SCTRL_FORCE;
    TMR4_SCTRL2=TMR_SCTRL_OEN|TMR_SCTRL_FORCE;
    XBARA1_CTRL0 |= XBARA_CTRL_STS1|XBARA_CTRL_STS0;
    XBARA1_CTRL1 |= XBARA_CTRL_STS0;
    rise.clearComplete();zero.clearComplete();fall.clearComplete();
    rise.enable();zero.enable();fall.enable();
    TMR4_CNTR0=0;
    TMR4_CNTR1=TMR4_COMP10+1;TMR4_CNTR2=TMR4_COMP10+1;
    started_at=micros();submitted=true;
    TMR4_ENBL |= 7;
    return true;
}
#endif
