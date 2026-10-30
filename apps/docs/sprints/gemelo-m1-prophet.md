# Gemelo M1 · Prophet: referencia y extracción del ladder

> 2026-10-29 · En curso · `feat/gemelo-dsp` · Spec aprobado: `specs/gemelo-plugin-v1.md`.
> Theory: `theory/gemelo-plugin-juce.md`; esta nota registra evidencia, no declara completada la puerta M1.

## Referencia antes del cambio

Se fijó el código del Prophet de `9ab538d` y se ejecutó el grafo original con el harness de
`tools/gemelo-baseline/`: 44.100 Hz, bloques de 128, control virtual con ticks de 1 ms antes de cada bloque,
eventos alineados al borde de bloque, semilla interna fija. Los seis PCM y el manifiesto de hashes de código,
compilador y audios están en `apps/firmware-teensy/test/golden/prophet5/`.

Casos: acorde saw, pulse/PWM, POLY-MOD, glide, resonancia y robo de voz. Se comprueba que los archivos sean
repetibles, contengan ataque y apagado y que el mixer del harness conserve suma Q16, saturación y copy-on-write.
Son **referencias digitales simuladas**, no grabaciones del hardware; carecen de preset GFP, SFX, codec y jitter real.

## Primer paso del refactor

`src/dsp/ladder_stage_core.h` reúne el cálculo existente del ladder, drive, compensación y protección contra NaN,
recibiendo la frecuencia de muestreo al construirse. `MoogLadder4P` conserva su interfaz AudioStream, sus secciones
críticas y las conversiones PCM16 por etapa. El cálculo de raíz para el drive permanece fuera de la sección crítica.
El núcleo está todavía en el firmware: no se ha extraído el Prophet completo ni creado `libs/gf-dsp/`.

## Pruebas y límites

- `run.py --compare .../test/golden/prophet5`: cinco casos dentro de −80 dB RMS relativo y −60 dBFS pico; el caso
  resonante exige igualdad byte a byte hasta implementar comparación espectral/de envolvente. En el Mac de esta
  ejecución los seis PCM fueron idénticos byte a byte a los de la referencia anterior.
- Test nativo de la etapa comprueba salida finita y respuesta a 500 Hz consistente dentro de 2% entre 44,1, 48 y
  96 kHz, con cutoff 1 kHz. No cubre todas las frecuencias, resonancias ni CPU en Teensy.
- `pio test -e native`: **155/155** pruebas aprobadas después del cambio. `pio run -e sketch49`: build exitoso sin
  flashear. PlatformIO usó su core temporal y la copia local de Unity para evitar escrituras globales/descargas.
- CPU y escucha A/B en la Teensy: **pendientes**. `pio device list` no mostró un puerto Teensy en esta sesión. No se
  ha flasheado ni medido hardware.

## Siguiente trabajo

1. Completar comparación de envolvente/espectro para el caso resonante, sin tratar la igualdad byte a byte como
   condición necesaria universal.
2. Extraer control, voces, osciladores y VCA del Prophet en incrementos con comparaciones después de cada cambio;
   preservar cuantización y orden de mezcla. Crear adaptador Teensy y runner nativo del core completo.
3. Medir CPU/escucha en hardware y probar bloques variables antes de dar M1 por concluido. Solo después iniciar M2.
