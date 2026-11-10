#include "engine.h"
#include "hybrid_preview.h"
#include "prophet_preset.h"
#include "dx7_preset.h"
#include "dx7_6op_preview.h"
#include "juno_preset.h"
#include "ob6_preset.h"
#include "model_d_preset.h"
#include "arp2600_preset.h"
#include "ui/groove_macros.h"
#include <algorithm>
#include <cmath>
namespace host {
namespace {
class PreviewEngine final:public Engine {
    int id_;
    std::unique_ptr<gf::ProphetPreview> bus_;
    std::unique_ptr<gf::ModelDPreview> model_;
    std::unique_ptr<gf::JunoPreview> juno_;
    std::unique_ptr<gf::Ob6Preview> ob6_;
    std::unique_ptr<gf::Dx7Preview> dx_;
    std::unique_ptr<gf::Dx7SixPreview> six_;
    float low_=0, band_=0, filter_g_=0, filter_k_=1;
    float six_tape_=1;
    int six_bend_=8192;
    void six_expression() noexcept {
        // Native bend has ±3 semitones: reserve the remaining range for tape wow/flutter.
        const int tape=int(std::lround(std::log2(six_tape_)*32768.f));
        six_->set_pitch_bend(8192+(six_bend_-8192)*2/3+tape);
    }
    std::unique_ptr<gf::Arp2600Preview> arp_;
    std::unique_ptr<gf::HybridPreview> hybrid_;
    std::vector<int16_t> dl_,dr_,plate_;
    std::array<float,2> dc_x_{},dc_y_{};
    const float dc_=float(std::exp(-2.0*3.141592653589793*5.0/48000));
    void voice_on(uint8_t n,float v) noexcept {
        switch(id_) { case 0:model_->note_on(n,v);break; case 1:juno_->note_on(n,v);break; case 3:ob6_->note_on(n,v);break; case 4:dx_->note_on(n,v);break; case 5:arp_->note_on(n,v);break; case 7:six_->note_on(n,v);break; }
    }
    void voice_off(uint8_t n) noexcept {
        switch(id_) { case 0:model_->note_off(n);break; case 1:juno_->note_off(n);break; case 3:ob6_->note_off(n);break; case 4:dx_->note_off(n);break; case 5:arp_->note_off(n);break; case 7:six_->note_off(n);break; }
    }
    void apply_hybrid() noexcept {
        const auto& l=config.layer;
        hybrid_->set_layer_controls(l.morph*.01f,l.delay_b_ms,l.transpose_b,l.gain_a_db,l.gain_b_db);
        std::array<float,21> groove{}; std::array<float,25> fx{};
        for(int g=0;g<3;++g) { groove[size_t(g*7)]=l.a.patch.groove_on[g]; for(int f=0;f<(g==2?4:6);++f) groove[size_t(g*7+f+1)]=l.a.patch.groove[g][f]; }
        for(int f=0;f<5;++f) { fx[size_t(f*5)]=l.a.patch.sfx_on[f]; fx[size_t(f*5+1)]=l.a.patch.sfx_mix[f]; for(int s=0;s<3;++s) fx[size_t(f*5+s+2)]=l.a.patch.sfx_p[f][s]; }
        hybrid_->set_bus(groove,fx);
        for(unsigned side=0;side<2;++side) {
            const auto& p=side?l.b:l.a; const auto& v=p.patch; const auto e=side?l.eng_b:l.eng_a;
            std::array<float,48> osc{};
            for(int page=0;page<6;++page) for(int s=0;s<8;++s) {
                const auto desc=page<4?GF_OSC_SLOT[e][page][s]:GF_MOD_SLOT[e][page-4][s];
                const float value=page<4?v.osc[page][s]:v.mod[page-4][s];
                osc[size_t(page*8+s)]=gf_osc_used(&desc)?std::clamp((value-desc.lo)/(desc.hi-desc.lo),0.f,1.f):0;
            }
            hybrid_->set_half_osc(side,osc);
            const std::array<float,20> voice{v.decay_ms,v.sustain,v.f_attack_ms,v.f_decay_ms,v.f_sustain,v.f_release_ms,v.env_amt,v.drive,v.vel_amp,v.vel_cut,v.lfo_rate_hz,float(v.lfo_shape),float(v.lfo_div),v.lfo_amt[0],v.lfo_amt[1],v.lfo_amt[e==4?2:3],v.glide,float(l.a.patch.groove_on[3]),l.a.patch.groove[3][0],l.a.patch.groove[3][1]};
            hybrid_->set_half_voice(side,voice);
            hybrid_->set_half_controls(side,p.cutoff_hz,p.reso_norm,v.attack_ms,v.release_ms);
        }
        constexpr int indices[]={0,1,2,7,4,5,6};
        std::array<float,7> macros{};
        for(size_t i=0;i<7;++i) macros[i]=l.a.patch.groove[4][indices[i]]*.01f;
        hybrid_->set_macros(macros); hybrid_->set_b_move(l.b.patch.groove[4][7]*.01f);
    }
public:
    explicit PreviewEngine(const Config& c):Engine(c),id_(registry()[size_t(c.engine)].firmware) {
        if(id_==6) {
            hybrid_=std::make_unique<gf::HybridPreview>(); hybrid_->prepare(48000);
            if(!hybrid_->load(c.layer)) throw std::runtime_error("invalid HYBRID layer");
            hybrid_->set_transport(120,false,0,true); hybrid_->set_expression(0,0,120,false);
            apply();
        } else {
            bus_=std::make_unique<gf::ProphetPreview>(); bus_->prepare(48000);
            dl_.assign(48002,0); dr_.assign(48002,0); plate_.assign(gf::PlateReverbPreview::required_samples(48000),0);
            bus_->set_delay_memory(dl_.data(),dr_.data(),48002); bus_->set_plate_memory(plate_.data(),plate_.size());
            switch(id_) {
                case 0:model_=std::make_unique<gf::ModelDPreview>();model_->prepare(48000);break;
                case 1:juno_=std::make_unique<gf::JunoPreview>();juno_->prepare(48000);break;
                case 3:ob6_=std::make_unique<gf::Ob6Preview>();ob6_->prepare(48000);break;
                case 4:dx_=std::make_unique<gf::Dx7Preview>();dx_->prepare(48000);break;
                case 7:six_=std::make_unique<gf::Dx7SixPreview>();six_->prepare(48000);six_->set_patch(c.six);break;
                case 5:arp_=std::make_unique<gf::Arp2600Preview>();arp_->prepare(48000);break;
            }
            if(id_!=2) bus_->set_voice_sink(this,[](void* p,uint8_t n,float v){static_cast<PreviewEngine*>(p)->voice_on(n,v);},[](void* p,uint8_t n){static_cast<PreviewEngine*>(p)->voice_off(n);});
            apply(); bus_->reset();
            switch(id_) {
                case 0:model_->reset();model_->set_modulation_control(0,0,120,false);break;
                case 1:juno_->reset();juno_->set_lfo_control(0,120,false);break;
                case 3:ob6_->reset();ob6_->set_lfo_control(0,120,false);break;
                case 4:dx_->reset();dx_->set_lfo_control(0,120,false);break;
                case 5:arp_->reset();arp_->set_lfo_control(0,0,120,false);break;
            }
            bus_->set_transport(120,false,0,true);
        }
    }
    void apply() noexcept override {
        if(hybrid_) { apply_hybrid(); return; }
        const auto& p=config.layer.a;
        const int bus_id=id_==7?4:id_;
        const auto patch=studio::macro_patch(p.patch,uint8_t(bus_id));
        const float move=studio::macro_amount(p.patch,GF_MACRO_MOVE);
        const float cutoff=std::clamp(p.cutoff_hz*studio::move_cutoff_mul(move),id_==7?20.f:GF_CUTOFF_RANGE[bus_id].lo,GF_CUTOFF_RANGE[bus_id].hi);
        const float res=studio::move_resonance(studio::macro_resonance(p.reso_norm,p.patch),move);
        const float q=std::clamp(id_==3?.7f+4.3f*res:.1f+4.9f*res,.707f,5.f);
        switch(id_) {
            case 0:model_->set_parameters(cutoff,q,patch.attack_ms,patch.release_ms);gf::apply_model_d_preset_preview(*model_,p);break;
            case 1:juno_->set_parameters(cutoff,q,patch.attack_ms,patch.release_ms);gf::apply_juno_preset_preview(*juno_,p);break;
            case 2:gf::apply_prophet_preset_preview(*bus_,p);break;
            case 3:ob6_->set_parameters(cutoff,q,patch.attack_ms,patch.release_ms);gf::apply_ob6_preset_preview(*ob6_,p);break;
            case 4:gf::apply_dx7_preset_preview(*dx_,p);break;
            case 7:{
                auto voice=config.six;
                // DRIFT affects per-operator detune; raw patch/automation remains unchanged.
                const int drift=int(std::lround(studio::macro_amount(p.patch,GF_MACRO_DRIFT)*2));
                const float grit=studio::macro_amount(p.patch,GF_MACRO_GRIT)*studio::macro_amount(p.patch,GF_MACRO_GRIT_MOD);
                const float pocket=studio::macro_amount(p.patch,GF_MACRO_POCKET);
                const float velocity=pocket*studio::macro_amount(p.patch,GF_MACRO_POCKET_VEL);
                const auto& route=gf::dx7six::algorithms[voice.vced[134]];
                for(int op=0;op<6;++op) {
                    auto* o=voice.vced.data()+op*21;
                    o[20]=uint8_t(std::clamp(int(o[20])+(op%2?drift:-drift),0,14));
                    if(route.ops[op]&3)o[16]=uint8_t(std::min(99,int(std::lround(o[16]+20*grit))));
                    o[1]=uint8_t(std::lround(o[1]+(99-o[1])*pocket));
                    o[3]=uint8_t(std::lround(o[3]+(99-o[3])*pocket));
                    o[6]=uint8_t(std::lround(o[6]*(1-pocket)));
                    o[15]=uint8_t(std::min(7,int(std::lround(o[15]+4*velocity))));
                }
                voice.vced[135]=uint8_t(std::min(7,int(std::lround(voice.vced[135]+3*grit))));
                six_->edit_patch(voice);
                filter_g_=std::tan(float(3.141592653589793)*std::min(cutoff,20000.f)/48000);
                filter_k_=1.f/std::max(.707f,.7f+2.3f*res);
                break;
            }
            case 5:arp_->set_parameters(cutoff,q,patch.attack_ms,patch.release_ms);arp_->set_patch(gf::make_arp2600_patch(p));arp_->set_ring_mix(std::clamp(patch.osc[0][2]*.01f,0.f,1.f));break;
        }
        if(id_!=2) { auto effects=p;effects.patch=studio::move_sfx(p.patch,move);gf::apply_synth_fx_preview(*bus_,effects,uint8_t(bus_id));gf::apply_groove_preview(*bus_,p,false); }
    }
    void note(uint8_t n,float v) noexcept override { if(hybrid_) { if(v>0)hybrid_->note_on(n,v);else hybrid_->note_off(n); } else { if(v>0)bus_->note_on(n,v);else bus_->note_off(n); } }
    void sustain(bool down) noexcept override { if(six_)six_->set_sustain(down); if(hybrid_) hybrid_->set_sustain(down);else bus_->set_sustain(down); }
    void bend(int value) noexcept override {
        switch(id_) {case 0:model_->set_pitch_bend(value);break;case 1:juno_->set_pitch_bend(value);break;case 2:bus_->set_pitch_bend(value);break;case 3:ob6_->set_pitch_bend(value);break;case 4:dx_->set_pitch_bend(value);break;case 5:arp_->set_pitch_bend(value);break;case 6:hybrid_->set_pitch_bend(value);break;case 7:six_bend_=value;six_expression();break;}
    }
    void frame(float& l,float& r) noexcept override {
        l=r=0;
        if(hybrid_) hybrid_->process_frame(l,r);
        else if(id_==2) bus_->process_frame(l,r);
        else {
            const float tape=bus_->tape_pitch_ratio(); float input=0;
            switch(id_) {
                case 0:model_->set_tape_pitch(tape);input=model_->process_sample();break;
                case 1:{float jl,jr;juno_->set_tape_pitch(tape);juno_->process_frame(jl,jr);bus_->process_external_stereo(jl,jr,l,r);break;}
                case 3:ob6_->set_tape_pitch(tape);input=ob6_->process_sample();break;
                case 4:dx_->set_tape_pitch(tape);input=dx_->process_sample();break;
                case 7:{
                    if(tape!=six_tape_){six_tape_=tape;six_expression();}
                    const float x=six_->process_sample();
                    // TPT state-variable post-filter: the DX7 core itself has no filter.
                    const float a=1.f/(1.f+filter_g_*(filter_g_+filter_k_));
                    const float b=a*(band_+filter_g_*(x-low_));
                    const float l=low_+filter_g_*b;band_=2*b-band_;low_=2*l-low_;input=l;
                    break;
                }
                case 5:arp_->set_tape_pitch(tape);input=arp_->process_sample();break;
            }
            if(id_!=1) bus_->process_external_frame(input,l,r);
        }
        float* channels[]={&l,&r};
        for(int i=0;i<2;++i) { const float x=*channels[i];const float y=x-dc_x_[size_t(i)]+dc_*dc_y_[size_t(i)];dc_x_[size_t(i)]=x;dc_y_[size_t(i)]=y;*channels[i]=y; }
    }
};
std::unique_ptr<Engine> create_preview(const Config& c) {return std::make_unique<PreviewEngine>(c);}
}
const std::vector<Registration>& registry() {
    static const std::vector<Registration> r={{"prophet",2,create_preview,preview_defaults,preview_catalog,preview_presets,preview_preset_name,preview_encode,preview_decode},{"dx7",4,create_preview,preview_defaults,preview_catalog,preview_presets,preview_preset_name,preview_encode,preview_decode},{"juno",1,create_preview,preview_defaults,preview_catalog,preview_presets,preview_preset_name,preview_encode,preview_decode},{"ob6",3,create_preview,preview_defaults,preview_catalog,preview_presets,preview_preset_name,preview_encode,preview_decode},{"model_d",0,create_preview,preview_defaults,preview_catalog,preview_presets,preview_preset_name,preview_encode,preview_decode},{"arp2600",5,create_preview,preview_defaults,preview_catalog,preview_presets,preview_preset_name,preview_encode,preview_decode},{"dx7_6op",7,create_preview,six_defaults,six_catalog,six_presets,six_preset_name,six_encode,six_decode},{"hybrid",6,create_preview,preview_defaults,preview_catalog,preview_presets,preview_preset_name,preview_encode,preview_decode}};
    return r;
}
std::unique_ptr<Engine> make_engine(const Config& c) { return registry().at(size_t(c.engine)).create(c); }
}
