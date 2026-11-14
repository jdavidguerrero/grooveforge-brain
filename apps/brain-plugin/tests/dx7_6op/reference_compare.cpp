#include "dx7_6op_preview.h"
#include "synth.h"
#include "controllers.h"
#include "dx7note.h"
#include "freqlut.h"
#include "exp2.h"
#include "sin.h"
#include "pitchenv.h"
#include "lfo.h"
#include "env.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>

namespace six = gf::dx7six;
int main(int argc, char** argv) {
    const auto dir = std::filesystem::path(argc > 1 ? argv[1] : ".");
    std::filesystem::create_directories(dir);
    Sin::init(); Exp2::init();
    bool ok = true;
    std::cout << "fs,case,note,rms_db_difference,relative_rms_error,max_error\n";
    for (double fs : {44100.0, 48000.0}) {
        Freqlut::init(fs); PitchEnv::init(fs); Lfo::init(fs);
        // Frozen stage timings for native regressions, produced by the unmodified oracle.
        if (fs == 44100) for (int rate : {30,60,90,99}) {
            int rates[] = {rate,rate,rate,rate}, levels[] = {99,80,65,0};
            Env reference{}; six::msfa::Env adapted{}; adapted.prepare(fs);
            reference.init(rates, levels, 127 * 32, 0); adapted.init(rates, levels, 127 * 32, 0);
            int previous = 0;
            for (int tick = 0; tick < 300000; ++tick) {
                if (tick == 150000) { reference.keydown(false); adapted.keydown(false); }
                const int32_t a = reference.getsample(), b = adapted.getsample();
                if (a != b) { std::cerr << "EG mismatch " << rate << ' ' << tick << '\n'; ok = false; break; }
                if (adapted.stage() != previous) {
                    std::cerr << "EG rate=" << rate << " stage=" << adapted.stage() << " tick=" << tick + 1 << '\n'; previous = adapted.stage();
                }
            }
        }
        for (int kind = 0; kind < 7; ++kind) for (int note : {48,60,84}) {
            auto p = six::init_patch();
            p.vced[134] = kind == 0 ? 31 : kind == 1 ? 4 : kind == 2 ? 15 : kind == 3 ? 3 : kind == 4 ? 5 : 0;
            p.vced[135] = kind == 0 ? 0 : 5;
            if (kind == 5) { p.vced[137] = 70; p.vced[139] = 65; p.vced[143] = 5; p.vced[130] = 65; }
            if (kind == 6) { p.vced[140] = 99; }
            for (int op = 0; op < 6; ++op) {
                p.vced[op*21+16] = kind == 0 && op != 5 ? 0 : (op & 1) ? 96 : 68;
                p.vced[op*21+18] = (op & 1) ? 1 : 3;
                p.vced[op*21+13] = 2; p.vced[op*21+15] = 3;
                if (kind == 6) p.vced[op*21+14] = 3;
            }
            gf::Dx7SixPreview engine; engine.prepare(fs); engine.set_patch(p); engine.note_on(uint8_t(note),100.0f/127);
            Dx7Note reference{}; reference.init(reinterpret_cast<const char*>(p.vced.data()), note, 100);
            Lfo lfo{}; lfo.reset(reinterpret_cast<const char*>(p.vced.data()+137)); lfo.keydown();
            Controllers ctrl{}; ctrl.values_[kControllerPitch] = 8192;
            const int blocks = int(fs * 1.5) / 64;
            std::vector<float> original, actual;
            original.reserve(blocks*64); actual.reserve(blocks*64);
            double ea=0,eb=0,ed=0,maxerr=0;
            for (int block = 0; block < blocks; ++block) {
                if (block == int(fs)/64) { reference.keyup(); engine.note_off(uint8_t(note)); }
                int32_t buffer[64] = {};
                reference.compute(buffer, lfo.getsample(), lfo.getdelay(), &ctrl);
                for (int i = 0; i < 64; ++i) {
                    const float a = buffer[i] * (0.125f / (1 << 24));
                    const float b = engine.process_sample();
                    original.push_back(a); actual.push_back(b);
                    ea+=a*a; eb+=b*b; ed+=(a-b)*(a-b); maxerr=std::max(maxerr,double(std::abs(a-b)));
                }
            }
            const double db = 10*std::log10(eb/ea), err = std::sqrt(ed/ea);
            std::cout << int(fs) << ',' << kind << ',' << note << ',' << std::setprecision(9) << db << ',' << err << ',' << maxerr << '\n';
            if ((kind != 3 && kind != 4 && kind != 6) &&
                (std::abs(db) > (fs == 44100 ? .001 : .2) || err > (fs == 44100 ? .001 : .16))) ok = false;
            const auto prefix = std::to_string(int(fs))+"_"+std::to_string(kind)+"_"+std::to_string(note);
            std::ofstream(dir/(prefix+"_msfa.f32"),std::ios::binary).write(reinterpret_cast<char*>(original.data()),original.size()*sizeof(float));
            std::ofstream(dir/(prefix+"_gf.f32"),std::ios::binary).write(reinterpret_cast<char*>(actual.data()),actual.size()*sizeof(float));
        }
    }
    return ok ? 0 : 1;
}
