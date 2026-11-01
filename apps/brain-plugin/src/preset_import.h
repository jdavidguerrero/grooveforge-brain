#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gf {

/** @brief Interfaz de importación fuera del hilo de audio para presets GFP admitidos. */
class PresetImport {
public:
    virtual ~PresetImport() = default;
    /** @return false si los bytes no contienen un preset válido de engine admitido. */
    virtual bool import_preset(const void* data, size_t size) = 0;
    /** @brief Reemplaza una mitad HYBRID con un GFP válido; side es 0 para A o 1 para B. */
    virtual bool import_hybrid_half(const void* data, size_t size, unsigned side) = 0;
    /** @brief Exporta la capa HYBRID como GFL v2. */
    virtual bool export_hybrid_preset(std::vector<uint8_t>& data) = 0;
    /** @brief Exporta Juno v5 si el estado puede representarse en GFP. */
    virtual bool export_juno_preset(std::vector<uint8_t>& data) = 0;
    /** @brief Exporta un GFP OB-6 cargado con sus controles editados. */
    virtual bool export_ob6_preset(std::vector<uint8_t>& data) = 0;
    /** @brief Exporta el GFP Model D cargado con los controles disponibles. */
    virtual bool export_model_d_preset(std::vector<uint8_t>& data) = 0;
    /** @brief Exporta el GFP ARP 2600 cargado con los controles disponibles. */
    virtual bool export_arp2600_preset(std::vector<uint8_t>& data) = 0;
    /** @brief Exporta DX7 v5 si el estado puede representarse en GFP. */
    virtual bool export_dx7_preset(std::vector<uint8_t>& data) = 0;
};

}  // namespace gf
