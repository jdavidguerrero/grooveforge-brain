#include "bench_support.h"
#include "build_info.h"
#include "hybrid_preview.h"
#include "prophet_preset.h"
#include "dx7_preset.h"
#include "dx7_6op_preview.h"
#include "juno_preset.h"
#include "ob6_preset.h"
#include "model_d_preset.h"
#include "arp2600_preset.h"
#include "ui/groove_macros.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <numeric>

namespace {
constexpr unsigned RATE = 48000;
constexpr double WARMUP = 2.0;
constexpr unsigned FIXTURE_FRAMES = 3 * RATE;
const std::array<std::string, 8> ENGINES{"prophet", "dx7", "juno", "ob6", "model-d", "arp2600", "hybrid", "dx7_6op"};

int engine_id(const std::string& name) {
    if (name == "model-d") return 0;
    if (name == "juno") return 1;
    if (name == "prophet") return 2;
    if (name == "ob6") return 3;
    if (name == "dx7") return 4;
    if (name == "arp2600") return 5;
    if (name == "hybrid") return 6;
    if (name == "dx7_6op") return 7;
    throw std::runtime_error("Unknown engine: " + name);
}
unsigned max_voices(const std::string& name) {
    constexpr unsigned counts[] = {1, 6, 5, 6, 6, 4, 6, 6};
    return counts[engine_id(name)];
}

// A common workload isolates voice count from note multiplication by ARP/CHORD/UNISON.
void configure_workload(studio::Preset& p, int id, bool wet) {
    for (auto& enabled : p.patch.groove_on) enabled = false;
    p.patch.groove_on[GF_GROOVE_PUMP] = wet;
    auto& pump = p.patch.groove[GF_GROOVE_PUMP];
    pump[GF_PUMP_DEPTH] = 35; pump[GF_PUMP_SHAPE] = 0; pump[GF_PUMP_RATE] = 0;
    pump[GF_PUMP_OFFSET] = 0; pump[GF_PUMP_RELEASE] = 180;
    p.patch.release_ms = std::min(p.patch.release_ms, 100.0f);
    p.patch.f_release_ms = std::min(p.patch.f_release_ms, 100.0f);
    if (id == 4) for (auto& op : p.patch.osc) op[7] = std::min(op[7], 100.0f);
    if (id == 1 && !wet) { p.patch.osc[0][4] = 0; p.patch.mod[0][1] = 0; }
    for (unsigned fx = 0; fx < GF_SFX_COUNT; ++fx) p.patch.sfx_on[fx] = wet;
    const float mixes[] = {.20f, .20f, .25f, .20f, .20f};
    for (unsigned fx = 0; fx < GF_SFX_COUNT; ++fx) p.patch.sfx_mix[fx] = mixes[fx];
    p.patch.sfx_p[GF_SFX_DRIVE][0] = .35f;
    p.patch.sfx_p[GF_SFX_DRIVE][1] = .55f;
    p.patch.sfx_p[GF_SFX_DRIVE][2] = 0;
    p.patch.sfx_p[GF_SFX_TAPE][0] = .30f;
    p.patch.sfx_p[GF_SFX_TAPE][1] = .20f;
    p.patch.sfx_p[GF_SFX_TAPE][2] = .15f;
    p.patch.sfx_p[GF_SFX_CHORUS][0] = .50f;
    p.patch.sfx_p[GF_SFX_CHORUS][1] = .30f;
    p.patch.sfx_p[GF_SFX_CHORUS][2] = 1; // PHASER in the shared slot.
    p.patch.sfx_p[GF_SFX_DELAY][0] = .50f;
    p.patch.sfx_p[GF_SFX_DELAY][1] = .35f;
    p.patch.sfx_p[GF_SFX_DELAY][2] = .55f;
    p.patch.sfx_p[GF_SFX_REVERB][0] = .55f;
    p.patch.sfx_p[GF_SFX_REVERB][1] = 1; // PLATE = index 4 of five types.
    p.patch.sfx_p[GF_SFX_REVERB][2] = .50f;
}

class Engine {
public:
    Engine(std::string name, unsigned voices, unsigned root, bool wet)
        : name_(std::move(name)), id_(engine_id(name_)), voices_(voices), root_(root) {
        if (id_ == 7) {
            six_ = std::make_unique<gf::Dx7SixPreview>(); six_->prepare(RATE);
            auto p = gf::dx7six::init_patch();
            p.vced[134] = 4; p.vced[135] = 5;
            for (int op = 0; op < 6; ++op) {
                p.vced[op * 21 + 16] = (op & 1) ? 90 : 72;
                p.vced[op * 21 + 18] = (op & 1) ? 1 : 3;
                p.vced[op * 21 + 15] = 3;
            }
            six_->set_patch(p); preset_ = "GF SIX PAIRS";
            bus_ = std::make_unique<gf::ProphetPreview>(); bus_->prepare(RATE);
            delay_l_.assign(RATE + 2, 0); delay_r_.assign(RATE + 2, 0);
            plate_.assign(gf::PlateReverbPreview::required_samples(RATE), 0);
            bus_->set_delay_memory(delay_l_.data(), delay_r_.data(), RATE + 2);
            bus_->set_plate_memory(plate_.data(), plate_.size());
            studio::Preset effects; studio::factory_preset(2, 0, &effects);
            configure_workload(effects, 2, wet);
            gf::apply_synth_fx_preview(*bus_, effects, 2);
            bus_->set_voice_sink(this,
                [](void* ctx, uint8_t n, float v) { static_cast<Engine*>(ctx)->voice_on(n,v); },
                [](void* ctx, uint8_t n) { static_cast<Engine*>(ctx)->voice_off(n); });
        } else if (id_ == 6) {
            studio::LayerPreset layer;
            if (!studio::factory_layer(7, &layer)) throw std::runtime_error("Factory layer 7 missing");
            configure_workload(layer.a, layer.eng_a, wet);
            configure_workload(layer.b, layer.eng_b, wet);
            preset_ = std::string(layer.name) + ": " + layer.a.name + " + " + layer.b.name;
            hybrid_ = std::make_unique<gf::HybridPreview>(); hybrid_->prepare(RATE);
            if (!hybrid_->load(layer)) throw std::runtime_error("HYBRID load failed");
            hybrid_->set_transport(120, false, 0, true);
            hybrid_->set_expression(0, 0, 120, false);
        } else {
            studio::Preset p;
            if (!studio::factory_preset(uint8_t(id_), 0, &p)) throw std::runtime_error("Factory preset 0 missing");
            preset_ = p.name;
            configure_workload(p, id_, wet);
            bus_ = std::make_unique<gf::ProphetPreview>(); bus_->prepare(RATE);
            delay_l_.assign(RATE + 2, 0); delay_r_.assign(RATE + 2, 0);
            plate_.assign(gf::PlateReverbPreview::required_samples(RATE), 0);
            bus_->set_delay_memory(delay_l_.data(), delay_r_.data(), RATE + 2);
            bus_->set_plate_memory(plate_.data(), plate_.size());
            const auto patch = studio::macro_patch(p.patch, uint8_t(id_));
            const float move = studio::macro_amount(p.patch, GF_MACRO_MOVE);
            const float cutoff = std::clamp(p.cutoff_hz * studio::move_cutoff_mul(move), GF_CUTOFF_RANGE[id_].lo, GF_CUTOFF_RANGE[id_].hi);
            const float res = studio::move_resonance(studio::macro_resonance(p.reso_norm, p.patch), move);
            const float q = std::clamp(id_ == 3 ? .7f + 4.3f * res : .1f + 4.9f * res, .707f, 5.0f);
            switch (id_) {
                case 0:
                    model_ = std::make_unique<gf::ModelDPreview>(); model_->prepare(RATE);
                    model_->set_parameters(cutoff, q, patch.attack_ms, patch.release_ms);
                    gf::apply_model_d_preset_preview(*model_, p); model_->reset();
                    model_->set_modulation_control(0, 0, 120, false); break;
                case 1:
                    juno_ = std::make_unique<gf::JunoPreview>(); juno_->prepare(RATE);
                    juno_->set_parameters(cutoff, q, patch.attack_ms, patch.release_ms);
                    gf::apply_juno_preset_preview(*juno_, p); juno_->reset();
                    juno_->set_lfo_control(0, 120, false); break;
                case 2: gf::apply_prophet_preset_preview(*bus_, p); bus_->reset(); break;
                case 3:
                    ob6_ = std::make_unique<gf::Ob6Preview>(); ob6_->prepare(RATE);
                    ob6_->set_parameters(cutoff, q, patch.attack_ms, patch.release_ms);
                    gf::apply_ob6_preset_preview(*ob6_, p); ob6_->reset();
                    ob6_->set_lfo_control(0, 120, false); break;
                case 4:
                    dx7_ = std::make_unique<gf::Dx7Preview>(); dx7_->prepare(RATE);
                    gf::apply_dx7_preset_preview(*dx7_, p); dx7_->reset();
                    dx7_->set_lfo_control(0, 120, false); break;
                case 5:
                    arp_ = std::make_unique<gf::Arp2600Preview>(); arp_->prepare(RATE);
                    arp_->set_parameters(cutoff, q, patch.attack_ms, patch.release_ms);
                    arp_->set_patch(gf::make_arp2600_patch(p));
                    arp_->set_ring_mix(std::clamp(patch.osc[0][2] * .01f, 0.0f, 1.0f)); arp_->reset();
                    arp_->set_lfo_control(0, 0, 120, false); break;
            }
            if (id_ != 2) {
                bus_->set_voice_sink(this,
                    [](void* ctx, uint8_t n, float v) { static_cast<Engine*>(ctx)->voice_on(n, v); },
                    [](void* ctx, uint8_t n) { static_cast<Engine*>(ctx)->voice_off(n); });
                auto effects = p;
                effects.patch = studio::move_sfx(p.patch, move);
                gf::apply_synth_fx_preview(*bus_, effects, uint8_t(id_));
                gf::apply_groove_preview(*bus_, p, false);
            }
            bus_->set_transport(120, false, 0, true);
        }
        dc_coefficient_ = float(std::exp(-2.0 * 3.14159265358979323846 * 5.0 / RATE));
    }
    const std::string& preset() const { return preset_; }
    const std::string& name() const { return name_; }
    unsigned voices() const { return voices_; }
    unsigned root() const { return root_; }
    void frame(uint64_t sample, float& l, float& r) {
        const uint64_t cycle = sample / (2 * RATE), phase = sample % (2 * RATE);
        constexpr int shifts[] = {0, 2, -2, 5};
        constexpr unsigned offsets[] = {0, 3, 7, 10, 12, 15};
        const auto root = unsigned(int(root_) + shifts[cycle % 4]);
        if (phase == 0 || phase == RATE * 3 / 2) {
            for (unsigned v = 0; v < voices_; ++v) {
                const auto note = uint8_t(root + offsets[v]);
                if (phase == 0) {
                    if (hybrid_) hybrid_->note_on(note, .78f - .035f * float(v));
                    else bus_->note_on(note, .78f - .035f * float(v));
                } else {
                    if (hybrid_) hybrid_->note_off(note);
                    else bus_->note_off(note);
                }
            }
        }
        if (sample % (RATE / 2) == 0) {
            const int bend = ((sample / (RATE / 2)) % 2) ? 8704 : 8192;
            switch (id_) {
                case 0: model_->set_pitch_bend(bend); break;
                case 1: juno_->set_pitch_bend(bend); break;
                case 2: bus_->set_pitch_bend(bend); break;
                case 3: ob6_->set_pitch_bend(bend); break;
                case 4: dx7_->set_pitch_bend(bend); break;
                case 5: arp_->set_pitch_bend(bend); break;
                case 6: hybrid_->set_pitch_bend(bend); break;
                case 7: six_->set_pitch_bend(bend); break;
            }
        }
        l = r = 0;
        if (hybrid_) hybrid_->process_frame(l, r);
        else if (id_ == 2) bus_->process_frame(l, r);
        else {
            const float tape = bus_->tape_pitch_ratio();
            float input = 0;
            switch (id_) {
                case 0: model_->set_tape_pitch(tape); input = model_->process_sample(); break;
                case 1: {
                    float jl, jr; juno_->set_tape_pitch(tape); juno_->process_frame(jl, jr);
                    bus_->process_external_stereo(jl, jr, l, r); break;
                }
                case 3: ob6_->set_tape_pitch(tape); input = ob6_->process_sample(); break;
                case 4: dx7_->set_tape_pitch(tape); input = dx7_->process_sample(); break;
                case 5: arp_->set_tape_pitch(tape); input = arp_->process_sample(); break;
                case 7: input = six_->process_sample(); break;
            }
            if (id_ != 1) bus_->process_external_frame(input, l, r);
        }
        float* channels[] = {&l, &r};
        for (unsigned c = 0; c < 2; ++c) {
            const float x = *channels[c];
            const float y = x - dc_x_[c] + dc_coefficient_ * dc_y_[c];
            dc_x_[c] = x; dc_y_[c] = y; *channels[c] = y;
        }
    }
private:
    void voice_on(uint8_t n, float v) {
        switch (id_) {
            case 0: model_->note_on(n,v); break; case 1: juno_->note_on(n,v); break;
            case 3: ob6_->note_on(n,v); break; case 4: dx7_->note_on(n,v); break; case 5: arp_->note_on(n,v); break; case 7: six_->note_on(n,v); break;
        }
    }
    void voice_off(uint8_t n) {
        switch (id_) {
            case 0: model_->note_off(n); break; case 1: juno_->note_off(n); break;
            case 3: ob6_->note_off(n); break; case 4: dx7_->note_off(n); break; case 5: arp_->note_off(n); break; case 7: six_->note_off(n); break;
        }
    }
    std::string name_, preset_;
    int id_;
    unsigned voices_, root_;
    std::vector<int16_t> delay_l_, delay_r_, plate_;
    std::unique_ptr<gf::ProphetPreview> bus_;
    std::unique_ptr<gf::ModelDPreview> model_;
    std::unique_ptr<gf::JunoPreview> juno_;
    std::unique_ptr<gf::Ob6Preview> ob6_;
    std::unique_ptr<gf::Dx7Preview> dx7_;
    std::unique_ptr<gf::Dx7SixPreview> six_;
    std::unique_ptr<gf::Arp2600Preview> arp_;
    std::unique_ptr<gf::HybridPreview> hybrid_;
    float dc_coefficient_ = 0;
    std::array<float,2> dc_x_{}, dc_y_{};
};

using Parts = std::vector<std::unique_ptr<Engine>>;
Parts make_parts(const std::string& engine, unsigned voices, bool wet) {
    Parts parts;
    if (engine == "four-part") {
        parts.emplace_back(std::make_unique<Engine>("model-d", 1, 36, wet));
        parts.emplace_back(std::make_unique<Engine>("juno", 6, 48, wet));
        parts.emplace_back(std::make_unique<Engine>("ob6", 4, 60, wet));
        parts.emplace_back(std::make_unique<Engine>("hybrid", 6, 48, wet));
    } else parts.emplace_back(std::make_unique<Engine>(engine, voices, engine == "model-d" ? 36 : 48, wet));
    return parts;
}

void render(Parts& parts, uint64_t start, unsigned frames, std::vector<float>& out) {
    std::fill(out.begin(), out.end(), 0.0f);
    const float gain = parts.size() == 4 ? .25f : 1.0f;
    for (auto& part : parts) {
        for (unsigned i = 0; i < frames; ++i) {
            float l, r; part->frame(start + i, l, r);
            out[2*i] += gain * l; out[2*i+1] += gain * r;
        }
    }
}

struct AudioStats {
    double squares = 0, peak = 0;
    uint64_t samples = 0, nonfinite = 0, clipped = 0;
    void inspect(const std::vector<float>& out, unsigned frames) {
        for (size_t i = 0; i < frames * 2; ++i) {
            const double value = out[i];
            ++samples;
            if (!std::isfinite(value)) { ++nonfinite; continue; }
            peak = std::max(peak, std::abs(value)); squares += value * value;
            if (value > 1.0 || value < -1.0) ++clipped;
        }
    }
    void validate() const {
        if (nonfinite || squares <= 1e-16) throw std::runtime_error("Non-finite or silent audio; refusing timing result");
    }
    std::string json() const {
        std::ostringstream out;
        out << std::setprecision(12) << "{\"peak\":" << peak << ",\"rms\":" << std::sqrt(squares / double(samples))
            << ",\"nonfinite_samples\":" << nonfinite << ",\"clipped_samples\":" << clipped << '}';
        return out.str();
    }
};

std::string parts_json(const Parts& parts) {
    std::ostringstream out; out << '[';
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out << ',';
        const auto& p = parts[i];
        out << "{\"engine\":" << bench::quote(p->name()) << ",\"held_notes\":" << p->voices()
            << ",\"root\":" << p->root() << ",\"preset\":" << bench::quote(p->preset()) << '}';
    }
    out << ']'; return out.str();
}

struct Result {
    std::string engine, variant, parts;
    unsigned voices = 0, block = 0;
    uint64_t frames = 0, blocks = 0, misses = 0;
    double wall = 0, mean = 0, p99 = 0, max = 0;
    bool gate = false;
    AudioStats audio;
    std::string json() const {
        std::ostringstream out; out << std::setprecision(12);
        out << "{\"engine\":" << bench::quote(engine) << ",\"variant\":" << bench::quote(variant)
            << ",\"held_notes\":" << voices << ",\"block_frames\":" << block << ",\"parts\":" << parts
            << ",\"frames\":" << frames << ",\"audio_seconds\":" << double(frames) / RATE
            << ",\"blocks\":" << blocks << ",\"render_wall_seconds\":" << wall
            << ",\"real_time_factor\":" << mean / 100 << ",\"core_percent_mean\":" << mean
            << ",\"core_percent_p99\":" << p99 << ",\"core_percent_max\":" << max
            << ",\"deadline_misses\":" << misses << ",\"timing_gate_pass\":" << (gate ? "true" : "false")
            << ",\"audio\":" << audio.json() << '}'; return out.str();
    }
};

Result measure(const std::string& engine, unsigned voices, bool wet, unsigned block, double seconds) {
    auto parts = make_parts(engine, voices, wet);
    Result r; r.engine = engine; r.voices = voices; r.variant = wet ? "wet" : "dry"; r.block = block;
    r.parts = parts_json(parts);
    r.blocks = uint64_t(std::ceil(seconds * RATE / block)); r.frames = r.blocks * block;
    std::vector<float> output(block * 2);
    std::vector<double> durations(size_t(r.blocks), 0);
    uint64_t position = 0;
    while (position < uint64_t(WARMUP * RATE)) {
        const unsigned n = unsigned(std::min<uint64_t>(block, uint64_t(WARMUP * RATE) - position));
        render(parts, position, n, output); position += n;
    }
    using Clock = std::chrono::steady_clock;
    static_assert(Clock::is_steady, "Benchmark requires a monotonic clock");
    for (uint64_t b = 0; b < r.blocks; ++b) {
        const auto begin = Clock::now();
        render(parts, position, block, output);
        const auto end = Clock::now();
        durations[size_t(b)] = std::chrono::duration<double>(end - begin).count();
        r.audio.inspect(output, block); // Validation and I/O are outside the timed render.
        position += block;
    }
    r.audio.validate();
    r.wall = std::accumulate(durations.begin(), durations.end(), 0.0);
    r.mean = 100 * r.wall / (double(r.frames) / RATE);
    const double deadline = double(block) / RATE;
    r.misses = uint64_t(std::count_if(durations.begin(), durations.end(), [=](double d) { return d > deadline; }));
    std::sort(durations.begin(), durations.end());
    const size_t rank = size_t(std::ceil(.99 * double(durations.size()))) - 1;
    r.p99 = 100 * durations[rank] / deadline; r.max = 100 * durations.back() / deadline;
    r.gate = seconds >= 20 && r.mean <= 50 && r.p99 <= 70 && r.max <= 90 && r.misses == 0;
    return r;
}

struct Fixture {
    std::string engine, variant, file, hash;
    unsigned block = 0, voices = 0;
    AudioStats audio;
    std::string json() const {
        std::ostringstream out;
        out << "{\"engine\":" << bench::quote(engine) << ",\"variant\":" << bench::quote(variant)
            << ",\"held_notes\":" << voices << ",\"block_frames\":" << block << ",\"wav\":" << bench::quote(file)
            << ",\"frames\":" << FIXTURE_FRAMES << ",\"pcm_sha256\":" << bench::quote(hash)
            << ",\"audio\":" << audio.json() << '}'; return out.str();
    }
};
Fixture fixture(const std::string& engine, unsigned voices, bool wet, unsigned block, const std::filesystem::path& folder) {
    Fixture f; f.engine = engine; f.voices = voices; f.variant = wet ? "wet" : "dry"; f.block = block;
    f.file = engine + "-" + f.variant + "-b" + std::to_string(block) + "-v" + std::to_string(voices) + ".wav";
    auto parts = make_parts(engine, voices, wet);
    std::vector<float> output(2 * block);
    std::vector<uint8_t> pcm; pcm.reserve(FIXTURE_FRAMES * 4);
    for (uint64_t start = 0; start < FIXTURE_FRAMES; start += block) {
        const unsigned n = unsigned(std::min<uint64_t>(block, FIXTURE_FRAMES - start));
        render(parts, start, n, output); f.audio.inspect(output, n);
        for (size_t i = 0; i < 2 * n; ++i) {
            if (!std::isfinite(output[i])) throw std::runtime_error("Non-finite fixture");
            const auto value = int16_t(std::lround(std::clamp(output[i], -1.0f, 1.0f) * 32767.0f));
            const auto bits = uint16_t(value);
            pcm.push_back(uint8_t(bits & 255)); pcm.push_back(uint8_t(bits >> 8));
        }
    }
    f.audio.validate(); f.hash = bench::sha256(pcm);
    bench::write_wav((folder / f.file).string(), pcm, RATE);
    return f;
}

double number(const std::string& text) {
    size_t end = 0;
    const double value = std::stod(text, &end);
    if (end != text.size() || !std::isfinite(value)) throw std::runtime_error("Invalid number: " + text);
    return value;
}
} // namespace

int main(int argc, char** argv) {
    try {
        double seconds = 20;
        std::vector<unsigned> blocks{128, 256};
        std::vector<std::string> engines(ENGINES.begin(), ENGINES.end()); engines.push_back("four-part");
        std::filesystem::path json_path = "gemelo-pi-results/results.json";
        bool self_test_only = false;
        for (int a = 1; a < argc; ++a) {
            const std::string option = argv[a];
            if (option == "--help") {
                std::cout << "gf_engine_bench [--seconds 20] [--block 128|256] [--engine prophet|dx7|juno|ob6|model-d|arp2600|hybrid|dx7_6op|four-part]\n"
                    "                [--json path] [--threads 1] [--self-test]\n"
                    "Defaults: full matrix, dry+wet, 48 kHz, 2 s warm-up/case; WAV and PCM SHA-256 beside JSON.\n"
                    "RTF = render wall / audio duration (lower is better). Timing gate is provisional; no realtime host.\n";
                return 0;
            }
            if (option == "--self-test") { self_test_only = true; continue; }
            if (a + 1 >= argc) throw std::runtime_error("Missing value for " + option);
            const std::string value = argv[++a];
            if (option == "--seconds") {
                seconds = number(value);
                if (seconds <= 0 || seconds > 3600) throw std::runtime_error("--seconds must be in (0,3600]");
            } else if (option == "--block") {
                if (value != "128" && value != "256") throw std::runtime_error("--block must be 128 or 256");
                blocks = {unsigned(std::stoul(value))};
            } else if (option == "--engine") {
                if (value != "four-part") engine_id(value);
                engines = {value};
            } else if (option == "--json") json_path = value;
            else if (option == "--threads") {
                if (value != "1") throw std::runtime_error("Only --threads 1 is supported");
            } else throw std::runtime_error("Unknown option: " + option);
        }
        bench::FlushToZero ftz;
        if (!ftz.verified()) throw std::runtime_error("FTZ/denormal input verification failed");
        bench::self_test();
        if (self_test_only) { std::cout << "SHA-256 vectors and FTZ input/output: PASS\n"; return 0; }
        auto folder = json_path.parent_path();
        if (folder.empty()) folder = ".";
        std::filesystem::create_directories(folder);
        std::vector<Result> results;
        std::vector<Fixture> fixtures;
        std::cout << "48 kHz | 1 thread | " << seconds << " s/case + 2 s warm-up | " << GF_BENCH_OPT
            << " | " << GF_BENCH_COMPILER << "\nFlags: " << GF_BENCH_FLAGS << '\n';
        std::cout << "engine       mode notes block    RTF    mean%     p99%     max%  misses\n";
        for (const auto block : blocks) for (const auto& engine : engines) {
            const auto maximum = engine == "four-part" ? 0 : max_voices(engine);
            std::vector<unsigned> counts = engine == "four-part" ? std::vector<unsigned>{0}
                : std::vector<unsigned>{1, std::min(4u, maximum), maximum};
            std::sort(counts.begin(), counts.end()); counts.erase(std::unique(counts.begin(), counts.end()), counts.end());
            for (const bool wet : {false, true}) {
                for (const auto voices : counts) {
                    auto r = measure(engine, voices, wet, block, seconds);
                    std::cout << std::left << std::setw(13) << r.engine << std::setw(5) << r.variant
                        << std::right << std::setw(5) << (engine == "four-part" ? "mix" : std::to_string(voices))
                        << std::setw(6) << block << std::fixed << std::setprecision(3) << std::setw(8) << r.mean / 100
                        << std::setprecision(2) << std::setw(9) << r.mean << std::setw(9) << r.p99 << std::setw(9) << r.max
                        << std::setw(8) << r.misses << std::endl;
                    results.push_back(std::move(r));
                }
                auto f = fixture(engine, maximum, wet, block, folder);
                std::cout << "PCM_SHA256 " << f.hash << "  " << f.file << std::endl;
                fixtures.push_back(std::move(f));
            }
        }
        const auto selected = [&](const std::string& e, unsigned b, bool wet) {
            return std::any_of(results.begin(), results.end(), [&](const Result& r) { return r.engine == e && r.block == b && r.variant == (wet ? "wet" : "dry"); });
        };
        bool complete = true, timing_pass = std::string(GF_BENCH_BUILD_TYPE) == "Release";
        for (unsigned block : {128u,256u}) for (bool wet : {false,true}) complete &= selected("four-part", block, wet);
        for (const auto& r : results) if (r.engine == "four-part") timing_pass &= r.gate;
        std::ofstream json(json_path); json.exceptions(std::ios::badbit | std::ios::failbit);
        json << "{\n  \"schema_version\":1,\n  \"workload_version\":\"factory0-retrobrass7-v2\",\n"
            << "  \"compiler\":" << bench::quote(GF_BENCH_COMPILER) << ",\n  \"system\":" << bench::quote(GF_BENCH_SYSTEM)
            << ",\n  \"build_type\":" << bench::quote(GF_BENCH_BUILD_TYPE) << ",\n  \"optimization\":" << bench::quote(GF_BENCH_OPT)
            << ",\n  \"flags\":" << bench::quote(GF_BENCH_FLAGS) << ",\n  \"git_revision\":" << bench::quote(GF_BENCH_REVISION)
            << ",\n  \"git_dirty_at_configure\":" << bench::quote(GF_BENCH_DIRTY)
            << ",\n  \"source_manifest_sha256\":" << bench::quote(GF_BENCH_SOURCE_HASH)
            << ",\n  \"sample_rate\":48000,\n  \"threads\":1,\n  \"warmup_audio_seconds\":2,\n  \"requested_audio_seconds\":" << seconds
            << ",\n  \"ftz_verified\":true,\n  \"fp_control_register\":" << ftz.control()
            << ",\n  \"fixture_format\":\"stereo PCM16 little-endian; SHA-256 of data chunk only; gain 1 per engine, 0.25 per mixed part\",\n"
            << "  \"full_four_part_matrix\":" << (complete ? "true" : "false")
            << ",\n  \"provisional_timing_gate_pass\":" << (complete && timing_pass && std::string(GF_BENCH_BUILD_TYPE) == "Release" ? "true" : "false")
            << ",\n  \"hardware_gate\":\"pending Pi repeats, thermal evidence, realtime I/O, latency and listening\",\n  \"results\":[\n";
        for (size_t i = 0; i < results.size(); ++i) json << (i ? ",\n" : "") << "    " << results[i].json();
        json << "\n  ],\n  \"fixtures\":[\n";
        for (size_t i = 0; i < fixtures.size(); ++i) json << (i ? ",\n" : "") << "    " << fixtures[i].json();
        json << "\n  ]\n}\n"; json.close();
        std::ofstream hashes(folder / "pcm-sha256.txt"); hashes.exceptions(std::ios::badbit | std::ios::failbit);
        for (const auto& f : fixtures) hashes << f.hash << "  " << f.file << '\n';
        hashes.close();
        std::cout << "JSON: " << json_path << "\nProvisional 4-part timing gate: "
            << (complete ? (timing_pass ? "PASS" : "FAIL") : "INCOMPLETE")
            << "; hardware decision remains pending Pi and realtime tests.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "gf_engine_bench: " << e.what() << '\n'; return 1;
    }
}
