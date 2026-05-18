# Sprint 3.3 — Encoders, Buttons y LEDs (Teensy 4.1)

> **Estado:** implementado — pendiente de cableado físico y demo  
> **Rama del roadmap:** Fase 1 → Input Controls  
> **Spec referencia:** `apps/docs/01-architecture.md §3.3` — Pin Mapping (SSoT)

---

## 1. Theory

### 1.1 Encoders de cuadratura (ALPS EC11)

Un encoder rotativo incremental genera dos señales en cuadratura: canal A y canal B, desfasadas 90° entre sí. El ALPS EC11 produce 4 pulsos por detent mecánico (uno por flanco de A y B combinados).

```
Giro CW:   A: ‾|_|‾|_    B: _|‾|_|‾
Giro CCW:  A: ‾|_|‾|_    B: ‾|_|‾|_  (B lidera a A)
```

La dirección se detecta comparando el estado de B cuando A tiene flanco: si B está LOW cuando A cae → CW; si B está HIGH → CCW.

**Por qué Encoder.h (Teensyduino) en lugar de polling manual:**  
Encoder.h de PaulStoffregen usa interrupciones en los dos pines (A y B) para detectar cada flanco. El polling manual en `loop()` perdería pulsos si el loop tarda más de ~50µs por iteración. A 600MHz el Teensy puede atender los interrupts sin impacto medible en el audio ISR.

`readAndReset()` retorna el conteo acumulado desde el último llamado y lo resetea a cero. Dividir por 4 convierte pulsos crudos en "detents" (clicks visibles).

### 1.2 Debounce (Bounce2 / Button)

Los switches mecánicos (tanto encoders push como Kailh Choc) generan rebotes de hasta ~5ms al presionar. Sin debounce, un press registra como múltiples eventos.

Bounce2 usa un filtro de intervalo fijo: solo acepta un cambio de estado si el pin se mantiene estable durante N milisegundos (5ms en este proyecto). La clase `Button` extiende `Bounce` con:
- `setPressedState(LOW)` — declara que el pin en LOW = botón presionado (con pull-up)
- `pressed()` — true solo en el ciclo donde ocurre la transición debounced a presionado
- `isPressed()` — estado actual (sin transición)
- `duration()` — milisegundos en el estado actual (útil para detección de held)

**INPUT_PULLUP:** los pines internos de pull-up del Teensy 4.1 son de ~47kΩ. Con los switches Kailh Choc (< 1Ω de resistencia de contacto), la tensión en el pin cae a ~0V al presionar. Sin pull-up externo, el pin flotaría y generaría falsos positivos.

### 1.3 Protocolo WS2812B — timing crítico

El WS2812B usa un protocolo one-wire de alta velocidad (800kHz). Cada bit se codifica por duración del pulso HIGH:

```
Bit 0: HIGH ~350ns, LOW ~900ns
Bit 1: HIGH ~900ns, LOW ~350ns
Reset: LOW > 50µs
```

Tolerancia: ±150ns. Un LED consume 3 bytes (G, R, B) = 24 bits. Los 16 LEDs requieren 384 bits = 480µs de transmisión continua, más el reset.

**Por qué FastLED en GPIO28 (no WS2812Serial):**  
WS2812Serial requiere un pin que sea TX de un UART hardware del Teensy 4.1 (pines 1, 8, 14, 17, 20, 24, 29, 35). GPIO28 no es un UART TX. FastLED usa bit-banging con interrupciones deshabilitadas durante la transmisión — esto bloquea el CPU ~500µs por `show()`, aceptable a la frecuencia de actualización de UI (< 60Hz).

**Brightness cap (80/255 ≈ 31%):** cada WS2812B en blanco puro consume ~60mA. 16 LEDs en blanco = 960mA — imposible con USB 500mA. A brightness 80, el consumo máximo baja a ~300mA, dejando margen para Teensy (~150mA) + ESP32 (~200mA).

### 1.4 Por qué Teensy maneja la UI en lugar del ESP32

El ESP32-S3 corre FreeRTOS. Las tareas RTOS tienen latencia de despacho no determinística (típicamente 1-5ms de jitter dependiendo de prioridades y ISR pendientes). Para el quadrature decoding, perder flancos de encoder es inaceptable — 1ms de jitter equivale a 40 RPM de pérdida a velocidad normal de giro.

El Teensy corre bare-metal (sin OS). Los interrupts de GPIO tienen latencia <1µs desde el flanco hasta la ISR. Esto garantiza que cada pulso de encoder sea capturado sin pérdida, incluso con el audio ISR corriendo a 44.1kHz.

El ESP32 recibe el estado de UI procesado ya (delta de encoders, eventos de botones) via Bridge Protocol — no maneja señales crudas de GPIO.

---

## 2. Tabla de cableado

> Hardware no cableado aún. Esta tabla define los cables a conectar cuando el PCB esté listo.

### 2.1 Encoders ALPS EC11

| Pin Teensy | Función | Componente | Señal | Nota |
|---|---|---|---|---|
| GPIO 10 | ENC L — A | ALPS EC11 encoder L | Canal A | Pull-up interno activo |
| GPIO 11 | ENC L — B | ALPS EC11 encoder L | Canal B | Pull-up interno activo |
| GPIO 12 | ENC L — SW | ALPS EC11 encoder L | Push switch | Pull-up interno activo |
| GPIO 13 | ENC R — A | ALPS EC11 encoder R | Canal A | Pull-up interno activo |
| GPIO 14 | ENC R — B | ALPS EC11 encoder R | Canal B | Pull-up interno activo |
| GPIO 15 | ENC R — SW | ALPS EC11 encoder R | Push switch | Pull-up interno activo |
| GPIO 16 | ENC NAV — A | ALPS EC11 encoder NAV | Canal A | Pull-up interno activo |
| GPIO 17 | ENC NAV — B | ALPS EC11 encoder NAV | Canal B | Pull-up interno activo |
| GPIO 26 | ENC NAV — SW | ALPS EC11 encoder NAV | Push switch | Pull-up interno activo |
| GND | Común | Todos los encoders | GND mecánico | Un cable GND desde cada encoder |

**Colores de cable sugeridos:** A = amarillo, B = naranja, SW = blanco, GND = negro.

### 2.2 Botones Kailh Choc V2

| Pin Teensy | Función | Componente | Nota |
|---|---|---|---|
| GPIO 2 | B1 | Kailh Choc V2 — botón 1 | Un terminal → GPIO, otro → GND |
| GPIO 3 | B2 | Kailh Choc V2 — botón 2 | Un terminal → GPIO, otro → GND |
| GPIO 4 | B3 | Kailh Choc V2 — botón 3 | Un terminal → GPIO, otro → GND |
| GPIO 5 | B4 | Kailh Choc V2 — botón 4 | Un terminal → GPIO, otro → GND |

### 2.3 WS2812B LED ring + keycap underglow

| Pin Teensy | Función | Componente | Nota |
|---|---|---|---|
| GPIO 28 | DIN | WS2812B — primer LED del chain | Resistor 300-500Ω en serie recomendado para proteger el pin |
| 5V / VCC | VCC | WS2812B chain completa | Alimentar desde rail 5V (no desde pin Teensy) |
| GND | GND | WS2812B chain completa | GND común con Teensy |

**Orden del chain:** índices 0–11 = ring ENC NAV (sentido horario desde las 12), índices 12–15 = keycap underglow B1–B4.

### 2.4 Filter bypass CD4066

| Pin Teensy | Función | Componente | Estado activo |
|---|---|---|---|
| GPIO 25 | CD4066 enable | CD4066 pin de control | HIGH = bypass activo, LOW = filtro en circuito |

---

## 3. Arquitectura de módulos

```
src/ui/
├── encoders.h / .cpp    — Encoders: wrappea 3× Encoder.h + 3× Button (SW)
├── buttons.h  / .cpp    — Buttons: 4× Button (B1-B4) con detección de held
├── leds.h     / .cpp    — Leds: FastLED WS2812B, 16 LEDs, métodos de alto nivel
└── ui_manager.h / .cpp  — UIManager: despacha eventos a callbacks de aplicación
```

### Flujo de datos

```
[ALPS EC11] ──interrupt──► Encoder.h (ISR) ──► readAndReset() ──► UIManager
[Kailh Choc] ──GPIO──► Button.update() ──► pressed() ──► UIManager
[ENC SW] ──GPIO──► Button.update() ──► pressed() ──► UIManager (double-push detection)
                                                              │
                       ┌──────────────────────────────────────┤
                       ▼                  ▼                   ▼
               ParamChangedCb        ButtonCb             NavCb
               (cutoff, resonance)   (B1-B4, confirm,    (nav delta)
                                      mode switch,
                                      filter bypass,
                                      resonance reset)
```

### Lógica de double-push (UIManager)

El doble push del ENC NAV en ≤400ms activa BTN_MODE_SWITCH. La detección es stateful:

1. Primer press → guarda `millis()` en `_nav_sw_first_press_ms`
2. Segundo press dentro de 400ms → dispara `BTN_MODE_SWITCH`
3. Si pasan 400ms sin segundo press → dispara `BTN_CONFIRM` y resetea estado

El timeout se evalúa en cada llamada a `update()` — no requiere timer separado.

---

## 4. Demo — qué verás al conectar el hardware

1. **Boot:** ring de 16 LEDs pulsa en teal (`#1D9E75`) 300ms, luego se apaga. Serial imprime `[BOOT] GrooveForge Brain — Sprint 3.3 UI ready`.
2. **ENC L:** girar imprime `[UI] cutoff delta=±0.01` por detent. Empujar toggle GPIO25 (filter bypass) con reporte serial.
3. **ENC R:** girar imprime `[UI] resonance delta=±0.01` por detent. Empujar resetea resonance.
4. **ENC NAV:** girar imprime `[UI] nav delta=±N`. Push simple → `[UI] ENC NAV confirm`. Double-push rápido (≤400ms) → ring cambia a purple (`#534AB7`) indicando FX mode; doble de vuelta → teal (SYNTH mode).
5. **B1–B4:** cada botón imprime `[UI] B1/B2/B3/B4 pressed`.
6. **Held:** presionar cualquier Kailh Choc >500ms no genera evento `pressed()` adicional — la lógica de held está disponible para Sprint 4.x (B4 held = PRESET browser).

---

## 5. Decisiones de diseño no triviales

### 5.1 `Encoder` como miembro de clase (no global)

La documentación de Encoder.h recomienda instancias globales para maximizar rendimiento de ISR. En este proyecto, los tres encoders se instancian como miembros de `Encoders` — que a su vez es una variable `static` global en `main.cpp`. El efecto es idéntico: los objetos tienen duración estática. Esto es preferible porque encapsula la dependencia.

### 5.2 `build_flags = -I../../apps/bridge-protocol/include`

`protocol.h` es un header compartido (no una librería PlatformIO). Se incluye via `-I` en `build_flags` para que el LDF no intente empaquetarlo como librería, evitando dependencias circulares.

### 5.3 Separación UIManager / motor de audio

`UIManager` no toca el audio graph directamente — solo llama callbacks registrados. Esto permite testear la UI con mocks nativos (sin hardware de audio) en `pio test -e native`.

---

## 6. Learnings

> Completar tras el primer demo con hardware conectado.

---

*Sprint 3.3 — Encoders + Buttons + LEDs*  
*Juan Guerrero (GPROG) · GrooveForge Brain v3.0*
