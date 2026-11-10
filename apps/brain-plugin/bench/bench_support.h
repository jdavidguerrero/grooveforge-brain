#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(__SSE2__)
#include <xmmintrin.h>
#endif

namespace bench {

/** @brief Configura denormales por hilo y restaura el estado del llamador al salir. */
class FlushToZero {
public:
    FlushToZero() {
#if defined(__aarch64__) || defined(__arm64__)
        asm volatile("mrs %0, fpcr" : "=r"(previous_));
        uint64_t value = previous_ | (uint64_t(1) << 24);
        asm volatile("msr fpcr, %0\n\tisb" : : "r"(value) : "memory");
        asm volatile("mrs %0, fpcr" : "=r"(active_));
#elif defined(__SSE2__)
        previous_ = _mm_getcsr();
        _mm_setcsr(unsigned(previous_) | (1u << 15) | (1u << 6));
        active_ = _mm_getcsr();
#else
        throw std::runtime_error("FTZ requires aarch64 FPCR or x86 SSE2 MXCSR");
#endif
        volatile float normal = std::numeric_limits<float>::min();
        volatile float half = 0.5f;
        volatile float output = normal * half;
        volatile float subnormal = std::numeric_limits<float>::denorm_min();
        volatile float input = subnormal * 2.0f;
        verified_ = output == 0.0f && input == 0.0f;
    }
    ~FlushToZero() {
#if defined(__aarch64__) || defined(__arm64__)
        asm volatile("msr fpcr, %0\n\tisb" : : "r"(previous_) : "memory");
#elif defined(__SSE2__)
        _mm_setcsr(unsigned(previous_));
#endif
    }
    FlushToZero(const FlushToZero&) = delete;
    FlushToZero& operator=(const FlushToZero&) = delete;
    /** @brief Comprueba underflow de salida y tratamiento de entradas subnormales. */
    bool verified() const { return verified_; }
    /** @brief Registro activo, sin ocultar flags de excepción previos. */
    uint64_t control() const { return active_; }
private:
    uint64_t previous_ = 0, active_ = 0;
    bool verified_ = false;
};

inline std::string quote(const std::string& input) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : input) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setfill('0') << std::setw(4) << unsigned(c);
        else out << c;
    }
    out << '"';
    return out.str();
}

inline uint32_t rotate(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }

/** @brief SHA-256 FIPS 180-4 de bytes; sin dependencia de utilidades del sistema. */
inline std::string sha256(const std::vector<uint8_t>& bytes) {
    constexpr uint32_t k[] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    std::array<uint32_t,8> h{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    auto data = bytes;
    const uint64_t bits = uint64_t(data.size()) * 8;
    data.push_back(0x80);
    while (data.size() % 64 != 56) data.push_back(0);
    for (int i = 7; i >= 0; --i) data.push_back(uint8_t(bits >> (i * 8)));
    for (size_t offset = 0; offset < data.size(); offset += 64) {
        uint32_t w[64]{};
        for (unsigned i = 0; i < 16; ++i)
            for (unsigned b = 0; b < 4; ++b) w[i] = (w[i] << 8) | data[offset + i * 4 + b];
        for (unsigned i = 16; i < 64; ++i) {
            const auto a = w[i-15], b = w[i-2];
            w[i] = w[i-16] + (rotate(a,7) ^ rotate(a,18) ^ (a >> 3)) + w[i-7]
                + (rotate(b,17) ^ rotate(b,19) ^ (b >> 10));
        }
        auto a=h[0], b=h[1], c=h[2], d=h[3], e=h[4], f=h[5], g=h[6], z=h[7];
        for (unsigned i = 0; i < 64; ++i) {
            const uint32_t t1 = z + (rotate(e,6)^rotate(e,11)^rotate(e,25)) + ((e&f)^(~e&g)) + k[i] + w[i];
            const uint32_t t2 = (rotate(a,2)^rotate(a,13)^rotate(a,22)) + ((a&b)^(a&c)^(b&c));
            z=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=z;
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (auto x : h) out << std::setw(8) << x;
    return out.str();
}

inline void little_endian(std::ostream& stream, uint32_t value, unsigned count) {
    for (unsigned b = 0; b < count; ++b) stream.put(char((value >> (b * 8)) & 255));
}

/** @brief WAV PCM16 estéreo con bytes definidos explícitamente en little endian. */
inline void write_wav(const std::string& path, const std::vector<uint8_t>& pcm, unsigned rate) {
    std::ofstream out(path, std::ios::binary);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out.write("RIFF", 4); little_endian(out, uint32_t(pcm.size()) + 36, 4); out.write("WAVEfmt ", 8);
    little_endian(out, 16, 4); little_endian(out, 1, 2); little_endian(out, 2, 2);
    little_endian(out, rate, 4); little_endian(out, rate * 4, 4);
    little_endian(out, 4, 2); little_endian(out, 16, 2); out.write("data", 4);
    little_endian(out, uint32_t(pcm.size()), 4);
    out.write(reinterpret_cast<const char*>(pcm.data()), std::streamsize(pcm.size()));
    out.close();
}

/** @brief Vectores conocidos independientes del renderer para validar el hash. */
inline void self_test() {
    const std::array<std::string,3> inputs{"", "abc", std::string(1000000, 'a')};
    const std::array<std::string,3> expected{
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"};
    for (size_t i = 0; i < inputs.size(); ++i)
        if (sha256(std::vector<uint8_t>(inputs[i].begin(), inputs[i].end())) != expected[i])
            throw std::runtime_error("SHA-256 self-test failed");
}
} // namespace bench
