# Studio · Home de una sola vista: PERFORM con onda y filtro animados

> **Fecha:** 2026-10-19 · **Estado:** APROBADO por Juan ("dejemos solo un modo perform … el modo spectrum fusionado …
> la onda y cutoff y resonance que se muestre de manera animada … reducir el engine") · **Depende de:**
> theory/04 (navegación en el Teensy), fidelidad-engines.md §11.7 (PANEL).

## 1. Por qué

Probado en el hardware, la home tiene dos vistas que no aportan todavía:

- **SPECTRUM** dibuja barras de 32 bandas que nadie manda: el Teensy no tiene comando de espectro (`02` §2.10,
  pendiente), así que con el bridge conectado queda vacía. Solo en el demo hay datos (inventados).
- **AI** y los chips de PERFORM (KEY / SCALE, CHORD, BPM, GENRE) muestran resultados de ML que el Brain todavía no
  produce: son para cuando se conecte **GroovePilot**.

## 2. Qué cambia

| Antes | Después |
|---|---|
| Home: PERFORM → AI → SPECTRUM → PANEL | Home: **PERFORM → PANEL** |
| PERFORM: card del engine grande, 2 diales, franja de espectro, 4 chips de AI | PERFORM: card del engine compacta, CUTOFF y RES como lecturas con barra, **visualización animada** de la onda y del filtro |
| SPECTRUM y AI en la rotación de NAV | Fuera de la rotación. El código queda (vuelven con GroovePilot / el comando de espectro) |

## 3. La visualización (sin datos nuevos del Teensy)

Se calcula en la cabeza con lo que ya llega por PARAM_CHANGED (theory/04 §5), así que no hace falta otro comando ni
más ancho de banda en el UART:

- **Onda (arriba):** la forma del oscilador principal del engine (onda de la página OSC: SAW / SQR / PULSE con su
  PW, SHAPE continua del OB-6, SAW / PULSE del Prophet, FM de 2 operadores con el ratio y el nivel del DX7) a una
  nota de referencia de 110 Hz, **pasada por un filtro de 4 polos con realimentación** al cutoff y la resonancia
  actuales. Al bajar el cutoff la onda se redondea; al subir la resonancia aparece el "ringing" a la frecuencia de
  corte, como en un osciloscopio a la salida de un synth real. Es ilustrativa: no es el audio medido.
- **Respuesta del filtro (abajo):** la misma curva que usaba SPECTRUM (`engine_filter_db`: 24 dB/oct o el SVF del
  OB-6 con su morph), con la línea del cutoff y la etiqueta de RES.
- **Animación:** las dos se mueven con el **LFO → CUTOFF** (la fase del LFO ya se integra en la cabeza) y con el
  **LFO → PWM**; la onda corre despacio (fase) para que se lea viva. Con el LFO en cero y sin tocar nada, solo corre
  la fase.
- **Costo en la Pi:** se redibuja a ~30 Hz solo el área de la gráfica (520 × 328); la onda son 256 puntos.

## 4. Navegación (Teensy)

`studio::VIEWS` pasa a {PERFORM, PANEL}. NAV en PERFORM va a PANEL; en PANEL, NAV recorre las tarjetas y al pasar
los bordes vuelve a PERFORM. Las luces del anillo en home muestran 2 posiciones. La página AI (1) y SPECTRUM (2)
siguen en el enum para no cambiar los ids del protocolo.

## 5. Qué no cubre

El espectro real (necesita el comando de `02` §2.10 y un FFT en el Teensy), la onda real medida a la salida y los
datos de GroovePilot.
