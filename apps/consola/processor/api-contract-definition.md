# 📖 API Contract Definition: PushClone

Guía de referencia entre el script Remote Script "PushClone" y el firmware del controlador (Teensy/STM32/etc). Incluye el flujo de conexión, el formato SysEx y la tabla oficial de comandos **vigentes** (los mismos que se definen en `consts.py`).

---

## 1. Handshake y Ciclo de Conexión

El script envía un saludo activo en cuanto se carga y repite el intento hasta que el hardware responda.

1. **Live → Hardware (`CMD_HANDSHAKE = 0x00`)**  
   Payload recomendado: `[0x50, 0x43]` ("PC" = PushClone).  
   Ejemplo (SEQ=0): `F0 7F 00 7F 00 00 00 02 50 43 13 F7`.

2. **Hardware → Live (`CMD_HANDSHAKE = 0x00`)**  
   El firmware responde con su propio ID (p. ej. `[0x54, 0x53]` = "TS"). El script valida la longitud y reinicia el temporizador de handshake.

3. **Live → Hardware (`CMD_HANDSHAKE_REPLY = 0x01`)**  
   Confirmación final con payload `[0x4C, 0x56]` ("LV"). Ejemplo: `F0 7F 00 7F 01 00 00 02 4C 56 1B F7`.

4. **Estado completo**  
   Tras confirmar el saludo, `PushClone` llama a `_send_complete_state()` y transmite la foto completa de transporte, tracks, clips y grid.

5. **Desconexión limpia (`CMD_DISCONNECT = 0x02`)**  
   Cuando Ableton cierra el script, envía `CMD_DISCONNECT` con payload vacío para que el hardware limpie LEDs y estados.

> El hardware puede iniciar el proceso enviando `CMD_HANDSHAKE` primero; cualquier lado que reciba el comando responde con `CMD_HANDSHAKE_REPLY` para cerrar el ciclo.

---

## 1.5 Formato General de los SysEx PushClone

Todos los mensajes usan la misma envoltura de 7 bits:

```
F0 7F 00 7F [CMD] [SEQ] [LEN_MSB] [LEN_LSB] [PAYLOAD ...] [CHECKSUM] F7
```

| Campo | Bytes | Descripción |
| --- | --- | --- |
| `F0 7F 00 7F` | 4 | Cabecera Universal Non-Commercial. |
| `[CMD]` | 1 | Identificador del comando (ver tablas de la sección 4). |
| `[SEQ]` | 1 | Contador (0‑127). Sirve para depurar pérdidas. |
| `[LEN_MSB][LEN_LSB]` | 2 | Longitud del payload en 14 bits: `len = (MSB << 7) | LSB`. |
| `[PAYLOAD]` | N | Datos codificados en 7 bits (0‑127). |
| `[CHECKSUM]` | 1 | XOR de `CMD`, `SEQ` y todos los bytes del payload, enmascarado con `0x7F`. |
| `F7` | 1 | Fin del SysEx. |

**Recordatorio:** el firmware debe leer siempre ambos bytes de longitud. Mensajes como `CMD_GRID_UPDATE (0x60)` llevan 192 bytes de payload y dependen del campo de 14 bits.

---

## 2. Volcado de Estado Inicial

Tras el handshake, Live envía una ráfaga ordenada de comandos para sincronizar al controlador:

- **Transporte (0x40‑0x49)**  
  `CMD_TRANSPORT_PLAY`, `CMD_TRANSPORT_RECORD`, `CMD_TRANSPORT_LOOP`, `CMD_TRANSPORT_TEMPO`, `CMD_TRANSPORT_SIGNATURE`, `CMD_TRANSPORT_POSITION`, `CMD_TRANSPORT_METRONOME`, `CMD_TRANSPORT_STATE`.  Todos incluyen flags simples `[0/1]` salvo tempo/firma que llevan enteros de 7 bits.

- **Mezclador y Tracks (0x20‑0x2F)**  
  Por cada pista visible se envían `CMD_TRACK_NAME`, `CMD_TRACK_COLOR`, `CMD_MIXER_VOLUME/PAN/MUTE/SOLO/ARM`, `CMD_TRACK_PLAYING_SLOT`, `CMD_TRACK_FIRED_SLOT` y `CMD_CPU_USAGE` (si está habilitado).  Los nombres se limitan a 12 caracteres UTF‑8.

- **Clips y Escenas (0x10‑0x1D + streaming 0x92/0x95/0x96)**  
  `ClipManager` publica `CMD_CLIP_STATE`, `CMD_CLIP_NAME`, `CMD_CLIP_LOOP`, `CMD_CLIP_MUTED`, `CMD_CLIP_WARP`, `CMD_CLIP_START/END`, `CMD_SCENE_*`.  Cuando un clip entra en reproducción se habilita el stream `CMD_CLIP_PLAYING_POSITION (0x92)` y `CMD_CLIP_IS_RECORDING (0x96)`.

- **Grid del Session Ring (0x60/0x61)**  
  `CMD_GRID_UPDATE` entrega los 32 pads de la ventana 8×4 en un solo paquete; cambios puntuales se envían con `CMD_GRID_SINGLE_PAD`.

- **Navegación (0x00‑0x0F)**  
  `CMD_RING_POSITION`, `CMD_SELECTED_TRACK`, `CMD_SELECTED_SCENE`, `CMD_DETAIL_CLIP` mantienen alineada la pantalla/touchscreen.

Tu firmware debe almacenar este estado para que LEDs y UI reflejen exactamente lo que Live muestra.

---

## 3. Protocolo del Grid 8×4

- **Comando completo:** `CMD_GRID_UPDATE (0x60)`  → payload de 192 bytes (32 pads × 6 bytes RGB codificados en 7 bits).  El orden de los pads es por columnas (tracks) y filas (scenes) siguiendo la sesión activa.
- **Actualización puntual:** `CMD_GRID_SINGLE_PAD (0x61)`  → `[pad_index, r, g, b]` en rango 0‑127 (compact RGB).
- **Eventos de usuario:** `CMD_GRID_PAD_PRESS (0x62)`  → `[pad_index, state]` (`state=1` press, `0` release).  PushClone enruta estos eventos al `ClipManager` para disparar clips, cambiar colores y propagar `CMD_TRACK_FIRED_SLOT`/`CMD_TRACK_PLAYING_SLOT`.

### Codificación RGB (24 bits seguro para SysEx)

Cada pad usa 6 bytes (dos por canal):
```
R_msb = (red  >> 7) & 0x7F
R_lsb =  red        & 0x7F
G_msb = (green>> 7) & 0x7F
G_lsb =  green      & 0x7F
B_msb = (blue >> 7) & 0x7F
B_lsb =  blue       & 0x7F
```

### Ejemplo en C++ (Teensy)

```cpp
void handleGridUpdate(const uint8_t* payload, size_t length) {
    if (length != 192) return; // 32 pads * 6 bytes
    for (uint8_t pad = 0; pad < 32; ++pad) {
        size_t offset = pad * 6;
        uint8_t red   = (payload[offset + 0] << 7) | payload[offset + 1];
        uint8_t green = (payload[offset + 2] << 7) | payload[offset + 3];
        uint8_t blue  = (payload[offset + 4] << 7) | payload[offset + 5];
        grid.setPixelColor(pad, grid.Color(red, green, blue));
    }
    grid.show();
}
```

En tu parser principal:

```cpp
if (command == CMD_GRID_UPDATE) {
    handleGridUpdate(payload, payload_len);
} else if (command == CMD_GRID_SINGLE_PAD) {
    // ...
}
```

---

## 4. Tabla de Comandos Vigentes

Las siguientes tablas siguen exactamente los valores de `consts.py`. "Dir" indica el sentido recomendado.

### 4.1 Sistema y Navegación (0x00‑0x0F)

| Comando | Hex | Dir | Payload |
| --- | --- | --- | --- |
| `CMD_HANDSHAKE` | `0x00` | Bidireccional | `[id_hi, id_lo]` |
| `CMD_HANDSHAKE_REPLY` | `0x01` | Live → HW | `[0x4C, 0x56]` |
| `CMD_DISCONNECT` | `0x02` | Live → HW | — |
| `CMD_PING_TEST` | `0x03` | Bidireccional | `[0x01/0x02]` |
| `CMD_SWITCH_VIEW` | `0x04` | HW → Live | `[view_id]` |
| `CMD_VIEW_STATE` | `0x05` | Live → HW | Snapshot compacto |
| `CMD_SELECTED_TRACK` | `0x06` | Live → HW | `[track_idx]` |
| `CMD_SELECTED_SCENE` | `0x07` | Live → HW | `[scene_idx]` |
| `CMD_DETAIL_CLIP` | `0x08` | Live → HW | `[track, scene]` |
| `CMD_BROWSER_MODE` | `0x09` | Live → HW | `[mode]` |
| `CMD_RING_NAVIGATE` | `0x0A` | HW → Live | `[dx, dy]` |
| `CMD_RING_SELECT` | `0x0B` | HW → Live | `[track, scene]` |
| `CMD_RING_POSITION` | `0x0C` | Live → HW | `[track_start, scene_start, width, height]` |
| `CMD_TRACK_SELECT` | `0x0D` | HW → Live | `[track_idx]` |
| `CMD_SCENE_SELECT` | `0x0E` | HW → Live | `[scene_idx]` |
| `CMD_SESSION_MODE` | `0x0F` | Live → HW | `[mode_id]` |

### 4.2 Clips y Escenas (0x10‑0x1D)

| Comando | Hex | Dir | Payload |
| --- | --- | --- | --- |
| `CMD_CLIP_STATE` | `0x10` | Live → HW | `[track, scene, state, rgb*]` |
| `CMD_CLIP_TRIGGER` | `0x11` | HW → Live | `[track, scene]` |
| `CMD_SCENE_FIRE` | `0x12` | HW → Live | `[scene]` |
| `CMD_CLIP_STOP` | `0x13` | HW → Live | `[track, scene]` |
| `CMD_CLIP_NAME` | `0x14` | Live → HW | `[track, scene, len, utf8…]` |
| `CMD_CLIP_LOOP` | `0x15` | Live → HW | `[track, scene, flag]` |
| `CMD_CLIP_MUTED` | `0x16` | Live → HW | `[track, scene, flag]` |
| `CMD_CLIP_WARP` | `0x17` | Live → HW | `[track, scene, flag]` |
| `CMD_CLIP_START` | `0x18` | Live → HW | `[track, scene, hi, lo]` |
| `CMD_CLIP_END` | `0x19` | Live → HW | `[track, scene, hi, lo]` |
| `CMD_SCENE_STATE` | `0x1A` | Live → HW | `[scene, flags]` |
| `CMD_SCENE_NAME` | `0x1B` | Live → HW | `[scene, len, utf8…]` |
| `CMD_SCENE_COLOR` | `0x1C` | Live → HW | `[scene, r, g, b]` |
| `CMD_SCENE_IS_TRIGGERED` | `0x1D` | Live → HW | `[scene, flag]` |

### 4.3 Mixer y Tracks (0x20‑0x2F)

| Comando | Hex | Dir | Descripción |
| --- | --- | --- | --- |
| `CMD_MIXER_STATE` | `0x20` | Live → HW | Snapshot por pista |
| `CMD_MIXER_VOLUME/PAN/MUTE/SOLO/ARM` | `0x21‑0x25` | Bidireccional | `[track, value]` |
| `CMD_MIXER_SEND` | `0x26` | Bidireccional | `[track, send_idx, value]` |
| `CMD_TRACK_NAME` | `0x27` | Live → HW | `[track, len, utf8…]` |
| `CMD_TRACK_COLOR` | `0x28` | Live → HW | `[track, r, g, b]` |
| `CMD_TRACK_PLAYING_SLOT` | `0x29` | Live → HW | `[track, slot]` |
| `CMD_TRACK_FIRED_SLOT` | `0x2A` | Live → HW | `[track, slot] (127=none)` |
| `CMD_TRACK_FOLD_STATE` | `0x2B` | Live → HW | `[track, flag]` |
| `CMD_TRACK_CROSSFADE` | `0x2C` | Bidireccional | `[track, assign]` |
| `CMD_CPU_USAGE` | `0x2F` | Live → HW | `[avg, peak]` |

### 4.4 Device & Plugin (0x30‑0x3F)

| Comando | Hex | Dir | Descripción |
| --- | --- | --- | --- |
| `CMD_DEVICE_LIST` | `0x30` | Live → HW | Lista de dispositivos por pista |
| `CMD_DEVICE_SELECT` | `0x31` | HW → Live | `[track, device]` |
| `CMD_DEVICE_PARAMS` | `0x32` | Live → HW | Dump de parámetros |
| `CMD_PARAM_CHANGE / VALUE` | `0x33 / 0x34` | Bidireccional / Live → HW | Actualización de parámetros |
| `CMD_DEVICE_ENABLE` | `0x35` | Bidireccional | `[track, device, flag]` |
| `CMD_DEVICE_PREV_NEXT` | `0x36` | HW → Live | `[direction]` |
| `CMD_PARAM_PAGE` | `0x37` | HW → Live | `[delta]` |
| `CMD_RACK_MACRO` | `0x38` | Bidireccional | `[track, device, macro, value]` |
| `CMD_DEVICE_CHAIN` / `CMD_CHAIN_*` | `0x39‑0x3F` | Variado | Gestión de racks/Chains |

### 4.5 Transporte (0x40‑0x4F)

Ver tabla en la sección 2 (mismos comandos). Todos los toggles usan payload `[flag]`.

### 4.6 Notas y Step Sequencer (0x50‑0x5F)

Incluye `CMD_NOTE_ON/OFF`, `CMD_SCALE_CHANGE/INFO`, `CMD_OCTAVE_CHANGE/INFO` y toda la familia del step sequencer (`CMD_STEP_SEQUENCER_*`, `CMD_STEP_EDIT_PARAMS`, `CMD_LOOP_MARKERS`).

### 4.7 Grid, Groove y Quantize (0x60‑0x6F)

`CMD_GRID_UPDATE`, `CMD_GRID_SINGLE_PAD`, `CMD_GRID_PAD_PRESS`, `CMD_SESSION_OVERVIEW`, `CMD_DRUM_RACK_STATE`, `CMD_DRUM_PAD_STATE`, `CMD_GROOVE_AMOUNT/TEMPLATE/POOL`, `CMD_RECORD_QUANTIZATION`, `CMD_TRANSPORT_QUANTIZE`, `CMD_MIDI_CLIP_QUANTIZE`, `CMD_QUANTIZE_CLIP`, `CMD_QUANTIZE_NOTES`, `CMD_CUE_POINT`.

### 4.8 Acciones de Canción y Clips (0x70‑0x7F)

`CMD_CREATE_*`, `CMD_DUPLICATE_*`, `CMD_CLIP_DELETE/COPY/PASTE`, resultados (`CMD_CLIP_*_RESULT`), además de `CMD_MIDI_NOTES`, `CMD_MIDI_NOTE_ADD`, `CMD_MIDI_NOTE_REMOVE`.

### 4.9 Streaming/Realtime (0x90‑0x96)

| Comando | Hex | Dir | Descripción |
| --- | --- | --- | --- |
| `CMD_TRACK_METER` | `0x90` | Live → HW | `[track, level]` cada 50 ms. |
| `CMD_TRACK_CUE_VOLUME` | `0x91` | Bidireccional | `[value]` (0‑127). |
| `CMD_CLIP_PLAYING_POSITION` | `0x92` | Live → HW | `[track, scene, hi, lo]`. |
| `CMD_CLIP_LOOP_START / END` | `0x93 / 0x94` | Bidireccional | `[track, scene, hi, lo]`. |
| `CMD_CLIP_LENGTH` | `0x95` | Live → HW | `[track, scene, hi, lo]`. |
| `CMD_CLIP_IS_RECORDING` | `0x96` | Live → HW | `[track, scene, flag]`. |

---

## 5. Recordatorios de Implementación

- El grid físico representa la ventana del Session Ring (8 tracks × 4 escenas). Usa `CMD_RING_POSITION` para saber qué porción del set debes monitorear y pide nombres/estados a `ClipManager.ensure_region_monitored(...)` si cambia.
- `CMD_TRACK_PLAYING_SLOT` y `CMD_TRACK_FIRED_SLOT` son la fuente de verdad para LEDs/Touch UI: playing usa el color del clip en verde, queued en amarillo y stopped vuelve al color base del track.
- `CMD_TRANSPORT_PLAY` permanece en `True` aunque detengas un clip individual; sólo pasa a `False` cuando se pulsa Stop global en Live. Implementa un botón dedicado de Stop (manda `CMD_TRANSPORT_PLAY` con `0`).
- Todos los nombres se recortan a 12 bytes UTF‑8; si necesitas textos más largos, combínalos con etiquetas de tu firmware.
- Mantén el parser pendiente de paquetes vacíos (`F0 F7`), pero ignóralos para evitar logs innecesarios.

Con este contrato tu firmware queda alineado con los comandos que ya ves en los logs (`0x0D`, `0x08`, `0x11`, `0x29`, `0x2A`, `0x40`, `0x45`, `0x92`, etc.).
