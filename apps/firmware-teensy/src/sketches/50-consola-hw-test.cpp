/**
 * Sketch 50 · Prueba de hardware de la consola (Teensy única, `17-mapa-pines-teensy.md`).
 *
 * Para probar el montaje de la protoboard por partes, mientras se conecta: busca los dispositivos I2C e imprime
 * en vivo encoders, botones de los MCP23017, FSR, perilla MASTER, ADS1115 y bytes de la NeoTrellis M4.
 * Mantiene el SGTL5000 activo con un tono opcional en audífonos para comprobar que el I2C de la superficie no lo
 * afecta. No es el firmware de la consola: no manda MIDI ni audio USB.
 *
 * Comandos por el monitor serie (115200): `s` busca I2C · `t` tono 440 Hz en audífonos sí/no · `?` ayuda.
 *
 * Teensy Audio Library oficial: AudioSynthWaveformSine, AudioOutputI2S, AudioControlSGTL5000.
 * Lectura de MCP23017 y ADS1115 por registros: código propio (sin dependencias).
 */
#include <Arduino.h>
#include <Audio.h>
#include <Encoder.h>
#include <Wire.h>

namespace {

AudioSynthWaveformSine tone_;
AudioOutputI2S i2s_out;
AudioConnection c_l(tone_, 0, i2s_out, 0);
AudioConnection c_r(tone_, 0, i2s_out, 1);
AudioControlSGTL5000 sgtl;

// Pines del mapa 17 §1 (prototipo: encoders 1–4 y la rueda jog).
Encoder enc[] = {{2, 3}, {4, 5}, {6, 9}, {10, 11}, {37, 40}};
const char* const ENC_NAME[] = {"ENC1", "ENC2", "ENC3", "ENC4", "JOG"};
constexpr uint8_t ENC_COUNT = sizeof(enc) / sizeof(enc[0]);
int32_t enc_last[ENC_COUNT] = {};

constexpr uint8_t FSR_PINS[] = {14, 15};
constexpr uint8_t POT_MASTER = 22;
constexpr uint8_t MCP_ADDR[] = {0x20, 0x21};
constexpr uint8_t ADS_ADDR = 0x48;
constexpr uint8_t ADS_CHANNELS[] = {0, 2};  // A0 fader, A2 Sharp

bool mcp_ok[2] = {};
uint16_t mcp_last[2] = {0xFFFF, 0xFFFF};
uint32_t i2c_errors = 0;
int fsr_peak[2] = {};
bool fsr_on[2] = {};
int pot_last = -100;
int16_t ads_last[2] = {-30000, -30000};
bool ads_ok = false;
bool tone_on = false;
bool analog_on = false;  // `a`: imprime FSR y MASTER (apagado: un pin al aire llena la pantalla)
float pot_avg = 0;

const char* i2c_name(uint8_t a) {
    switch (a) {
        case 0x0A: return "SGTL5000";
        case 0x20: return "MCP23017 #1";
        case 0x21: return "MCP23017 #2";
        case 0x22: return "MCP23017 #3";
        case 0x48: return "ADS1115 #1";
        case 0x49: return "ADS1115 #2";
        case 0x60: case 0x61: return "MCP4725";
        default: return "?";
    }
}

void scan_other(TwoWire& w, const char* label) {
    // Diagnóstico de cableado: si los módulos aparecen aquí, están en los pines equivocados.
    w.begin();
    for (uint8_t a = 1; a < 127; ++a) {
        w.beginTransmission(a);
        if (w.endTransmission() == 0) Serial.printf("  OJO 0x%02X en %s (debería ir a 18/19)\n", a, label);
    }
    w.end();
}

void i2c_scan() {
    Serial.println("I2C:");
    uint8_t found = 0;
    for (uint8_t a = 1; a < 127; ++a) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  0x%02X  %s\n", a, i2c_name(a));
            ++found;
        }
    }
    if (!found) Serial.println("  nada: revisar SDA 18 / SCL 19, 3,3 V y GND");
    scan_other(Wire1, "Wire1 (SDA 17 / SCL 16)");
    scan_other(Wire2, "Wire2 (SDA 25 / SCL 24)");
}

bool write_reg16(uint8_t addr, uint8_t reg, uint16_t value) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(uint8_t(value >> 8));
    Wire.write(uint8_t(value));
    return Wire.endTransmission() == 0;
}

bool mcp_write(uint8_t addr, uint8_t reg, uint8_t a, uint8_t b) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(a);
    Wire.write(b);
    return Wire.endTransmission() == 0;
}

bool mcp_setup(uint8_t i) {
    // IODIRA/B = 0xFF (entradas) y GPPUA/B = 0xFF (pull-ups internas): el botón va a GND.
    return mcp_write(MCP_ADDR[i], 0x00, 0xFF, 0xFF) && mcp_write(MCP_ADDR[i], 0x0C, 0xFF, 0xFF);
}

bool mcp_read(uint8_t i, uint16_t& bits) {
    Wire.beginTransmission(MCP_ADDR[i]);
    Wire.write(0x12);  // GPIOA, luego GPIOB
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(MCP_ADDR[i], uint8_t(2)) != 2) return false;
    const uint8_t a = Wire.read();
    bits = uint16_t(a) | uint16_t(Wire.read()) << 8;
    return true;
}

bool ads_read(uint8_t channel, int16_t& value) {
    // Single-shot, MUX AINx–GND, ±4,096 V, 860 SPS.
    const uint16_t config = 0x8000 | uint16_t(0x4 + channel) << 12 | 0x0200 | 0x0100 | 0x00E0 | 0x0003;
    if (!write_reg16(ADS_ADDR, 0x01, config)) return false;
    delayMicroseconds(1300);
    Wire.beginTransmission(ADS_ADDR);
    Wire.write(0x00);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(ADS_ADDR, uint8_t(2)) != 2) return false;
    value = int16_t(uint16_t(Wire.read()) << 8 | Wire.read());
    return true;
}

// Protocolo binario de la M4 (processor/include/shared/BinaryProtocol.h): [0xAA][CMD][LEN][PAYLOAD][XOR de CMD, LEN y
// payload]. La M4 ignora sus teclas hasta recibir la rejilla completa de colores (CMD_LED_GRID_UPDATE 0xA0, 96 bytes).
void m4_send(uint8_t cmd, const uint8_t* payload, uint8_t len) {
    uint8_t x = cmd ^ len;
    for (uint8_t i = 0; i < len; ++i) x ^= payload[i];
    Serial7.write(0xAA); Serial7.write(cmd); Serial7.write(len);
    Serial7.write(payload, len);
    Serial7.write(x);
}

void m4_grid_init() {
    uint8_t rgb[96];
    for (uint8_t i = 0; i < 32; ++i) {  // colores 7 bits, tenues: una fila por color
        const uint8_t row = i / 8;
        rgb[i * 3 + 0] = row == 0 || row == 3 ? 24 : 0;
        rgb[i * 3 + 1] = row == 1 || row == 3 ? 24 : 0;
        rgb[i * 3 + 2] = row == 2 || row == 3 ? 24 : 0;
    }
    m4_send(0xA0, rgb, sizeof(rgb));
    Serial.println("M4: rejilla enviada (deben encenderse los pads)");
}

void help() {
    Serial.println("\n== GrooveForge consola · prueba de hardware ==");
    Serial.println("s: buscar I2C · t: tono 440 Hz · a: FSR y MASTER · g: iniciar rejilla de la M4 · ?: ayuda");
}

}  // namespace

void setup() {
    Serial.begin(115200);
    AudioMemory(8);
    sgtl.enable();
    sgtl.volume(0.5f);
    tone_.frequency(440);
    tone_.amplitude(0);
    Wire.begin();
    Wire.setClock(400000);
    Serial7.begin(1000000);  // NeoTrellis M4
    // Pull-down interno (~100 kΩ): sin nada conectado el pin lee 0 en vez de ruido; con el pot de 10 kΩ o el
    // divisor del FSR el error es despreciable para esta prueba.
    for (uint8_t p : FSR_PINS) pinMode(p, INPUT_PULLDOWN);
    pinMode(POT_MASTER, INPUT_PULLDOWN);
    analogReadAveraging(16);
    delay(1500);
    help();
    i2c_scan();
    for (uint8_t i = 0; i < 2; ++i) {
        mcp_ok[i] = mcp_setup(i);
        Serial.printf("MCP 0x%02X: %s\n", MCP_ADDR[i], mcp_ok[i] ? "listo" : "no responde");
    }
    int16_t probe;
    ads_ok = ads_read(0, probe);
    Serial.printf("ADS 0x%02X: %s\n", ADS_ADDR, ads_ok ? "listo" : "no responde");
}

void loop() {
    static uint32_t last_mcp = 0, last_ads = 0, last_retry = 0;
    const uint32_t now = millis();

    while (Serial.available()) {
        const char c = Serial.read();
        if (c == 's') i2c_scan();
        else if (c == 't') { tone_on = !tone_on; tone_.amplitude(tone_on ? 0.2f : 0.0f); Serial.printf("tono %s\n", tone_on ? "ON" : "OFF"); }
        else if (c == 'g') m4_grid_init();
        else if (c == 'a') { analog_on = !analog_on; Serial.printf("analógicos %s\n", analog_on ? "ON" : "OFF"); }
        else if (c == '?') help();
    }

    for (uint8_t i = 0; i < ENC_COUNT; ++i) {
        const int32_t detents = enc[i].read() / 4;
        if (detents != enc_last[i]) {
            Serial.printf("%s %+ld  (total %ld)\n", ENC_NAME[i], long(detents - enc_last[i]), long(detents));
            enc_last[i] = detents;
        }
    }

    if (now - last_mcp >= 5) {
        last_mcp = now;
        for (uint8_t i = 0; i < 2; ++i) {
            if (!mcp_ok[i]) continue;
            uint16_t bits;
            if (!mcp_read(i, bits)) { ++i2c_errors; mcp_ok[i] = false; Serial.printf("MCP 0x%02X perdido (errores I2C %lu)\n", MCP_ADDR[i], (unsigned long)i2c_errors); continue; }
            const uint16_t changed = bits ^ mcp_last[i];
            for (uint8_t b = 0; b < 16; ++b) {
                if (changed & (1u << b)) {
                    Serial.printf("MCP%u GP%c%u %s\n", i + 1, b < 8 ? 'A' : 'B', b % 8, (bits >> b) & 1 ? "suelto" : "PULSADO");
                }
            }
            mcp_last[i] = bits;
        }
    }
    if (now - last_retry >= 2000) {
        last_retry = now;
        for (uint8_t i = 0; i < 2; ++i) if (!mcp_ok[i] && mcp_setup(i)) { mcp_ok[i] = true; Serial.printf("MCP 0x%02X listo\n", MCP_ADDR[i]); }
        if (!ads_ok) { int16_t v; if (ads_read(0, v)) { ads_ok = true; Serial.println("ADS listo"); } }
    }

    for (uint8_t i = 0; analog_on && i < 2; ++i) {
        const int v = analogRead(FSR_PINS[i]);
        if (!fsr_on[i] && v > 160) { fsr_on[i] = true; fsr_peak[i] = v; }
        else if (fsr_on[i]) {
            if (v > fsr_peak[i]) fsr_peak[i] = v;
            if (v < 100) { fsr_on[i] = false; Serial.printf("FSR%u golpe pico %d / 1023\n", i + 1, fsr_peak[i]); }
        }
    }

    pot_avg += (analogRead(POT_MASTER) - pot_avg) * 0.05f;
    const int pot = int(pot_avg);
    if (analog_on && abs(pot - pot_last) > 24) { pot_last = pot; Serial.printf("MASTER %d / 1023\n", pot); }

    if (ads_ok && now - last_ads >= 60) {
        last_ads = now;
        for (uint8_t i = 0; i < 2; ++i) {
            int16_t v;
            if (!ads_read(ADS_CHANNELS[i], v)) { ++i2c_errors; ads_ok = false; Serial.println("ADS perdido"); break; }
            if (abs(v - ads_last[i]) > 800) {  // ~0,1 V: una entrada al aire flota ±0,06 V
                ads_last[i] = v;
                Serial.printf("ADS A%u %d (%.2f V)\n", ADS_CHANNELS[i], v, v * 4.096f / 32768.0f);
            }
        }
    }

    static uint32_t m4_bytes = 0, last_m4 = 0;
    static uint8_t m4_buf[8]; static uint8_t m4_n = 0;
    while (Serial7.available()) {
        const uint8_t b = Serial7.read(); ++m4_bytes;
        if (m4_n == 0 && b != 0xAA) continue;
        m4_buf[m4_n++] = b;
        if (m4_n >= 3 && m4_n == m4_buf[2] + 4) {
            if (m4_buf[1] == 0x62 && m4_buf[2] >= 2)
                Serial.printf("M4 pad track %u scene %u %s\n", m4_buf[3], m4_buf[4], m4_buf[2] >= 3 && !m4_buf[5] ? "suelto" : "PULSADO");
            else Serial.printf("M4 trama cmd 0x%02X len %u\n", m4_buf[1], m4_buf[2]);
            m4_n = 0;
        } else if (m4_n >= sizeof(m4_buf)) m4_n = 0;
    }
    if (m4_bytes != last_m4 && now % 500 < 5) { Serial.printf("M4: %lu bytes recibidos\n", (unsigned long)m4_bytes); last_m4 = m4_bytes; }
}
