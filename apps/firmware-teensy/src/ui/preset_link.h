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
    enum Kind : uint8_t { None = 0, Hello, Export, Put, Trunc } kind = None;
    uint8_t engine = 0, index = 0;   ///< PUT: engine e índice · TRUNC: engine y cantidad que queda
    const char* hex = nullptr;       ///< PUT: el preset en hex (apunta dentro de la línea)
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
    else if (!strcmp(tok[0], "PUT") && nt == 4 && num(tok[1], &c.engine) && num(tok[2], &c.index)) {
        c.kind = LinkCmd::Put;
        c.hex = tok[3];
    } else if (!strcmp(tok[0], "TRUNC") && nt == 3 && num(tok[1], &c.engine) && num(tok[2], &c.index)) {
        c.kind = LinkCmd::Trunc;
    }
    return c;
}

}  // namespace studio
