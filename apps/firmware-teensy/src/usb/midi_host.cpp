// apps/firmware-teensy/src/usb/midi_host.cpp
// MidiHost — USB-A host MIDI, Sprint 4.1
//
// Librería: USBHost_t36 (PaulStoffregen, incluida con Teensyduino — custom, no oficial Audio)

#include "midi_host.h"

MidiHost::MidiHost()
    : _usb()
    , _midi(_usb)
{
    // _midi se auto-registra en _usb al construirse (patrón USBHost_t36).
    // No se necesita configuración adicional aquí.
}

void MidiHost::init() {
    _usb.begin();
    // USBHost_t36 no requiere configuración de pines adicional — los pads D+/D− del
    // Teensy 4.1 están conectados al controlador USB2 del SoC en hardware.
    // Pin 39 (USB host mode select) se controla internamente por USBHost_t36.
}

void MidiHost::poll() {
    _usb.Task();

    // Detectar eventos de conexión/desconexión.
    // MIDIDevice hereda operator bool() — true cuando el device está enumerado.
    bool connected_now = (bool)_midi;
    if (connected_now && !_was_connected) {
        Serial.print(F("[MidiHost] device connected: "));
        const char* name = device_name();
        if (name && name[0] != '\0') {
            Serial.println(name);
        } else {
            Serial.println(F("(name not available)"));
        }
    } else if (!connected_now && _was_connected) {
        Serial.println(F("[MidiHost] device disconnected"));
    }
    _was_connected = connected_now;

    // Consumir todos los mensajes MIDI disponibles en este tick de loop().
    // midi1.read() retorna true mientras haya mensajes pendientes — agotamos
    // el buffer completo para no acumular latencia entre ticks.
    while (_midi.read()) {
        uint8_t type    = _midi.getType();
        uint8_t channel = _midi.getChannel();   // 1–16
        uint8_t data1   = _midi.getData1();
        uint8_t data2   = _midi.getData2();

        switch (type) {
            case usbMIDI.NoteOn:
                // MIDI running status: NoteOn con velocity 0 equivale a NoteOff.
                // El Standard MIDI 1.0 spec §2.1 lo define para optimizar banwidth.
                // Despachamos el NoteOn con velocity 0 tal cual — el callback del
                // caller es responsable de interpretarlo como NoteOff si lo requiere.
                if (_cb_note_on) {
                    _cb_note_on(channel, data1, data2);
                }
                break;

            case usbMIDI.NoteOff:
                if (_cb_note_off) {
                    _cb_note_off(channel, data1, data2);
                }
                break;

            case usbMIDI.ControlChange:
                if (_cb_cc) {
                    _cb_cc(channel, data1, data2);
                }
                break;

            case usbMIDI.PitchBend: {
                // USB MIDI Event Packet: data1 = LSB, data2 = MSB del valor 14-bit.
                // Rango raw: 0x0000–0x3FFF. Centro: 0x2000 (8192).
                // Convertimos a signed: -8192 a +8191.
                int16_t bend = (int16_t)(((uint16_t)data2 << 7) | data1) - 8192;
                if (_cb_pitch_bend) {
                    _cb_pitch_bend(channel, bend);
                }
                break;
            }

            // Ignoramos: AfterTouch, SysEx, MTC, Song Position, etc.
            // Agregar cases aquí si se necesitan en sprints futuros.
            default:
                break;
        }
    }
}

void MidiHost::on_note_on(void (*cb)(uint8_t channel, uint8_t note, uint8_t velocity)) {
    _cb_note_on = cb;
}

void MidiHost::on_note_off(void (*cb)(uint8_t channel, uint8_t note, uint8_t velocity)) {
    _cb_note_off = cb;
}

void MidiHost::on_cc(void (*cb)(uint8_t channel, uint8_t cc, uint8_t value)) {
    _cb_cc = cb;
}

void MidiHost::on_pitch_bend(void (*cb)(uint8_t channel, int16_t bend)) {
    _cb_pitch_bend = cb;
}

bool MidiHost::is_connected() {
    // MIDIDevice hereda operator bool() de USBDriver/MIDIDeviceBase —
    // true cuando el device está enumerado. No es const en USBHost_t36.
    return (bool)_midi;
}

const char* MidiHost::device_name() {
    if (!(bool)_midi) {
        return "";
    }
    // USBHost_t36: USBDriver::product() retorna const uint8_t* (descriptor USB).
    // Casteamos a const char* — ambos son byte strings; el contenido es UTF-8/ASCII.
    // Algunos dispositivos tienen el descriptor de producto vacío o con basura.
    const uint8_t* name = _midi.product();
    return (name != nullptr) ? reinterpret_cast<const char*>(name) : "";
}
