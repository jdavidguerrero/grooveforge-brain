#pragma once

// USBHost_t36 — incluida con Teensyduino (PaulStoffregen/USBHost_t36)
// No requiere entrada en lib_deps — el LDF de Teensyduino la resuelve via #include.
#include <USBHost_t36.h>

/**
 * @brief Controlador de USB-A host MIDI para GrooveForge Brain (Nivel 1).
 *
 * Envuelve USBHost + MIDIDevice de la librería USBHost_t36 con una interfaz
 * orientada a callbacks, ocultando el detalle de la state machine del bus USB.
 *
 * El USB-A host del Teensy 4.1 es hardware separado del puerto device (micro-USB).
 * El device usa el periférico UOTG0 (Cortex-M7 interno). El host usa pads dedicados
 * D+/D− en la cara inferior del PCB. No comparten silicio.
 *
 * Ref: apps/docs/01-architecture.md §3.3 — pin 39: "USB host mode select"
 *      apps/docs/01-architecture.md §5.2 — USB MIDI → audio out latencia <5ms p95
 *
 * Nivel de superpoderes: Nivel 1 — + Teclado MIDI (USB-A host)
 * apps/docs/01-architecture.md §1 — North Star
 */
class MidiHost {
public:
    MidiHost();

    /**
     * @brief Inicializa el controlador USB host y el objeto MIDIDevice.
     *
     * Debe llamarse en setup() antes del primer poll(). Internamente llama
     * myusb.begin() que activa el controlador USB host del Teensy 4.1.
     *
     * No es necesario llamarla más de una vez — USBHost_t36 maneja la
     * re-enumeración automáticamente cuando se desconecta y reconecta un device.
     */
    void init();

    /**
     * @brief Procesa eventos USB y despacha callbacks MIDI — llamar en cada loop().
     *
     * Internamente invoca:
     *   1. myusb.Task()  — avanza la state machine del bus USB (enumeration,
     *                       transfers en curso, detach events). No puede estar
     *                       en una ISR: hace allocations y llama user callbacks.
     *   2. midi1.read()  — consume los USB MIDI Event Packets pendientes y
     *                       despacha los callbacks registrados.
     *
     * Non-blocking: no usa delay() ni while(!available()). Retorna en <1ms en
     * ausencia de tráfico MIDI pesado.
     */
    void poll();

    /**
     * @brief Registra callback para Note On.
     *
     * @param cb Puntero a función: void cb(uint8_t channel, uint8_t note, uint8_t velocity)
     *           channel: 1–16 (convención MIDI — USBHost_t36 ya ajusta de 0-based)
     *           note:    0–127 (60 = C4, 69 = A4)
     *           velocity: 0–127 (velocity 0 en NoteOn equivale a NoteOff en MIDI —
     *                    el caller debe manejar este caso)
     */
    void on_note_on(void (*cb)(uint8_t channel, uint8_t note, uint8_t velocity));

    /**
     * @brief Registra callback para Note Off.
     *
     * @param cb Puntero a función: void cb(uint8_t channel, uint8_t note, uint8_t velocity)
     */
    void on_note_off(void (*cb)(uint8_t channel, uint8_t note, uint8_t velocity));

    /**
     * @brief Registra callback para Control Change.
     *
     * CCs relevantes para GrooveForge Brain:
     *   cc  0 — Bank Select: cambia engine activo (0=Moog, 1=Juno, 2=Prophet)
     *   cc 71 — Resonance (GM2 controller 71 "Timbre/Harmonic Intensity")
     *   cc 74 — Brightness / Cutoff (GM2 controller 74 "Brightness")
     *
     * @param cb Puntero a función: void cb(uint8_t channel, uint8_t cc, uint8_t value)
     *           value: 0–127
     */
    void on_cc(void (*cb)(uint8_t channel, uint8_t cc, uint8_t value));

    /**
     * @brief Registra callback para Pitch Bend.
     *
     * @param cb Puntero a función: void cb(uint8_t channel, int16_t bend)
     *           bend: -8192 (full down) a +8191 (full up). 0 = center.
     *           El rango de semitones depende del Pitch Bend Range (RPN 0) del synth.
     *           Por defecto: ±2 semitones (estándar GM).
     */
    void on_pitch_bend(void (*cb)(uint8_t channel, int16_t bend));

    /**
     * @brief Retorna true si hay un dispositivo MIDI USB conectado y enumerado.
     *
     * Usa el operator bool() de MIDIDevice de USBHost_t36, que retorna true cuando
     * el device está enumerated y listo para recibir/enviar datos.
     */
    bool is_connected();

    /**
     * @brief Retorna el nombre del dispositivo conectado, si está disponible.
     *
     * USBHost_t36 expone el product string del descriptor USB. En algunos dispositivos
     * de bajo costo este string está vacío o es inválido — el caller debe manejar
     * el caso de string vacío.
     *
     * No es const: USBDriver::operator bool() y product() no son métodos const
     * en la API de USBHost_t36 — el estado de conexión puede mutar en cualquier llamada.
     *
     * @return Puntero a string constante. Válido mientras el dispositivo esté conectado.
     *         Retorna "" si no hay dispositivo o el nombre no está disponible.
     */
    const char* device_name();

private:
    // USBHost_t36 — custom (PaulStoffregen/USBHost_t36, incluida con Teensyduino)
    // myusb es el controlador del bus. Debe ser único en el programa.
    // midi1 registra sus callbacks en myusb al construirse.
    USBHost    _usb;
    MIDIDevice _midi;

    // Callbacks registrados por el caller. nullptr = no registrado.
    void (*_cb_note_on)(uint8_t, uint8_t, uint8_t)  = nullptr;
    void (*_cb_note_off)(uint8_t, uint8_t, uint8_t) = nullptr;
    void (*_cb_cc)(uint8_t, uint8_t, uint8_t)       = nullptr;
    void (*_cb_pitch_bend)(uint8_t, int16_t)        = nullptr;

    // Estado previo de conexión para detectar connect/disconnect events.
    bool _was_connected = false;
};
