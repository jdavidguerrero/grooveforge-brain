#include "audio.h"
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
thread_local bool realtime=false;
std::atomic<unsigned> violations{0};
void* operator new(std::size_t n) {if(realtime)violations.fetch_add(1);if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{if(realtime)violations.fetch_add(1);std::free(p);}
void operator delete[](void* p) noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept{::operator delete(p);}
void* operator new(std::size_t n,std::align_val_t alignment) {
    if(realtime)violations.fetch_add(1);
    void* p=nullptr;if(posix_memalign(&p,size_t(alignment),n?n:1)==0)return p;throw std::bad_alloc();
}
void* operator new[](std::size_t n,std::align_val_t alignment){return ::operator new(n,alignment);}
void operator delete(void* p,std::align_val_t) noexcept{if(realtime)violations.fetch_add(1);std::free(p);}
void operator delete[](void* p,std::align_val_t alignment) noexcept{::operator delete(p,alignment);}
void operator delete(void* p,std::size_t,std::align_val_t alignment) noexcept{::operator delete(p,alignment);}
void operator delete[](void* p,std::size_t,std::align_val_t alignment) noexcept{::operator delete(p,alignment);}
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){
    using namespace host;
    try{
        initialize_catalogs();auto audio=std::make_unique<Audio>(initial_configs(true));
        std::array<std::array<float,128>,10> data{};std::array<float*,10> outputs{};for(size_t i=0;i<10;++i)outputs[i]=data[i].data();
        uint64_t token=1;double energy=0;
        for(size_t e=0;e<registry().size();++e){
            auto config=defaults(int(e));benchmark_patch(config);auto engine=make_engine(config);
            audio->commands.push({Operation::Swap,token++,0,0,0,engine.release()});
            for(size_t j=0;j<catalog(int(e)).size();++j){
                const auto& param=catalog(int(e))[j];
                audio->commands.push({Operation::Param,token++,0,unsigned(j),(j%2?param.min:param.max),nullptr});
                if(j%32==31){realtime=true;audio->process(128,outputs);realtime=false;Ack a;while(audio->acknowledgements.pop(a))delete a.retired;}
            }
            for(unsigned block=0;block<760;++block){
                std::array<Midi,128> midi{};unsigned count=0;
                for(unsigned f=0;f<128;++f){Midi tmp[32];auto n=pattern(uint64_t(block)*128+f,tmp);for(unsigned i=0;i<n;++i){tmp[i].frame=f;midi[count++]=tmp[i];}}
                if(block%30==0){const uint8_t ccs[]={14,15,16,19,71,74,64};for(unsigned p=0;p<4;++p)for(auto cc:ccs)midi[count++]={0,uint8_t(0xb0|p),cc,uint8_t(block%2?0:90)};std::stable_sort(midi.begin(),midi.begin()+count,[](const Midi&a,const Midi&b){return a.frame<b.frame;});}
                realtime=true;
                const uint32_t ump[]={0x40b04a00,0x80000000};Midi converted;
                if(decode_ump(ump,sizeof(ump),0,converted)!=8||!routed_midi(converted))++violations;
                audio->process(128,outputs,midi.data(),count);realtime=false;
                Ack a;while(audio->acknowledgements.pop(a))delete a.retired;Telemetry t;while(audio->telemetry.pop(t)){};
                for(unsigned f=0;f<128;++f){require(std::isfinite(data[8][f]),"nonfinite audio");require(std::abs((data[0][f]+data[2][f]+data[4][f]+data[6][f])-data[8][f])<1e-6f,"mix differs from part sum");energy+=double(data[8][f])*data[8][f];}
            }
        }
        require(!audio->fault.load(),"audio fault");require(energy>1e-8,"silent audio");require(violations.load()==0,"heap allocation/free on realtime thread");
        std::cout<<"RT: "<<registry().size()<<" engines, params, swaps, 4-part notes/CC/sustain/bend; allocations/frees="<<violations<<"; energy="<<energy<<"; mix verified\n";return 0;
    }catch(const std::exception& e){realtime=false;std::cerr<<e.what()<<'\n';return 1;}
}
