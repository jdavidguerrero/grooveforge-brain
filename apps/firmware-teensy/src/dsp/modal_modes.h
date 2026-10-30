#pragma once

namespace dsp {
/** @brief Frecuencia relativa y T60 base de un modo resonante. */
struct ModalMode { float freqRatio; float t60Base; };
/** @brief Datos modales de CAMPANA, GUADUA, CRISTAL y MADERA. */
constexpr ModalMode MODAL_MODES[4][6] = {
    {{1.0f,8.0f},{2.756f,6.0f},{5.404f,4.0f},{8.933f,3.0f},{13.46f,2.0f},{18.02f,1.5f}},
    {{1.0f,3.0f},{2.757f,2.5f},{5.404f,1.8f},{8.933f,1.2f},{13.34f,0.8f},{17.81f,0.5f}},
    {{1.0f,12.0f},{2.442f,10.0f},{3.908f,8.0f},{5.983f,6.0f},{8.547f,4.0f},{11.41f,2.0f}},
    {{1.0f,1.5f},{2.756f,1.2f},{5.404f,0.9f},{8.933f,0.6f},{13.34f,0.4f},{17.81f,0.2f}}
};
}
