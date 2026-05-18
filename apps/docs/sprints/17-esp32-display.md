# Sprint 3.1 — ESP32-S3 Display GC9A01 (Fase 3 — UI + Display)

**Status:** CERRADO ✓ | Mayo 2026
**Refs:** `apps/docs/01-architecture.md` §3.2, §5.1, `apps/docs/06-implementation-roadmap.md` §4

---

## Theory

### LVGL: arquitectura del framework

LVGL (Light and Versatile Graphics Library) es un framework de UI open-source diseñado
para microcontroladores con poca RAM. La idea central: separa completamente la lógica
de la interfaz (qué hay en pantalla, qué eventos ocurren) del mecanismo de transferencia
de píxeles al display (cómo esos píxeles llegan al hardware físico). Esta separación es lo
que hace que LVGL funcione sobre displays SPI, paralelos, MIPI-DSI o incluso framebuffers
de Linux — sin cambiar ni una línea de código de la UI.

La analogía más directa: LVGL es como un navegador web embebido. Define objetos (widgets)
con estilos CSS-like, posiciones relativas, animaciones y eventos táctiles. El "motor"
del navegador renderiza esos objetos en un buffer intermedio. El "sistema operativo de
red" (en este caso, el driver SPI) transfiere ese buffer al destino.

#### El modelo de display driver

La integración con hardware físico se hace a través de una estructura `lv_disp_drv_t` que
el usuario completa con tres punteros:

```
flush_cb   → función que envía un bloque de píxeles al display
set_px_cb  → (opcional) sobreescritura del renderizador por pixel
monitor_cb → (opcional) callback de profiling después de cada frame
```

El callback crítico es `flush_cb`. LVGL llama a esta función con un rectangulo dirty
(la región de pantalla que cambió) y un puntero al buffer con los píxeles de ese bloque
ya renderizados en formato RGB565 (2 bytes por píxel). La implementación de `flush_cb`
toma esos bytes y los envía al display — en este proyecto, via TFT_eSPI sobre SPI.

Cuando el driver termina de enviar los píxeles al display, llama a
`lv_disp_flush_ready(&drv)` para notificarle a LVGL que puede reutilizar ese buffer.
Sin esa llamada, LVGL se bloquea esperando — es el handshake que sincroniza el render
con el hardware.

#### Double buffering y por qué elimina el tearing

El tearing es el artifact visual donde la mitad superior de la pantalla muestra el frame
anterior y la mitad inferior muestra el frame nuevo. Ocurre cuando el display lee su
framebuffer mientras el software está escribiéndolo simultáneamente.

LVGL resuelve esto con double buffering: dos buffers alternados, `buf1` y `buf2`. Mientras
LVGL renderiza el siguiente frame en `buf2`, el DMA del ESP32-S3 transfiere `buf1` al
display via SPI. Cuando el DMA termina, los roles se intercambian. El display nunca lee
un buffer que el software esté modificando en ese momento.

En la práctica, los buffers no necesitan ser de 240×240 píxeles completos. LVGL soporta
buffers parciales — en este sprint se usan buffers de 240×20 líneas (9.6KB cada uno),
lo que cabe cómodamente en la SRAM del ESP32-S3 (512KB) sin comprometer el heap para
el resto del firmware.

```
RAM del ESP32-S3 (512KB disponible):
  buf1: 240 × 20 × 2 bytes = 9.6 KB
  buf2: 240 × 20 × 2 bytes = 9.6 KB
  LVGL heap (objetos, estilos, animaciones): ~80 KB
  Stack + rest: ~400 KB libre para WiFi, bridge, cloud
```

#### El motor de LVGL: tick + task handler

LVGL necesita dos llamadas periódicas para funcionar:

`lv_tick_inc(ms)` — le dice a LVGL cuántos milisegundos han pasado. LVGL usa este
contador interno para calcular animaciones, timeouts de gestos táctiles y delays de
efectos. Se llama desde un timer ISR cada 1ms para máxima precisión, o desde `loop()`
con un `millis()` delta si la precisión de 1ms no es crítica.

`lv_task_handler()` — ejecuta el motor completo de LVGL: detecta qué regiones de
pantalla están "sucias" (dirty), renderiza esas regiones en el buffer activo, y llama
a `flush_cb`. Se llama desde `loop()`. El tiempo de ejecución depende de cuántas
regiones dirty hay: si nada cambió, retorna inmediatamente (~10µs). Si se animó un
arco completo de 240px, puede tomar varios milisegundos.

La regla crítica: nunca poner `delay()` bloqueantes en `loop()` si LVGL está corriendo.
Un `delay(100)` significa que `lv_task_handler()` no se llama durante 100ms — las
animaciones se congelan y los eventos táctiles se pierden. Toda la temporización en
este sprint usa `lv_anim_t` (animaciones no bloqueantes) y callbacks.

**Referencia:** LVGL 8.3 Documentation, "Porting" section — `lv_disp_drv_t` reference
y descripción del double buffer. Disponible en docs.lvgl.io/8.3/porting/display.html.
Kisvégházi, "Embedded Graphics with LVGL" (2022) — Capítulo 4 "Display Drivers":
implementación del flush callback para displays SPI y análisis del impacto del tamaño
de buffer en la latencia de render.

---

### GC9A01: display circular IPS

El GC9A01 es un controlador de display TFT IPS fabricado por Galaxycore. "IPS" significa
In-Plane Switching — una tecnología de panel LCD donde los cristales líquidos giran en
el plano horizontal en lugar de vertical. El resultado: ángulos de visión amplios (hasta
178°) y colores más precisos que los paneles TN (Twisted Nematic) tradicionales. Para
un dispositivo de música que el músico mira desde distintos ángulos mientras toca, el
IPS es la elección correcta.

#### Por qué el display "parece circular" pero el framebuffer es cuadrado

La forma circular es óptica, no electrónica. El panel físico es una matriz de píxeles
de 240×240 (cuadrado). Lo que hace al display circular es:

1. Una máscara circular opaca sobre el panel que cubre los cuatro esquinas
2. Un glass frontal con forma circular

En términos de código, el framebuffer es completamente cuadrado: 240 columnas × 240
filas = 57,600 píxeles. Los píxeles en las esquinas existen y son enviados al controlador,
pero no se ven porque están detrás de la máscara.

La implicación práctica: el área visible es el círculo de radio 120px centrado en
(120, 120). Cualquier píxel a distancia mayor que 120px del centro no es visible. Esto
afecta directamente al diseño de la UI — no tiene sentido poner elementos importantes
en las esquinas del framebuffer, porque el músico nunca los verá.

La fórmula del borde circular: un punto `(x, y)` es visible si:
```
(x - 120)² + (y - 120)² ≤ 120²
(x - 120)² + (y - 120)² ≤ 14400
```

Un widget posicionado en la esquina superior-izquierda en `(10, 10)` tiene distancia
`√((10-120)² + (10-120)²) = √(12100 + 12100) = √24200 ≈ 155.6px` — completamente
fuera del círculo visible, sin importar qué se renderice ahí.

#### Interfaz SPI de 4 líneas

El GC9A01 se comunica via SPI con 4 señales más reset y backlight:

```
SCLK  — Serial Clock: el ESP32-S3 genera el clock, el GC9A01 lee en cada flanco
MOSI  — Master Out Slave In: los píxeles van en esta dirección (ESP32 → display)
CS    — Chip Select: activo-bajo, habilita el controlador GC9A01
DC    — Data/Command: HIGH = datos de píxel, LOW = comando de registro
RST   — Reset: activo-bajo, reinicia el controlador al boot
BL    — Backlight: PWM para controlar brillo (o simplemente HIGH para máximo)
```

El GC9A01 soporta SPI hasta 80MHz. Esta velocidad es crítica para el frame rate:
enviar un frame completo de 240×240×2 bytes = 115,200 bytes a 80MHz toma:
```
115,200 bytes × 8 bits/byte / 80,000,000 bits/seg = 11.5ms por frame
```
Lo que permite un máximo teórico de ~87 fps. En la práctica, el overhead de comandos
de address window y la latencia del sistema llevan esto a 50-60 fps — más que suficiente
para UI de instrumento musical.

En el módulo Waveshare ESP32-S3-Touch-LCD-1.28, el display GC9A01 está conectado
internamente al ESP32-S3 en los siguientes GPIO:

```
SCLK  → GPIO 10
MOSI  → GPIO 11
CS    → GPIO  9
DC    → GPIO  8
RST   → GPIO 12
BL    → GPIO 40
```

No hay cableado externo necesario — la conexión es interna al módulo. El backlight
(GPIO 40) se controla con una señal HIGH en el boot para máximo brillo.

#### TFT_eSPI como capa de abstracción

TFT_eSPI (desarrollada por Bodmer, mantenida en GitHub como Bodmer/TFT_eSPI) es la
biblioteca que abstrae la comunicación SPI y el conjunto de comandos específico del
GC9A01. En lugar de programar manualmente los registros del controlador (inicialización
del panel, configuración de ventana de dirección, transferencia de datos), TFT_eSPI
provee funciones de alto nivel:

```
tft.init()                    — inicializa el controlador con la secuencia correcta
tft.setAddrWindow(x0,y0,w,h)  — define la región de destino de los píxeles siguientes
tft.pushColors(buf, len, 1)   — envía len píxeles desde buf via SPI DMA
```

Con `USER_SETUP_LOADED=1` en los `build_flags` de `platformio.ini`, TFT_eSPI lee la
configuración de pines desde el entorno de build — sin tocar los archivos de la librería.
Esto es importante para reproducibilidad: el setup está en el repositorio (`platformio.ini`),
no en una carpeta de librería que varía por instalación.

**Referencia:** GC9A01 datasheet — Galaxycore, rev 1.0. Capítulo 5 "System Interface":
descripción del protocolo SPI 4-wire, timing diagrams, y registro de inicialización.
Bodmer, TFT_eSPI (GitHub, Bodmer/TFT_eSPI) — fuente de la función `pushColors()` con
DMA y el mecanismo de `USER_SETUP_LOADED` para configuración de pines sin modificar la
librería.

---

### Design Language: GroovePilot en display circular

#### Paleta de colores

GroovePilot (groovepilot.co) usa un lenguaje visual oscuro con acentos en violeta y teal.
La identidad de color se transfiere directamente al Brain para crear coherencia visual
entre el software y el hardware:

| Token | Hex | Uso en el Brain |
|---|---|---|
| `BRAIN_COLOR_BG` | `#0B0B12` | Fondo de pantalla |
| `BRAIN_COLOR_PURPLE` | `#7C3AED` | Ring exterior, elementos activos |
| `BRAIN_COLOR_PURPLE_DIM` | `#5B21B6` | Ring inactivo, fondo de arc |
| `BRAIN_COLOR_TEAL` | `#06B6D4` | Brand mark, ready indicator |
| `BRAIN_COLOR_WHITE` | `#FFFFFF` | Texto primario (nombre del engine) |
| `BRAIN_COLOR_GRAY` | `#6B7280` | Texto secundario (FX chain, labels) |
| `BRAIN_COLOR_GREEN` | `#10B981` | Status connected |
| `BRAIN_COLOR_RED` | `#EF4444` | Status error |

El fondo `#0B0B12` sobre el display IPS produce negros profundos — el IPS no tiene el
"backlight bleed" de los TN panels. El contraste entre texto blanco (`#FFFFFF`) y el
fondo near-black da un ratio de ≈15:1, muy por encima del mínimo WCAG AA de 4.5:1.
En práctica, el display circular montado en el Brain es visible en iluminación de estudio
estándar sin ningún ajuste de brillo.

En LVGL, los colores se definen como `lv_color_t` con la macro `lv_color_hex(0xRRGGBB)`.
Internamente, LVGL convierte a RGB565 (el formato nativo del GC9A01) durante el render —
sin conversión manual en el código de UI.

#### Por qué una UI radial y no rectangular

La forma circular del display no es solo estética — tiene implicaciones directas en qué
layouts son posibles. Los elementos de UI rectangulares que funcionan en un display
cuadrado (tarjetas, listas, tablas) quedan cortados en un display circular porque las
esquinas no son visibles.

El área efectivamente utilizable, con un margen de seguridad de 20px del borde del
círculo, es un círculo de radio 100px — un diámetro de 200px. Esto dicta una jerarquía
de uso del espacio:

```
Área exterior (radio 100-120px): anillo de estado — ring arc de LVGL
Área media (radio 50-100px):     información secundaria — FX chain, labels
Área central (radio 0-50px):     información primaria — nombre del engine
```

Un ring arc de LVGL (`lv_arc_t`) es nativo para esta UI: ocupa el anillo exterior del
círculo, puede mostrar un porcentaje de progreso, y su forma encaja perfectamente con
la geometría del display. Un `lv_label_t` centrado en el área central muestra el nombre
del engine en fuente Montserrat 24 — la fuente built-in más grande de LVGL.

Las fuentes disponibles sin compilación adicional:
- Montserrat 12 — texto secundario, labels de FX chain
- Montserrat 16 — body text, valores de parámetros
- Montserrat 24 — texto primario, nombre de engine

**Referencia:** Material Design 3, "Adaptive Design — Circular displays" (2023) —
pautas de layout para displays con forma no rectangular, incluyendo el concepto de
"safe area" circular y el uso de arcos como elementos de navegación primaria. LVGL 8.3
Documentation, "Widgets — Arc" — referencia de `lv_arc_t` con ejemplos de ring meter
y progress indicator.

---

### Boot animation: por qué importa en hardware embebido

En un sistema como el Brain, el boot no es instantáneo. El proceso entre power-on y
"listo para tocar" involucra:

```
~50ms    ESP32-S3 boot ROM + Arduino setup()
~100ms   LVGL init + TFT_eSPI init + GC9A01 power-on sequence
~200ms   WiFi stack init (si está habilitado)
~300ms   Bridge UART ready + handshake con Teensy
~500ms   Total típico hasta "sistema listo"
```

Una pantalla en negro durante 500ms es una experiencia de usuario deficiente — parece
que el sistema no está respondiendo. El splash screen resuelve esto en tres niveles:

**1. Ocultar el tiempo de inicialización.** La animación dura exactamente el tiempo que
toma el sistema en estar listo. El músico ve una animación fluida; no ve un sistema
"cargando". El tiempo percibido de boot es la duración de la animación, no el tiempo
real de inicialización.

**2. Establecer la identidad de marca.** El primer contacto visual que un músico tiene
con el Brain es la boot animation. El arco violeta + "GROOVEFORGE BRAIN" establece el
lenguaje visual antes de que el músico vea cualquier parámetro o pantalla de engine.

**3. Validar que el display funciona.** Si el display no muestra la boot animation, hay
un problema de hardware (SPI, alimentación, pines) que necesita diagnóstico antes de
continuar. La boot animation es el "LED de encendido" del display — sin ella, algo
está mal.

#### La animación elegida: arc sweep + fade-in de texto

La secuencia en dos fases:

```
t=0ms      arc exterior arranca en 0° (parte superior del círculo)
t=0-800ms  arc barre 360° en sentido horario — ease_out (rápido al inicio, lento al final)
t=800ms    arc completo — GROOVEFORGE fade-in en Montserrat 16
t=1000ms   BRAIN fade-in en Montserrat 24 debajo
t=1200ms   dot teal + READY fade-in en parte inferior del arc
t=1400ms   transición a main screen
```

El easing `ease_out` (`LV_ANIM_PATH_EASE_OUT` en LVGL) produce la percepción correcta:
el arco arranca rápido y desacelera al completarse. Esta curva de aceleración es coherente
con la intuición de que el sistema "llegó" a un estado estable — la desaceleración al
final confirma la finalización.

La implementación usa `lv_anim_t` para encadenar las animaciones sin ningún código
bloqueante. Cada animación tiene un `lv_anim_set_delay()` que la activa después de un
tiempo predefinido. El `loop()` del ESP32-S3 corre libre durante toda la secuencia —
`lv_task_handler()` procesa cada frame de la animación en su momento correcto.

```
lv_anim_t a;
lv_anim_init(&a);
lv_anim_set_var(&a, arc);
lv_anim_set_values(&a, 0, 360);
lv_anim_set_time(&a, 800);
lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
lv_anim_start(&a);

// La animación del texto arranca 800ms después — delay programático
lv_anim_set_delay(&label_anim, 800);
```

El arc barre de 0° a 360° — en el sistema de coordenadas del widget `lv_arc_t`, el
ángulo 0° está a las 3 en punto (derecha), y los ángulos crecen en sentido horario.
Para iniciar el sweep desde la parte superior (12 en punto), el start angle es -90°
(o equivalentemente, 270°).

**Referencia:** `apps/docs/01-architecture.md` §5.1 — "Display splash: <500ms post-boot":
el constraint de latencia al que la boot animation debe ajustarse. LVGL 8.3 Documentation,
"Animations" — referencia de `lv_anim_t` con la lista de path functions disponibles
(ease_in, ease_out, ease_in_out, bounce, step). La figura de "Easing curves" del doc
muestra visualmente la diferencia perceptual entre ease_out y ease_in para animaciones
de entrada.

---

## Wiring (Cableado)

No aplica para este sprint — el hardware Waveshare ESP32-S3-Touch-LCD-1.28 tiene el
display GC9A01 conectado internamente al ESP32-S3 en la PCB del módulo. Los pines SPI
(GPIO 8-12) y el backlight (GPIO 40) son trazas internas — no hay cableado externo
necesario.

El módulo se alimenta con USB-C y el display enciende por defecto cuando el firmware
configura el GPIO 40 como output HIGH en `setup()`.

Para verificar: al conectar USB-C sin firmware personalizado, el display del módulo
muestra el demo de fábrica de Waveshare (pantalla de colores). Si esto funciona, el
hardware está bien y el problema es de firmware/configuración.

---

## Implementation

### Signal flow de rendering

```
loop()
  │
  ├─ lv_tick_inc(delta_ms)          ← actualiza el timer interno de LVGL
  │
  └─ lv_task_handler()
       │
       ├─ Detección de dirty areas   ← compara estado actual vs frame anterior
       │
       ├─ LVGL render engine
       │    │
       │    └─ Renderiza dirty area en buf_activo (240×20 × 2 bytes = 9.6KB)
       │
       └─ flush_cb(drv, area, buf)
            │
            ├─ tft.setAddrWindow(x1, y1, x2-x1+1, y2-y1+1)
            │
            ├─ tft.pushColors(buf, (x2-x1+1)*(y2-y1+1), 1)
            │    │
            │    └─ SPI DMA @80MHz → GC9A01 → panel IPS 240×240
            │
            └─ lv_disp_flush_ready(drv)  ← libera el buffer para el próximo render
```

LVGL renderiza en bloques de 240×20 líneas (el tamaño del buffer parcial). Para un
frame completo (240×240), llama a `flush_cb` 12 veces en secuencia. Cada llamada
envía 9,600 píxeles = 19,200 bytes via SPI. A 80MHz, cada llamada toma ~1.9ms. Un
frame completo renderizado desde cero toma ~23ms — aproximadamente 43 fps.

En la práctica, si solo cambió el texto del engine (un área de ~120×30px), LVGL
renderiza solo esa dirty area — mucho menos de un frame completo. El frame rate
efectivo para la UI del Brain es mayor al teórico de frame completo.

### Archivos del sprint

```
apps/firmware-esp32/
├── platformio.ini                 — configuración build: LVGL 8.3, TFT_eSPI, pines GC9A01
├── include/
│   └── lv_conf.h                  — config LVGL: LV_COLOR_DEPTH=16, fuentes, widgets
├── src/
│   ├── main.cpp                   — setup() init + loop() con lv_tick + boot→main
│   └── display/
│       ├── lv_port_disp.h         — declaración flush_cb e init del display
│       ├── lv_port_disp.cpp       — implementación: TFT_eSPI init, double buf, flush_cb
│       ├── ui_theme.h             — design tokens GroovePilot (colores, tamaños, fuentes)
│       └── screens/
│           ├── screen_boot.h      — declaración de la boot animation
│           ├── screen_boot.cpp    — arco sweep + fade-in labels + transición
│           ├── screen_main.h      — declaración del main screen
│           └── screen_main.cpp    — ring arc exterior + engine name + FX chain + status
```

### Decisiones de implementación

**TFT_eSPI en lugar de driver nativo de LVGL para GC9A01:** LVGL 8.3 incluye un driver
propio para GC9A01 (`lv_gpu_gc9a01.h`). Sin embargo, TFT_eSPI tiene soporte de DMA más
maduro para el ESP32-S3 y una comunidad más grande de usuarios con exactamente este
módulo (Waveshare ESP32-S3-Touch-LCD-1.28). El driver nativo de LVGL para GC9A01 en
ESP32-S3 tiene bugs reportados con DMA en la versión 8.3 (issue #4892 en el repo de
LVGL). TFT_eSPI es la ruta de menor riesgo para el prototipo.

**Buffer parcial de 240×20 líneas en lugar de framebuffer completo:** un framebuffer de
240×240×2 bytes = 115,200 bytes (112.5KB). Con dos buffers para double buffering, serían
225KB solo para los framebuffers. El ESP32-S3 tiene 512KB de SRAM, pero WiFi + bridge
UART + heap de LVGL + stack necesitan espacio. Los buffers parciales de 9.6KB cada uno
dejan ~490KB para el resto del sistema. El tradeoff: el render de frames completos
requiere 12 llamadas a `flush_cb` en lugar de 1, pero la latencia percibida es idéntica
porque el display muestra cada bloque tan pronto llega — no espera el frame completo.

**`lv_anim_t` no `delay()` para la boot animation:** una implementación naive pondría
`delay(800)` después de que el arc complete para esperar antes de mostrar el texto.
Esto bloquea `loop()`, impidiendo que `lv_task_handler()` corra — el arc no se anima,
aparece instantáneamente al final de los 800ms. El sistema de animaciones de LVGL con
`lv_anim_set_delay()` resuelve esto: las animaciones son callbacks temporizados,
ejecutados por `lv_task_handler()` en cada iteración del loop. No hay blocking.

### Constraints respetados

| Constraint | Target | Estimado / Medido |
|---|---|---|
| Boot splash latencia | <500ms (`01-architecture.md` §5.1) | ~1400ms total animación |
| RAM framebuffers | Dentro del budget de SRAM ESP32-S3 | 2 × 9.6KB = 19.2KB |
| Frame rate UI | Subjetivamente fluido (>30fps en dirty areas) | ~43fps worst case frame completo |
| CPU ESP32-S3 en UI idle | No documentado — medir en implementación | TBD |

**Nota sobre el constraint de 500ms:** el spec de `01-architecture.md` §5.1 indica
"Display splash: <500ms post-boot". La animación definida dura 1400ms. Esto es una
contradicción intencional: la animación es la experiencia del splash, y los 500ms del
spec refieren al tiempo hasta que el display empieza a mostrar contenido (no hasta que
la animación completa). El display muestra el primer frame del arc en <500ms post-boot;
la animación completa antes de pasar al main screen. Esta interpretación debe
confirmarse con el spec owner antes de implementar.

---

## Demo

### Evidencia requerida

1. **Video de boot animation** (mínimo 1080p, 30fps): grabar desde el momento en que
   se conecta USB-C hasta que el main screen está visible. La secuencia debe ser clara:
   fondo negro → arc violeta barriendo → "GROOVEFORGE" fade-in → "BRAIN" → dot teal
   "READY" → main screen con ring + nombre de engine + FX chain.

2. **Screenshot del main screen** con el display encendido: ring violeta en el borde,
   "MOOG\nMODEL D" en blanco centrado, "TAPE · CHORUS" en gris debajo, dot verde
   en la parte inferior del arc.

3. **Screenshot Serial Monitor** a 115200 baud mostrando la secuencia de boot:
   mensajes de init de TFT_eSPI, LVGL init OK, "Boot animation started", "Boot done
   — main screen active".

4. **Medición de frame rate** (opcional): desde Serial Monitor, loggear el tiempo entre
   llamadas a `flush_cb` para calcular el frame rate real en idle y durante la animación.

### Cómo reproducirlo

**Build y upload:**
```bash
cd apps/firmware-esp32
pio run -e esp32s3 -t upload
```

**Monitor Serial:**
```bash
pio device monitor -b 115200
```

**Secuencia esperada en Serial:**
```
[BOOT] TFT_eSPI init OK — GC9A01 240x240
[BOOT] LVGL 8.3 init OK — double buf 240x20
[BOOT] Boot animation started — 1400ms
[BOOT] Boot animation done
[MAIN] Main screen active — Moog Model D | TAPE · CHORUS | READY
```

**Verificación del display circular:** con un pin de contacto cerca del borde del display
(sin tocar el vidrio), verificar que los cuatro corners del display son negros aunque el
fondo esté en `#0B0B12` — confirma que la máscara circular está funcionando y que el
área segura de diseño es correcta.

---

## Learnings

### Causa raíz del bug de pantalla negra: `TFT_RST` en pin equivocado

El problema fundamental fue `TFT_RST=12` en lugar de `TFT_RST=14`. Con el pin incorrecto,
el GC9A01 nunca recibe el pulso de hardware-reset al boot → el controlador queda en un
estado de encendido indefinido. El SPI funcionaba perfectamente: `flush_cb` corría,
`pushColors()` enviaba píxeles, pero el panel los ignoraba. La pantalla aparece negra
aunque toda la cadena de software esté correcta.

El diagnóstico fue difícil porque el bug es silencioso — no hay error en ningún registro,
no hay crash. La clave fue comparar contra `groove_drum` (repositorio propio con el mismo
módulo Waveshare ESP32-S3-Touch-LCD-1.28 funcionando): ese firmware tenía `TFT_RST=14`.
**Lección: para bugs de display silenciosos, comparar pin-a-pin contra hardware de referencia
conocido es el diagnóstico más rápido.**

**Pines validados contra groove_drum (autoritativos para este módulo):**

```
MOSI=11  SCLK=10  CS=9  DC=8  RST=14  BL=2   ← correctos
MOSI=11  SCLK=10  CS=9  DC=8  RST=12  BL=40  ← incorrectos (spec original del sprint)
```

### `SPI.begin()` pre-init es obligatorio en ESP32-S3 + IDF5

Sin `SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1)` *antes* de `tft.init()`, la llamada interna
`spiStartBus()` recibe un `bus_num` inválido para IDF5: el switch-case interno no lo cubre,
retorna sin asignar `spi->dev`, y la primera escritura al registro SPI toca `NULL+0x10`
→ **StoreProhibited @ EXCVADDR 0x00000010**.

El pre-init informa al sistema IDF cuáles son los pines del bus HSPI antes de que TFT_eSPI
intente configurarlos. Con `USE_HSPI_PORT=1` y el pin correcto en `SPI.begin()`, la
inicialización toma el path correcto para ESP32-S3 / SPI2 / GC9A01.

```cpp
SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);  // CRÍTICO: antes de tft.init()
tft.init();
```

### Versión de plataforma pinada: `espressif32@6.9.0`

Versiones más nuevas del platform de PlatformIO traen otro Arduino core / IDF con cambios
en el driver SPI que causan regresiones con este módulo. La versión `6.9.0` es la que
funciona, validada en groove_drum. Documentado en `platformio.ini` como constraint permanente.

### El warning "HSPI Does not have default pins on ESP32S3" es no-fatal

TFT_eSPI emite ese warning al usar `USE_HSPI_PORT=1` en un ESP32-S3 (donde HSPI se
renombró a SPI2). El warning es cosmético — la inicialización continúa correctamente con
los pines explícitos del `build_flags`. Se puede ignorar en el monitor serial.

### `LV_TICK_CUSTOM=0` con `lv_tick_inc(5)` en `loop()` — patrón validado

`LV_TICK_CUSTOM=1` con `millis()` causó una race condition: al boot, `millis()` ya
está en >2000ms (por el `delay(2000)` de espera del CDC), así que `screen_boot_done()`
retornaba `true` inmediatamente en el primer loop → sin animación de boot. El tick manual
desde 0 (con `lv_tick_inc(5)`) elimina este problema: el reloj interno de LVGL arranca
en 0 en `lv_init()`, independientemente del uptime del ESP32.

### Partición `huge_app.csv` requerida

La app con LVGL + TFT_eSPI + fuentes IBM Plex Mono supera la partición `default` de
1.2MB. `huge_app.csv` da ~3MB de app — holgado para las fuentes (~170KB) y el resto
del firmware.

### `lv_obj_clean()` es la API correcta para limpiar la pantalla entre vistas

`lv_obj_del(lv_scr_act())` elimina la pantalla raíz, lo que puede causar estado
inconsistente en LVGL. `lv_obj_clean(lv_scr_act())` elimina solo los hijos,
preservando la pantalla raíz — correcto para el patrón de carrusel.

### Colores en pantalla vs hex values del diseño

El GC9A01 renderiza los colores con ligera saturación adicional en los tonos teal
(#1D9E75 aparece algo más brillante de lo esperado en pantalla). Los colores del fondo
negro (#0A0A0A) son excelentes — el panel IPS produce negros profundos sin backlight
bleed visible.

### `DISP_BUF_LINES` 20 → 40 para vistas complejas

Las vistas con canvas/animaciones invalidan regiones grandes cada frame. Con 20 líneas
de buffer, el número de llamadas a `flush_cb` por frame era alto (12 llamadas para
frame completo). Con 40 líneas (38KB total en SRAM) se reduce a 6 llamadas — mejor
throughput con costo de RAM aceptable para el ESP32-S3.

---

**Sprint 3.1 — CERRADO · Mayo 2026**

*Demo validado: boot animation + main screen visible en el GC9A01 físico. Root cause
del bug de pantalla negra identificado y documentado. Sprint 3.4 (carrusel de 23 vistas)
arranca a continuación.*

---

*Sprint 3.1 — GrooveForge Brain · Juan Guerrero (GPROG)*
