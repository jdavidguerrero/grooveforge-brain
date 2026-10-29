# Grabadora automática del Brain

> **Fecha:** 2026-10-29 · **Estado:** BORRADOR para revisión de Juan · **Rama:** `feat/gemelo-dsp`
> **Para qué:** grabar el hardware real (codec y salida analógica incluidos) preset por preset y clip por clip, **sin
> que Juan esté**, con la misma procedencia que pide `protocolo-captura.md`. Es la mitad "hardware" del gemelo
> (ADR-006 §Revisión); la otra mitad es `gf-render` (`theory/gemelo-fase1-nucleo-dsp.md`).
> **Depende de:** `protocolo-captura.md` §1–§6, `tools/preset-librarian/gf_presets.py` (clase `Brain`, enlace `@GF`
> por USB serie), `tools/paleta-lab/make_test_clips.py`.

## 0. La idea

Hoy una toma es: elegir el preset en el panel, darle grabar en Live, tocar o lanzar el clip, parar, nombrar el archivo
y anotar la ficha. La grabadora hace lo mismo desde un script:

```
para cada (preset, clip):
    cargar el preset en el Brain           ← USB serie, enlace @GF
    grabar la interfaz (48 kHz / 24 bit)   ← Quantum ES 2, entradas 1/2
    mandar clock + Start + el clip MIDI    ← USB MIDI del Brain
    esperar la cola del release, parar
    verificar, recortar, guardar WAV + ficha con hashes
```

Va en **tiempo real** (un clip de 8 compases a 124 BPM son ~16 s más la cola), pero desatendida. 30 presets × 6 clips
≈ 1 h: se deja corriendo.

## 1. Lo que falta en el firmware (decisión de Juan)

El enlace `@GF` sabe exportar, subir (`PUT`) y borrar presets, y CC 18 elige un preset de **capa**. **No hay forma
remota de cargar un preset de engine** ni de saber qué firmware corre. Faltan tres comandos (`ui/preset_link.h` +
el sketch 49):

| Comando | Respuesta | Para qué |
|---|---|---|
| `@GF PLAY <engine> <hex>` | `@GF OK PLAY <engine> <nombre>` | Activa el engine y carga ese preset **en RAM, sin guardarlo** (sin escribir el flash). El sha256 del hex es exactamente lo que sonó |
| `@GF LPLAY <hex>` | `@GF OK LPLAY <nombre>` | Lo mismo para un preset de capa (GFL v2) |
| `@GF HELLO` (ampliado) | `… <build>` | Agrega el commit con que se compiló el firmware (`-DGF_BUILD=<git describe>` en `platformio.ini`), para que la ficha no diga `unknown` |

`PLAY` en vez de "cargar el lugar N" tiene una ventaja: renderiza cualquier preset de la librería del Mac tal cual está,
sin depender de qué hay en los bancos del Brain, y sin gastar escrituras de flash.

Es un cambio chico (~40 líneas + tests nativos de `parse_link_cmd`), pero **es firmware**: hay que flashear (con la
Teensy conectada directo, sin el aislador) y queda pendiente del visto bueno de Juan.

**Alternativa sin firmware:** Bank Select (CC 0 = engine) + Program Change (= lugar del banco). Live también podría
usarlo, pero solo carga lo que ya está guardado en el Brain y no da hash de lo que sonó. Hoy el sketch tampoco maneja
Program Change, así que igual habría que tocar el firmware.

## 2. Montaje

El de `protocolo-captura.md` §1, sin cambios: VOL del Brain en 0 dB, LINE OUT L/R → entradas 1/2 de la Quantum ES 2,
ganancia fija anotada, 48 kHz / 24 bit. **Live cerrado**: la grabadora es la dueña de la interfaz, del MIDI y del
puerto serie del Brain (una sola sesión le habla a la Teensy a la vez; cerrar cualquier monitor serie).

## 3. Una sesión

1. **Hola.** `@GF HELLO`: formato de presets y build del firmware → a la ficha de sesión.
2. **Ruido del montaje.** 5 s con todo conectado y sin notas → `ruido_montaje.wav` (§3.4 del protocolo).
3. **Latencia.** Con un preset de ataque instantáneo, 4 notas aisladas. Se mide el retardo nota MIDI → inicio del audio
   (umbral −40 dBFS sobre el ruido) y se usa la mediana. Sirve para recortar cada toma en el compás 1 exacto.
4. **Nivel.** El preset más fuerte de la lista con su clip: el pico tiene que quedar entre −18 y −6 dBFS. Si no, la
   grabadora **para y le pide a Juan** que mueva la ganancia: nunca la toca ella.
5. **Tomas** (§4), en el orden de la lista.
6. **Resumen**: tomas buenas, repetidas y fallidas, y la ficha de sesión en `metadata/sesiones/`.

## 4. Una toma

1. `CC 123` (all notes off) y `@GF PLAY …`; espera el `OK` y 300 ms para que el engine se asiente.
2. Empieza a grabar con **un compás de pre-roll** en silencio.
3. Manda MIDI Clock (24 ppqn) + Start a 124 BPM (o el del clip) y las notas del clip. Usa un hilo propio con
   `time.perf_counter` (espera activa el último milisegundo), porque el `sleep` del sistema tiene ~1 ms de jitter. El
   clock tiene que llegar para que PUMP y ARP caigan en fase (protocolo §1).
4. Al terminar el clip: Stop y se espera la cola hasta que el RMS de 50 ms caiga a 6 dB del ruido del montaje, con un
   máximo de 6 s.
5. **Verificación** (si algo falla, se repite la toma; a la segunda falla se marca y se sigue):
   - recorte: ninguna muestra ≥ −0,1 dBFS;
   - silencio: un pico < −50 dBFS indica cable, preset o engine equivocado;
   - xruns: el callback de audio no reportó desbordes (`status` de PortAudio);
   - el nombre que devolvió `PLAY` es el del preset pedido.
6. **Recorte:** desde el inicio del clip + la latencia medida hasta el final de la cola. **Sin normalizar** (protocolo
   §3).

**"Seco" y "en contexto" salen de la misma toma.** El protocolo graba solo la pista del Brain también en contexto, y con
el MIDI automatizado las dos tomas serían iguales. El contexto se arma después en Live, mezclando la toma seca con el
loop.

**Capas:** `@GF LPLAY` + el clip del rol tres veces, con el CC de MORPH en 0, 64 y 127 (protocolo §2, "capa híbrida").

## 5. Archivos y ficha

Nombres de `protocolo-captura.md` §4:
`renders/<AAAAMMDD>/<rol>_<nombre-preset>_seco_t<toma>.wav`, y al lado `…_seco_t<toma>.json`:

```json
{
  "source": "hardware",
  "herramienta": "gf-grabadora <commit del repo>",
  "firmware": {"build": "<de HELLO>", "formato_presets": 5},
  "preset": {"engine": 2, "nombre": "GF POCKET BASS", "sha256_raw": "…"},
  "midi": {"ruta": "midi/bass/bass_ghost_notes.mid", "sha256": "…", "bpm": 124},
  "audio": {"sha256": "…", "fs": 48000, "bits": 24, "canales": 2, "duracion_s": 17.3,
            "pico_dbfs": -9.8, "latencia_ms": 6.4, "repeticiones": 0},
  "montaje": {"interfaz": "Quantum ES 2", "ganancia_interfaz": "<anotada por Juan>", "vol_brain_db": 0}
}
```

`gf_snapshot.py` ya sabe leer WAV y hashes. Las tomas de la grabadora entran al manifiesto como cualquier otra.

## 6. Herramienta

`tools/grabadora/gf_grabadora.py`, un solo script con dependencias declaradas en línea (PEP 723) y corrido con
`uv run`, para no instalar nada global:

| Necesidad | Librería |
|---|---|
| Audio de la interfaz | `sounddevice` (PortAudio) + `soundfile` (WAV 24 bit) + `numpy` |
| MIDI al Brain | `mido` + `python-rtmidi` |
| Enlace `@GF` | `pyserial`, reusando la clase `Brain` de `gf_presets.py` (se importa, no se copia) |

```bash
uv run tools/grabadora/gf_grabadora.py sesion --lista lista.json --rol bass --ganancia "11 h"
uv run tools/grabadora/gf_grabadora.py latencia      # solo medir
uv run tools/grabadora/gf_grabadora.py nivel         # solo comprobar la ganancia
```

La lista es un JSON con pares (preset de la librería, clip). Puede salir de una ficha de sesión (`gf_session.py`) con
los ganadores.

## 7. Pruebas

**Sin hardware** (`python3 -m pytest tools/grabadora/tests`):
- un Brain falso (serie simulado) y audio falso (numpy): nombres, ficha, hashes;
- detección de latencia en una señal sintética con retardo conocido (±0,5 ms);
- recorte, silencio y xrun forzados → repetición y marca;
- espera de cola con un decay exponencial sintético.

**Con el Brain (puerta):**
- G1: una toma automática contra la misma toma grabada a mano en Live. Mismo nivel (±0,5 dB RMS) e inicio alineado
  (±1 ms).
- G2: 10 tomas seguidas sin atención, sin fallas, y el WAV de la misma toma repetida con el mismo nivel (±0,5 dB).
  Ojo: **no** bit a bit, porque el ruido analógico es distinto en cada toma.

## 8. Qué no cubre

- Tocar: los clips son de medición o de arreglo, no interpretación. Las partes tocadas a mano se siguen grabando en
  Live.
- El loop de contexto y la mezcla (Live).
- Mover la ganancia de la interfaz: es de Juan.

## Orden

1. Firmware: `PLAY`, `LPLAY` y `HELLO` con build (visto bueno de Juan + flasheo).
2. `gf_grabadora.py` con las pruebas sin hardware.
3. Puertas G1 y G2 con el Brain.
