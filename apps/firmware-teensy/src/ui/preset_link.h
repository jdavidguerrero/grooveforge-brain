#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file preset_link.h
 * @brief Comandos del bibliotecario de presets por el USB serie (variants/studio/theory/11), lógica pura.
 *
 * Líneas de texto que empiezan con "@GF "; el binario de un preset viaja en hexadecimal.
 */
namespace studio {

/// `n` bytes → 2n caracteres hex en mayúscula + '\0'. `out` necesita 2n + 1 lugares.
inline void hex_encode(const uint8_t* in, size_t n, char* out) {
    static const char D[] = "0123456789ABCDEF";
    for (size_t i = 0; i < n; ++i) {
        out[2 * i] = D[in[i] >> 4];
        out[2 * i + 1] = D[in[i] & 0xF];
    }
    out[2 * n] = '\0';
}

/// Hex (mayúscula o minúscula, sin espacios) → bytes. @return cuántos bytes, o 0 si es impar, inválido o no cabe.
inline size_t hex_decode(const char* s, uint8_t* out, size_t cap) {
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    const size_t len = strlen(s);
    if (len == 0 || len % 2 || len / 2 > cap) return 0;
    for (size_t i = 0; i < len / 2; ++i) {
        const int hi = nib(s[2 * i]), lo = nib(s[2 * i + 1]);
        if (hi < 0 || lo < 0) return 0;
        out[i] = uint8_t(hi << 4 | lo);
    }
    return len / 2;
}

struct LinkCmd {
    /// LEXPORT / LPUT / LTRUNC: presets de capa de HYBRID SYNTH (theory/12 §3d)
    enum Kind : uint8_t { None = 0, Hello, Export, Put, Trunc, Fs, Format, LayerExport, LayerPut, LayerTrunc, Ruta, Cue, Usb, In, Hp, Cv } kind = None;
    /// RUTA (0 = synths, 1 = entrada de línea → USB) y CUE (0/1): prototipo de la consola (apps/docs/consola/05-e3-teensy-a.md)
    uint8_t valor = 0;
    uint16_t cv = 0; ///< Volatile MCP4725 value, 0..4095.
    uint8_t engine = 0, index = 0;   ///< PUT: engine e índice · TRUNC: engine y cantidad que queda · LPUT / LTRUNC: index
    const char* hex = nullptr;       ///< PUT / LPUT: el binario en hex (apunta dentro de la línea)
};

/// Parsea una línea (sin el '\n'; se modifica: los espacios pasan a '\0'). Lo que no es "@GF …" da None.
inline LinkCmd parse_link_cmd(char* line) {
    LinkCmd c;
    size_t n = strlen(line);
    while (n && (line[n - 1] == '\r' || line[n - 1] == ' ')) line[--n] = '\0';
    if (strncmp(line, "@GF ", 4) != 0) return c;
    char* tok[4] = {};
    int nt = 0;
    for (char* p = line + 4; *p && nt < 4;) {
        while (*p == ' ') ++p;
        if (!*p) break;
        tok[nt++] = p;
        while (*p && *p != ' ') ++p;
        if (*p) *p++ = '\0';
    }
    if (nt == 0) return c;
    auto num = [](const char* s, uint8_t* out) {
        char* end = nullptr;
        const long v = strtol(s, &end, 10);
        if (!s[0] || *end || v < 0 || v > 255) return false;
        *out = uint8_t(v);
        return true;
    };
    if (!strcmp(tok[0], "HELLO") && nt == 1) c.kind = LinkCmd::Hello;
    else if (!strcmp(tok[0], "EXPORT") && nt == 1) c.kind = LinkCmd::Export;
    else if (!strcmp(tok[0], "USB") && nt == 1) c.kind = LinkCmd::Usb;
    else if (!strcmp(tok[0], "FS") && nt == 1) c.kind = LinkCmd::Fs;
    else if (!strcmp(tok[0], "FORMAT") && nt == 2 && !strcmp(tok[1], "YES")) c.kind = LinkCmd::Format;   // borra todo: pide YES
    else if (!strcmp(tok[0], "PUT") && nt == 4 && num(tok[1], &c.engine) && num(tok[2], &c.index)) {
        c.kind = LinkCmd::Put;
        c.hex = tok[3];
    } else if (!strcmp(tok[0], "TRUNC") && nt == 3 && num(tok[1], &c.engine) && num(tok[2], &c.index)) {
        c.kind = LinkCmd::Trunc;
    } else if (!strcmp(tok[0], "LEXPORT") && nt == 1) {
        c.kind = LinkCmd::LayerExport;
    } else if (!strcmp(tok[0], "LPUT") && nt == 3 && num(tok[1], &c.index)) {
        c.kind = LinkCmd::LayerPut;
        c.hex = tok[2];
    } else if (!strcmp(tok[0], "RUTA") && nt == 2 && (!strcmp(tok[1], "SINTE") || !strcmp(tok[1], "LINEA"))) {
        c.kind = LinkCmd::Ruta;
        c.valor = !strcmp(tok[1], "LINEA") ? 1 : 0;
    } else if (!strcmp(tok[0], "CUE") && nt == 2 && (!strcmp(tok[1], "0") || !strcmp(tok[1], "1"))) {
        c.kind = LinkCmd::Cue;
        c.valor = tok[1][0] == '1' ? 1 : 0;
    } else if (nt == 2 && (!strcmp(tok[0], "IN") || !strcmp(tok[0], "HP")) && num(tok[1], &c.valor)) {
        if (!strcmp(tok[0], "IN") && c.valor <= 15) c.kind = LinkCmd::In;
        if (!strcmp(tok[0], "HP") && c.valor <= 127) c.kind = LinkCmd::Hp;
    } else if (nt == 2 && !strcmp(tok[0], "CV")) {
        char* end = nullptr;
        const long value = strtol(tok[1], &end, 10);
        if (tok[1][0] && !*end && value >= 0 && value <= 4095) {
            c.kind = LinkCmd::Cv;
            c.cv = uint16_t(value);
        }
    } else if (!strcmp(tok[0], "LTRUNC") && nt == 2 && num(tok[1], &c.index)) {
        c.kind = LinkCmd::LayerTrunc;
    }
    return c;
}

}  // namespace studio
