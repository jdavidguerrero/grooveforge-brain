# Studio Head — Arquitectura de la cabeza gráfica (Pi 2B + Freenove DSI)

> **Tipo:** theory doc (precede al código, `CLAUDE.md`).
> **Variante:** Brain · Studio (personal, no comercial). Ver spec delta y ADR-002 (pendientes).
> **Specs base que respeta:** `02-bridge-protocol.md` (frame, heartbeat, timeouts, recovery),
> `01-architecture.md` §5.2 (latencias), diseño `variants/studio/design/` (pantallas 08d y 08e).
> **Fecha:** 2026-10-07

---

## 0. Qué resuelve este documento

La cabeza de Studio es una Raspberry Pi 2B con una pantalla táctil DSI de 800×480. Tiene un
solo trabajo: **mostrar el estado del Brain y mandar comandos táctiles**, arrancando rápido y
sin colgarse cuando el enlace con el Teensy falla.

Este doc define el modelo de fallos, la arquitectura de la app, el sistema operativo mínimo,
la supervisión y el camino de desarrollo Pi 5 → Pi 2B. **No** cubre el diseño visual de las
pantallas (eso es el diseño de Claude Design) ni el lado Teensy del bridge (tarea R-C1 del plan
de remediación).

---

## 1. Principio rector: el audio nunca depende de la cabeza

El Teensy es el **maestro** del bridge (`02` §1) y genera el audio solo. La cabeza es
reemplazable: se puede caer, reiniciar o desconectar y el instrumento sigue sonando.

Consecuencia de diseño: "que no se cuelgue" no se resuelve haciendo la cabeza infalible, sino
haciendo que **cada fallo se detecte y se recupere en segundos, sin intervención**.

### Matriz de fallos

| Fallo | Cómo se detecta | Recuperación | Tiempo objetivo |
|---|---|---|---|
| Cable UART suelto / Teensy reiniciado | sin heartbeat por >3 s (`02` §4.3) | pantalla 08d; al volver el heartbeat → GET_VERSION + pedir estado completo | detección ≤3 s; convergencia <2 s tras volver |
| Frame corrupto (ruido, byte perdido) | CRC-8 inválido o timeout entre bytes | descartar y resincronizar (§4.2) | inmediato |
| Comando sin ACK | timeout de 100 ms en UART (`02` §4.2) | reintento ×3 con el mismo SEQ | ≤400 ms |
| Hilo de UI trabado | watchdog de systemd sin `WATCHDOG=1` | systemd reinicia la app | <3 s |
| App caída (crash) | proceso termina | `Restart=always` | ~1 s |
| Kernel colgado | watchdog de hardware sin ping | reboot del SoC | ~10 s + arranque |
| Corte de energía | — | rootfs de solo lectura: nada que reparar | arranque normal |

---

## 2. Arquitectura de la app: un proceso, dos hilos

### 2.1 Por qué no dos procesos

Separar "bridge" y "UI" en dos procesos aísla fallos, pero agrega IPC (socket o memoria
compartida), serialización y un segundo servicio que supervisar. En una Pi 2B con un solo
desarrollador, la combinación **un proceso + dos hilos + watchdog de systemd** da casi el mismo
aislamiento práctico (si algo se traba, el watchdog reinicia todo en ~1 s, y el reinicio es
barato porque el estado verdadero vive en el Teensy) con mucha menos superficie.

### 2.2 Roles

```
           UART 921600 8N1 (/dev/ttyAMA0)
Teensy ◀──────────────────────────────▶ ┌─────────────────────────────┐
                                        │ HILO I/O  (SCHED_FIFO)      │
                                        │ · poll() sobre el UART       │
                                        │ · parser con resync + CRC-8  │
                                        │ · contesta heartbeat/ACK     │
                                        │ · escribe en StudioState     │
                                        │ · vacía la cola de salida    │
                                        └──────────┬──────────────────┘
                              estado (mutex corto) │ ▲ comandos (cola acotada)
                                        ┌──────────▼──┴───────────────┐
 Touch (evdev) ───────────────────────▶ │ HILO UI  (LVGL, 30 fps)     │
 Pantalla DSI (DRM/fbdev) ◀──────────── │ · snapshot del estado/frame  │
                                        │ · lv_timer_handler()         │
                                        │ · touch → comando a la cola │
                                        └─────────────────────────────┘
```

### 2.3 Las cinco reglas

1. **LVGL es de un solo hilo.** Solo el hilo de UI crea, modifica o lee objetos LVGL. El hilo de
   I/O no incluye `lvgl.h`. (LVGL 8.3 no es thread-safe; ver docs "Threading considerations".)
2. **El hilo de I/O nunca espera al render.** Heartbeats y ACKs se responden desde el hilo de
   I/O, aunque la UI esté dibujando. Así un frame lento nunca dispara un timeout del Teensy.
3. **"El último valor gana", no una cola de mensajes.** Ver §3.
4. **Toda cola es acotada.** La de salida (comandos táctiles) tiene tamaño fijo; si se llena,
   se descarta lo más viejo del mismo parámetro. Nunca crece sin límite.
5. **El touch se limita.** Un arrastre manda como máximo ~50 comandos/s, solo con el valor
   vigente de cada intervalo de 20 ms.

---

## 3. Modelo de estado: por qué no encolar mensajes

### 3.1 El problema de las tasas

El Teensy produce actualizaciones a ritmos muy distintos: un encoder girado rápido puede
generar cientos de cambios por segundo; el espectro llegaría a 30 Hz; GROOVE_STATE a 4 Hz
(`02` §2.9). La UI consume a 30 fps.

Si cada mensaje se encola y la UI lo procesa uno por uno, basta con que la tasa de llegada
supere un rato a la de servicio para que la cola crezca sin parar (en teoría de colas: con
utilización ρ = λ/μ ≥ 1 el backlog diverge). Eso se ve como una UI que "se va quedando atrás"
hasta parecer colgada.

### 3.2 La solución: estado + dirty flags

```cpp
// Estado espejo del Brain en la cabeza. POD: se copia entero en un frame.
struct StudioState {
    uint8_t  engine;            // 0–5
    uint16_t cutoff_hz;
    uint8_t  resonance_pct;
    bool     filter_bypass;
    uint8_t  key, key_conf;     // 0x80 KEY_DETECTED
    uint16_t chord; uint8_t chord_conf;
    uint16_t bpm_x10; uint8_t beat_phase;
    uint8_t  chroma[12];        // 0x83 GROOVE_STATE
    uint8_t  spectrum[32];      // pendiente de la enmienda a 02 (rango 0x90–0xEF)
    LinkState link;             // CONNECTING / UP / LOST
    uint32_t dirty;             // bit por región de pantalla
};
```

- El hilo de I/O decodifica cada frame y **sobrescribe** el campo correspondiente, marcando su
  bit en `dirty`.
- El hilo de UI, una vez por frame, toma el mutex, **copia** el struct (unos cientos de bytes:
  30 copias/s son despreciables), limpia `dirty` y suelta el mutex. Después dibuja desde la
  copia, sin locks.
- Solo se redibujan las regiones con su bit en `dirty`.

El mutex se sostiene microsegundos, así que no hace falta un seqlock ni estructuras lock-free.
Si algún día hiciera falta, `StudioState` ya es POD y el cambio es local.

**Eventos discretos** (un snap de Scale Lock, un toast de bypass) sí van por una cola chica y
acotada, porque cada uno debe mostrarse aunque lleguen dos seguidos.

### 3.3 Convergencia tras reconectar

Como la UI solo dibuja `StudioState`, recuperarse de un corte es: marcar `link = LOST`
(pantalla 08d), esperar el heartbeat, pedir el estado completo y volver a `link = UP`. No hay
historia que reconstruir.

---

## 4. El bridge del lado de la cabeza

### 4.1 Lo que exige `02-bridge-protocol.md`

| Regla del spec | Valor | Sección |
|---|---|---|
| Frame | `[CMD][LEN][SEQ][PAYLOAD 0–255][CRC8]` | §1 |
| CRC | CRC-8 poly 0x07 | §1 |
| Heartbeat | 1 Hz | §2.1 |
| Timeout de comando (UART) | 100 ms, reintento ×3 con el mismo SEQ | §4.2 |
| Enlace perdido | sin heartbeat por >3 s → dejar de enviar | §4.3 |
| Recuperación | al volver el heartbeat → GET_VERSION | §4.3 |

La cabeza reutiliza `apps/bridge-protocol/include/` tal cual (serialización y CRC), igual que el
ESP32. **Dependencia:** el lado Teensy de retry/recovery/resync todavía no está implementado
(auditoría P1-3, tarea R-C1). Sin R-C1, la cabeza detecta el corte pero el Teensy no se re-anuncia.

### 4.2 Parser con resincronización

El parser es una máquina de estados byte a byte. Lo que lo hace robusto:

- **Timeout entre bytes.** A 921600 baud un byte tarda ~10.9 µs y el frame más largo (259 bytes)
  ~2.8 ms. Si dentro de un frame pasan más de 5 ms sin bytes, el frame se da por perdido y el
  parser vuelve a esperar un CMD.
- **Resync deslizante.** Si el CRC falla, no se descarta todo el buffer: se avanza un byte y se
  vuelve a intentar parsear desde ahí. Un byte de basura cuesta como máximo un frame.
- **Validación de LEN y CMD.** Un CMD fuera de los rangos de `02` §2 o un LEN imposible se tratan
  igual que un CRC inválido.

### 4.3 El UART en Linux

**Por qué el PL011 y no el mini-UART.** La Pi tiene dos UART: el PL011 (completo, con su propio
reloj) y el mini-UART (cuyo baudrate deriva del clock del core, que cambia con la frecuencia de la
GPU). A 921600 el mini-UART no es confiable. La Pi 2B **no tiene Bluetooth**, así que el PL011
ya está en GPIO14/15 como `/dev/ttyAMA0`. Solo hay que liberar la consola serial:
borrar `console=serial0,115200` de `cmdline.txt` y deshabilitar `serial-getty@ttyAMA0`.

**¿Llega el PL011 a 921600?** El baudrate sale de un divisor fraccional sobre UARTCLK
(ARM PL011 TRM, §3.3.6). Con UARTCLK = 48 MHz (verificar con `vcgencmd measure_clock uart`):

```
BAUDDIV = 48 000 000 / (16 × 921 600) = 3.2552
IBRD = 3,  FBRD = round(0.2552 × 64) = 16
baud real = 48 000 000 / (16 × (3 + 16/64)) = 923 077  →  error +0.16 %
```

Un error de 0.16 % está muy por debajo del ~2 % que tolera un UART 8N1. Funciona.

**Configuración:** termios en modo raw (`cfmakeraw`), `B921600`, `CLOCAL | CREAD`, sin control
de flujo, `VMIN = 0, VTIME = 0` y `poll()` con timeout corto. Teensy y Pi trabajan a 3.3 V:
conexión directa TX↔RX cruzados más GND común.

**Prioridad.** El hilo de I/O corre con `SCHED_FIFO` (prioridad ~50) y la app hace
`mlockall(MCL_CURRENT | MCL_FUTURE)` para que una página paginada no frene el UART. Esto ataca la
desviación de latencia ya señalada: `02` §7.3 pide round-trip <2 ms p95, y Linux sin RT puede
meter jitter. Con esto debería alcanzar; **se mide antes de dar por cumplido el target**.

---

## 5. Render en la Pi 2B

### 5.1 El presupuesto de ancho de banda

| Caso | Bytes por frame (RGB565) | A 30 fps |
|---|---|---|
| Pantalla completa 800×480 | 768 000 | ~23 MB/s |
| Solo la franja de espectro 768×88 | 135 168 | ~4 MB/s |
| Igual a 32 bits (ARGB8888) | el doble en cada caso | — |

La Pi 2B puede con la pantalla completa ocasional (transición), pero el estado estable tiene que
ser **refresco parcial**: solo el espectro y los medidores se redibujan cada frame. Por eso:

- `LV_COLOR_DEPTH 16` y framebuffer de 16 bpp. El diseño ya pide assets RGB565 opacos.
- Imágenes pre-renderizadas al tamaño final (sin escalar en runtime).
- `LV_USE_PERF_MONITOR 1` durante el desarrollo, para ver fps y CPU en pantalla.

### 5.2 DRM o fbdev

| | fbdev | DRM/KMS |
|---|---|---|
| Complejidad | mínima (`lv_drivers/display/fbdev`) | media (`lv_drivers/display/drm`) |
| Tearing | posible | no, con page flip sincronizado a vsync |
| Recomendación | para arrancar | **objetivo**: el tearing se ve en cámara |

### 5.3 CPU

- Governor en `performance` (900 MHz fijos): evita la rampa de frecuencia en cada animación.
- El hilo de UI y el de I/O pueden fijarse a núcleos distintos (`pthread_setaffinity_np`); se
  decide después de medir, no antes.

### 5.4 Gráficas de alta tasa: objetos, canvas o dibujo directo

FX MAIN (diseño *FX Viz*) dibuja un osciloscopio de 120 columnas × 2 trazas, un espectro dual de
32 bandas o una "firma" del efecto, todo a 30 Hz en 728×256. Hay tres formas de hacerlo en LVGL 8:

| Técnica | Memoria | Costo por frame | Cuándo usarla |
|---|---|---|---|
| Un `lv_obj` por elemento (barras de SPECTRUM) | ~100 B por objeto | LVGL recalcula y redibuja solo lo que cambió | Pocos elementos (≤ ~100) con geometría estable |
| `lv_canvas` (gráfica de ENV) | W×H×2 B (728×256 = 373 KB) | Se pinta el buffer y LVGL lo copia como imagen | Contenido que cambia poco: se pinta una vez y se reutiliza |
| Dibujo directo en `LV_EVENT_DRAW_MAIN` | 0 B extra | Se emiten `lv_draw_rect` directo al buffer de render, recortados al área sucia | Contenido que cambia todos los frames |

Para ONDA y ESPECTRO todo cambia en cada frame, así que un canvas pagaría dos veces (pintar el
buffer y copiarlo) y 240 objetos costarían más en bookkeeping que en píxeles. **Dibujo directo**:
el objeto se invalida a 30 Hz y su callback emite rects sólidos sin antialiasing, que es
exactamente lo que pide el diseño ("todo con `lv_draw_rect` relleno, sin AA, sin `lv_line` ni
arcos"). Como LVGL renderiza en franjas (el buffer parcial es de 800×80), el callback se llama una
vez por franja y cada `lv_draw_rect` que cae fuera del área de recorte se descarta barato.

Las firmas que solo dependen de parámetros (curva de transferencia, ecos, envolventes) podrían
cachearse en un canvas, como sugiere el diseño. Hoy se recalculan en cada frame porque el costo
medido en la Pi 5 es despreciable; se cachean solo si la medición en la Pi 2B lo pide (§11).

---

## 6. Input táctil

- `lv_drivers/indev/evdev` lee el dispositivo de input del panel. No se usa un número fijo
  (`/dev/input/event3`): una regla udev crea un symlink estable (ver §8.3).
- **Política de fase 1** (acordada para la variante): todas las funciones se operan con los
  controles físicos del Teensy; el touch es un atajo (seleccionar engine, navegar). Targets de
  ≥64 px según el diseño.
- Los comandos táctiles usan frames que ya existen en `02` (SET_ENGINE, SET_PARAM,
  FX_SET_PARAM) y respetan la regla 5 de §2.3.

---

## 7. Arranque rápido

### 7.1 Dónde se va el tiempo

```
Encendido → bootcode/start.elf (GPU, ~2–3 s) → kernel (~2–3 s) → systemd → app → splash
            fijo                                  recortable      MUY recortable
```

El firmware de la GPU es fijo. El kernel se recorta poco. **Casi toda la ganancia está en
userspace**: lo que systemd arranca antes de la app.

### 7.2 Etapa 1 — Raspberry Pi OS Lite recortado (meta: UI operativa en ≤12 s)

- `config.txt`: `disable_splash=1`, `boot_delay=0`, `dtparam=audio=off`, overlay KMS
  (`vc4-kms-v3d`) más el overlay que indique Freenove para el panel.
- `cmdline.txt`: `quiet loglevel=3 logo.nologo fastboot noswap` y sin `console=serial0`.
- Deshabilitar: `NetworkManager-wait-online`, `avahi-daemon`, `triggerhappy`, `ModemManager`,
  `dphys-swapfile`, `apt-daily*.timer`, `man-db.timer`, `keyboard-setup`.
- Rootfs de solo lectura (overlay FS de `raspi-config`) y `journald` con `Storage=volatile`.
- SD clase A1/A2.
- Medir siempre con `systemd-analyze`, `systemd-analyze blame` y
  `systemd-analyze critical-chain studio-head.service`.

### 7.3 Etapa 2 — Buildroot (solo si la Etapa 1 no alcanza)

Imagen mínima con la app casi como único proceso: ~4–6 s. Cuesta mantener una distro propia.
No se empieza por acá.

### 7.4 No hace falta kernel PREEMPT_RT

El tiempo real del instrumento vive en el Teensy (`01` §5.2: audio <1 ms determinístico). La
cabeza necesita latencia de UI (decenas de ms) y un UART bien atendido (§4.3), no un kernel RT.

---

## 8. Supervisión con systemd

### 8.1 El servicio

```ini
[Unit]
Description=GrooveForge Studio head
DefaultDependencies=no
After=local-fs.target dev-ttyAMA0.device dev-dri-card0.device
Wants=dev-ttyAMA0.device dev-dri-card0.device

[Service]
Type=notify
ExecStart=/opt/studio-head/studio-head --config /etc/studio-head.conf
Restart=always
RestartSec=500ms
WatchdogSec=2
LimitRTPRIO=50
LimitMEMLOCK=infinity

[Install]
WantedBy=sysinit.target
```

- `Type=notify`: la app avisa `READY=1` cuando la primera pantalla está dibujada.
- `WatchdogSec=2`: el **hilo de UI** manda `WATCHDOG=1` (vía `sd_notify`) en cada vuelta del
  loop. Si se traba, systemd la mata y la reinicia. Se elige el hilo de UI a propósito: es el
  que puede trabarse por render; el de I/O está acotado por `poll()`.
- Watchdog de hardware: `RuntimeWatchdogSec=10` en `/etc/systemd/system.conf` (driver
  `bcm2835_wdt`) para el caso de kernel colgado.

### 8.2 Arrancar sin esperar la red

Con `DefaultDependencies=no` y `WantedBy=sysinit.target`, la app no espera red, login ni nada
de multi-user. La red (SSH para desarrollo) sube en paralelo.

### 8.3 Reglas udev para nombres estables

Los device units de DRM no existen por defecto y el número de `eventX` del touch puede cambiar.
Una regla udev resuelve ambas cosas:

```
SUBSYSTEM=="drm", KERNEL=="card[0-9]", TAG+="systemd"
SUBSYSTEM=="input", ATTRS{name}=="<nombre del touch Freenove>", SYMLINK+="studio-touch"
```

El nombre exacto del touch se lee con `evtest` en la Pi.

---

## 9. Desarrollar en la Pi 5, desplegar en la Pi 2B

### 9.1 Diferencias que muerden

| | Pi 5 | Pi 2B |
|---|---|---|
| CPU | Cortex-A76, aarch64 | Cortex-A7 (v1.1) o A53 (v1.2), armhf de 32 bits |
| Velocidad relativa | ~10× | 1× |
| DSI | 22 pines, nodo DRM `card1` | 15 pines, nodo DRM `card0` |
| Binarios | no se copian entre ellas | — |

- **Confirmar la revisión de la 2B:** `grep Revision /proc/cpuinfo` (a01041/a21041 = v1.1 con
  Cortex-A7; a22042 = v1.2 con A53).
- **Compilar para cada una.** CMake con tres toolchains: `native` (Mac/SDL), `aarch64` (Pi 5) y
  `armhf` (Pi 2B: `-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard`). Para la 2B, lo más
  reproducible es un contenedor Docker `linux/arm/v7` con el sysroot de Raspberry Pi OS.
- **La Pi 5 miente sobre el rendimiento.** Todo criterio de §11 se mide en la 2B, desde la
  primera semana.

### 9.2 Capas intercambiables (HAL)

| Capa | Mac | Pi 5 / Pi 2B |
|---|---|---|
| Display | SDL 800×480 | DRM (o fbdev) |
| Input | mouse vía SDL | evdev (`/dev/studio-touch`) |
| Transporte | USB serial del Teensy, o **replay** | `/dev/ttyAMA0` |

El **transporte de replay** reproduce un archivo de frames del bridge grabados de una sesión real
(por el USB serial del Teensy con el sketch 28). Sirve para tres cosas: desarrollar la UI sin
hardware, tests de regresión del parser (incluso con bytes corruptos inyectados), y grabar
contenido de pantalla reproducible.

Nada de rutas fijas en el código: display, input y transporte se eligen por configuración.

---

## 10. Estructura propuesta

```
apps/studio-head/
├── CMakeLists.txt
├── cmake/toolchains/          # aarch64.cmake, armhf.cmake
├── docker/armhf/              # imagen de build para la 2B
├── src/
│   ├── main.cpp               # arranque, hilos, sd_notify
│   ├── state/studio_state.h   # StudioState + dirty flags
│   ├── link/                  # hilo I/O, parser con resync, transporte (uart | replay)
│   ├── hal/                   # display (sdl | drm | fbdev), input (sdl | evdev)
│   └── ui/                    # pantallas del diseño (PERFORM, ENGINE, AI, SPECTRUM…)
├── assets/                    # imágenes RGB565 generadas desde el diseño
├── deploy/                    # studio-head.service, reglas udev, config.txt de referencia
└── test/                      # parser + replay (corre en CI, sin hardware)
```

Comparte `apps/bridge-protocol/include/` con ambos firmwares. Es C++17 + CMake: **excepción a
la regla de PlatformIO**, a registrar en el ADR-002.

---

## 11. Criterios de aceptación (medidos en la Pi 2B)

| Métrica | Meta | Cómo se mide |
|---|---|---|
| Encendido → splash | < 8 s | cronómetro sobre video de la pantalla |
| Encendido → UI con datos | < 12 s (Etapa 1) | ídem + `systemd-analyze` |
| PERFORM con espectro | 30 fps sostenidos | `LV_USE_PERF_MONITOR` |
| CPU del hilo de UI | < 60 % de un núcleo | `top -H` |
| Cortar y reconectar el UART | pantalla 08d en ≤3 s; estado convergido < 2 s tras volver | cronómetro + log |
| Replay con 1 % de bytes corruptos | cero cuelgues; solo se pierden los frames dañados | test automático |
| Hilo de UI trabado a propósito | reinicio por watchdog < 3 s | `kill -STOP` al hilo + log de systemd |
| Round-trip del bridge | < 2 ms p95 (`02` §7.3) | loopback con timestamps |

---

## 12. Qué no cubre

- Kernel RT, Qt, compositor o servidor gráfico (X11/Wayland): la app dibuja directo sobre DRM.
- Frames de espectro: dependen de la enmienda a `02` (rango libre 0x90–0xEF), que necesita
  instrucción explícita.
- El lado Teensy de la robustez del bridge (R-C1).
- La cabeza comercial (ESP32-S3 + GC9A01), que sigue su propio spec.

## 13. Decisiones abiertas (para el ADR-002)

1. Excepción a PlatformIO para `apps/studio-head/`.
2. Si el round-trip medido no cumple <2 ms p95, ¿se relaja solo para Studio?
3. fbdev para arrancar y DRM como objetivo: confirmar.

---

## Referencias

- LVGL 8.3 docs — Porting, Threading considerations, Linux drivers (`lv_drivers`: fbdev, drm, evdev). https://docs.lvgl.io/8.3/
- ARM PrimeCell UART (PL011) Technical Reference Manual (DDI 0183), §3.3.6 Fractional baud rate divider.
- Raspberry Pi documentation — Configuring UARTs; `config.txt`; `cmdline.txt`. https://www.raspberrypi.com/documentation/computers/configuration.html
- systemd: `systemd.service(5)`, `sd_notify(3)`, `systemd-system.conf(5)` (RuntimeWatchdogSec). https://www.freedesktop.org/software/systemd/man/
- Linux man pages: `termios(3)`, `sched(7)`, `mlockall(2)`.
- Buildroot user manual (Etapa 2). https://buildroot.org/docs.html
- Kleinrock, L. *Queueing Systems, Vol. 1* (1975) — estabilidad de colas (ρ < 1), base de §3.1.
