# Consola: USB audio de la Teensy A

> **Fecha:** 2026-11-09. **Estado:** decisión para evaluación; aceptación pendiente en hardware. **Decisión de Juan:** opción A: Brain → USB → Pi (mezcla) → Audio+ PCM5102A. Audio+ conduce el grafo de PipeWire; Teensy lo sigue con remuestreo adaptativo. No modifica los specs SSoT `00–06`.

## 1. Qué tiene que hacer el USB

**IN / OUT se nombran desde el host:** IN = Teensy → Pi; OUT = Pi → Teensy. Isochronous reserva oportunidades de transferencia, pero no retransmite audio perdido. Tener paquetes cada milisegundo no significa tener relojes de audio idénticos.

| Sincronización (USB Audio Class 1) | Relación con el reloj USB |
|---|---|
| Synchronous | El reloj de muestras está ligado al SOF del bus |
| Adaptive | El dispositivo adapta su reloj o su flujo al ritmo del otro extremo |
| Asynchronous | El dispositivo mantiene su reloj de audio independiente |

Para **OUT asíncrono**, un endpoint IN de **feedback** informa cuántas muestras consume el dispositivo por intervalo USB: el host varía lo que envía. En UAC1, el formato típico a Full Speed es 10.14 muestras/frame; High Speed usa otra escala/longitud. El core tiene explícitas ambas ramas (`S/usb_audio.cpp:82–104`). Ese feedback regula **Pi → Teensy**, no arregla la captura en sentido contrario.

Para **IN asíncrono**, la Teensy debe variar el número de muestras **reales y consecutivas** de cada paquete según su reloj o el nivel de su búfer. A 44.117,647 Hz y 1 ms, el promedio debe ser 44,117647 muestras/paquete; a 48 kHz habrá normalmente 48 y alguna corrección por ppm. No basta alternar 44/45 con un patrón fijo de 44.100 ni duplicar/descartar una muestra. El descriptor debe anunciar IN asíncrono y reservar capacidad para el paquete mayor.

Linux `snd-usb-audio` entrega a la captura ALSA las muestras recibidas y avanza según las longitudes efectivas de los paquetes; la tasa nominal del descriptor no obliga a inventar muestras para que cada segundo físico tenga exactamente 44.100. PipeWire reconcilia el ritmo de ese dispositivo con el del driver Audio+ mediante remuestreo adaptativo y búferes. Puede absorber deriva continua; **no reconstruye muestras descartadas dentro del firmware**. La configuración ya observada en el Pi está en [E3](mediciones/e3-relojes-20261109.md). Este comportamiento general es teoría; no se inspeccionaron el kernel ni PipeWire del Pi en esta sesión offline.

Referencias de teoría (sin acceso a red en esta evaluación): USB 2.0 §5.12.4, especialmente §5.12.4.2 «Feedback» (también citado en el core); USB Audio 1.0, sincronización y descriptores AS; Linux `sound/usb/pcm.c` (`retire_capture_urb`); documentación PipeWire «Clock domains». Los candidatos implementan **UAC2**, no UAC1: el principio de relojes se conserva, los descriptores/control de reloj cambian.

## 2. Stock: fallo del mecanismo y contradicción de frecuencia

Fuentes locales: **S** = `~/.platformio/packages/framework-arduinoteensy/cores/teensy4`; **A** / **G** = `../usbaudio-libs/{alex6679,grrrr}_teensy-4-usbAudio/changedCorefiles`, relativos a la raíz del clon. Estas rutas son referencias de auditoría, no dependencias del build. Las líneas se refieren a los archivos locales leídos.

- `S/usb_audio.cpp:390–401,433`: `usb_audio_transmit_callback` manda 44 muestras nueve veces y 45 una vez, promedio fijo **44.100/s**, con `TODO: dynamic adjust to match USB rate`.
- `S/usb_audio.cpp:361–380`: `AudioOutputUSB::update` conserva dos bloques por canal. Cuando ambos están ocupados, libera el primero, avanza el segundo y **reinicia offset a cero**. Pierde la parte aún pendiente del bloque liberado, cuya capacidad es 128 muestras; no necesariamente 128 muestras útiles en cada evento.
- Con producción a 44.117,647 y consumo a 44.100, sobran **17,647 muestras/s**. Un bloque equivale a ~2,9 ms. E3 observó ráfagas de ~190 detecciones cada ~11 s; esas detecciones del Hilbert **no son una cuenta directa de muestras descartadas**. Es compatible con discontinuidades, no prueba por sí solo su tamaño ni el reloj real.
- OUT sí tiene feedback por ocupación: `S/usb_audio.cpp:235–261`; contadores OUT en `:66–67,194–202,258–261`. El underrun también aumenta sin flujo activo; se deben comparar deltas durante reproducción, excluyendo arranque/parada. No son contadores de las pérdidas IN.
- Stock anuncia UAC1 (`S/usb_desc.c:1445`), IN adaptive (`:1554`) y OUT async + feedback (`:1613,1631`); no es suficiente cambiar solo ese atributo para corregir IN.

**Contradicción que impide afirmar el diagnóstico numérico para este binario:** aquí el paquete instalado es **1.160.0 = Teensyduino 1.60**, plataforma Teensy **5.1.0** (`package.json`, `platform.json`; el builder deriva `TEENSYDUINO=160` de la versión). `S/AudioStream.h:57–61` define **44.100,0**, y `libraries/Audio/output_i2s.cpp:407–416` programa el PLL a partir de esa constante. No se encontró un override en `platformio.ini`. Los documentos E3/05 y comentarios del grafo dicen 44.117,647. Preservamos ambos hechos: hay un mecanismo stock incapaz de seguir deriva, pero **falta identificar el firmware/framework que produjo E3 y medir LRCLK** para atribuirle exactamente 17,647 muestras/s. Incluso dos relojes nominales a 44.100 difieren en ppm.

## 3. Candidatos locales

**Audio Library oficial:** objetos I2S, mixers, engines que la usan. **Custom/comunidad:** sustituciones de core USB, `AudioStream.h`, interfaces y control de búfer siguientes. No son una actualización oficial de PJRC.

| Punto | alex6679 | grrrr |
|---|---|---|
| Revisión local / último commit | `f731821`, 2026-05-20 | `e842379`, 2026-08-14 |
| Base declarada en README | Teensyduino 1.59; probado con PlatformIO Teensy 5.0.0 | Mismo README/base; no prueba de port a 1.60 |
| Clase USB | UAC2 | UAC2 |
| IN sigue el reloj/búfer | **Sí**, `ASYNC_TX_ENDPOINT` activo en `usb_desc.h:1014` | **Sí**, activo en `usb_desc.h:1028` |
| OUT feedback | PI configurable, estima ocupación y tiempo; estados RX/TX | Mismo controlador (archivo de interfaz idéntico) |
| Canales | 2/4/6/8 por sentido, configuración simétrica | Igual |
| Frecuencias | 44,1/48/96 kHz seleccionables al compilar | Igual |
| Formato USB | PCM 16/24 bits; AudioStream sigue siendo int16 | Añade PCM hasta 32 bits y float USB, sin convertir el motor interno en float |
| Licencia | MIT en LICENSE; archivos de core retienen aviso PJRC con condición adicional de selección de dispositivos | Igual |
| Reemplazo | 9 archivos de core, no una librería `lib_deps` | Los mismos 9; cambios adicionales en formatos/descriptores |

**El README está desactualizado:** describe omisión/duplicación de muestras, pero ambos árboles activan la rama asíncrona. En **ambos**, `usb_audio_interface.cpp:572–584` modifica `target` ±1; `:804–864` calcula ocupación suavizada usando `ARM_DWT_CYCCNT` y aplica esa corrección tras 10 desviaciones. Devuelve `target * canales * subslot` en `:903`. La alternativa que mueve el offset y pierde/repite muestras (`:586–600,891–895`) solo compila **sin** `ASYNC_TX_ENDPOINT`. Los descriptores también seleccionan `0x05` con esa marca (`A/usb_desc.c:1397–1402`). El controlador OUT calcula feedback PI en `:462–479`; `getStatus()` expone errores, memoria RX y correcciones IN. Los contadores TX se reinician cuando deja de transmitir (`:754–758`): guardar deltas por sesión y registrar reconexiones.

Los nueve archivos son `AudioStream.h`, `usb.c`, `usb_desc.c/.h`, `usb_audio.cpp/.h`, `usb_audio_interface.cpp/.h` y `util/LastCall.h`. GUI y menús Arduino no son necesarios con PlatformIO. Reemplazar `usb.c`/descriptores 1.60 por versiones basadas en 1.59 puede regresar CDC, MIDI, enumeración o caché DMA; compilar no demuestra esos comportamientos.

`AudioStream.h:39–52` permite fijar `AUDIO_SAMPLE_RATE_EXACT` y `AUDIO_SAMPLE_RATE_I`: 44.100 o 48.000 **nominales** coherentes entre DSP, USB e I2S; no obliga a 44.117,647. El PLL oficial calcula una aproximación racional y conserva tolerancia del cristal: cambiar constantes no garantiza 48.000,000 Hz físicos ni sincronía con el Pi. A 48 kHz hay que validar SGTL5000, osciladores, filtros, delays y cualquier tabla/constante a 44,1. No se cambia la tasa en esta evaluación.

**Recomendación:** evaluar **alex6679 fijo a f731821**, async IN activo, **4 canales en cada sentido**, PCM16, 44.100 nominal, bloques de 128. Cumple ≥2 hacia el Pi y ≥4 desde el Pi (sala + cue para la futura opción C); hoy el grafo sigue usando dos canales, los otros dos quedan reservados. grrrr no aporta una mejora de sincronía sobre alex y amplía la superficie de formatos. Mantener Audio+ como driver y el remuestreo de PipeWire. Alternativa con menor sustitución de core: portar solo IN asíncrono al stock 1.60 y ampliar sus descriptores a cuatro canales; conserva el USB reciente, pero exige desarrollar y validar otro controlador y no se implementa aquí. Esto no declara aprobado el audio para tocar: falta la hora de hardware y enumeración de MIDI/CDC.

## 4. Integración: PlatformIO obligatorio

| Método | Ventaja | Riesgo/costo |
|---|---|---|
| Framework completo vendorizado, `platform_packages = framework-arduinoteensy@symlink://ruta` (o `@file://ruta`) solo en `consola_usb` | Paquete explícito, reproducible si se fija su contenido; sin parche global | ~93 MB locales; arrastra bibliotecas ajenas, múltiples licencias. `file://` instala una copia que puede quedar obsoleta; `symlink://` requiere conservar la ruta |
| Overlay de los nueve archivos sobre copia privada del framework instalado | Solo ~340 KB de terceros versionados, conserva bibliotecas oficiales y no escribe en el paquete compartido | Hay que fijar/verificar la versión base y redirigir todos los includes y fuentes a la copia, no solo compilar otro `usb_audio.cpp` |
| Sobrescribir `~/.platformio/packages/...` o poner archivos en `src/` | Rápido una vez | **Descartado:** contamina otros entornos o duplica símbolos; los includes entre archivos del core pueden seguir tomando headers stock |

**Elegido: overlay versionado y copia privada generada** bajo `.pio/`, aplicado por un script `pre:` exclusivo de `[env:consola_usb]`, que extiende `consola`. El script comprueba paquete 1.160.0, copia el framework, superpone esos nueve archivos y redirige el directorio del paquete para ese proceso de build. Es una adaptación custom del builder de PlatformIO, no una API USB oficial; verificarla al actualizar PlatformIO. No requiere descargar ni editar el paquete global. No se copia ningún archivo desde los clones de referencia durante builds posteriores. Mantener LICENSE, avisos PJRC y revisión/origen en el overlay. Se agrega un retorno defensivo custom en `AudioOutputUSB::update`: si la interfaz está detenida (`bIdx == -1`), vaciar las colas y salir antes de indexar el búfer. Ambos candidatos tienen esa ruta sin retorno. El resto del overlay conserva los archivos de alex. Las fechas de mantenimiento son las últimas revisiones disponibles localmente; no se verificó actividad posterior.

`sketch49`, `consola` y `native` conservan flags, fuentes y framework stock. El nuevo diagnóstico `@GF USB` sigue el parser `preset_link.h` y el switch del sketch 49: en el experimento informa RX/TX, memoria RX, skips/pads y correcciones de tamaño; en consola stock informa sus dos contadores OUT; fuera de consola devuelve `@GF ERR CONSOLA`. Sin argumentos ni reset: el host registra diferencias. Test native: aceptar USB y rechazar argumentos. No prueba la ISR.

Verificación sin flash desde `apps/firmware-teensy`:

```sh
~/.platformio/penv/bin/pio run -e consola_usb
~/.platformio/penv/bin/pio run -e consola
~/.platformio/penv/bin/pio run -e sketch49
~/.platformio/penv/bin/pio test -e native
```

## 5. Medición que corre Juan

1. **Antes:** guardar hash del firmware, versión/flags y tasa; medir LRCLK del SGTL5000. Guardar descriptores (`lsusb -v`), canales ALSA, conexiones PipeWire y `pw-top`: Audio+ driver a 48 kHz, Teensy seguidor a 44,1. Verificar audio + MIDI + CDC y reconexión. No asumir `card 3`: identificar serie/nodo. Comprobar 4 canales por sentido también a través del aislador/hub usado.
2. **IN solo, 60 min:** `@GF RUTA LINEA`, `@GF CUE 0`. Audio+ reproduce seno continuo 1 kHz a −20 dBFS; cable analógico Audio+ → LINE_IN del SGTL5000. Pi **graba la fuente Teensy por PipeWire**, sin enviar audio USB a Teensy ni conectar su captura de vuelta a sala. Usar detector de fase estilo `tools/consola/relojes.py`: ventanas de 1 s con 400 muestras de solape, bordes excluidos, umbral 0,05 rad y mediana del avance para tolerar deriva lenta. Ajustar SR al archivo grabado (44,1 o 48 kHz); no ejecutar sin cambios el loopback ALSA de ese script. Vigilar ambos canales, nivel, silencios, xruns y WAV crudo. Validar detector con un salto de una muestra inyectado offline; elegir antes una exclusión fija de arranque de 5 s y registrar esa exclusión. **Pass: 0 segundos con saltos después del arranque**, ningún silencio inesperado ni xrun; no esconder ráfagas ajustando el umbral.
3. **OUT solo, 60 min:** detener captura Teensy; Pi envía seno continuo a cue por PipeWire, `@GF CUE 1`. Leer `@GF USB` cada segundo por CDC, tomar baseline tras los 5 s iniciales; exigir **ΔRX underrun = ΔRX overrun = ΔRX memoria = 0**, sin reconexiones. Escuchar/grabar analógicamente audífonos (atenuar) para detectar clics: los contadores no prueban calidad por sí solos. En stock solo existen los dos contadores OUT; aumentos sin stream no son fallos de esta prueba.
4. **Duplex/carga:** repetir IN 60 min con OUT activo y luego seis voces/engines + FX previstos, MIDI y controles. Skips/pads TX = 0; las correcciones de tamaño ±1 son normales. RX/TX errores = 0 después de arranque. Comparar con stock bajo idénticas condiciones y guardar registros/temperatura/carga del Pi.
5. **CPU/latencia:** Brain engines **≤~30 %**, total normal **≤60 %**. Registrar `AudioProcessorUsage/Max`, `AudioMemoryUsage/Max` y carga de ISR USB (el contador Audio Library no incluye necesariamente todo el USB); medir margen real con temporización/ciclo o GPIO. Registrar `pw-top`, quantum, tasa y xruns bajo carga. Medir clic Audio+ → LINE_IN → USB → Pi y MIDI→salida física: p50/p95/máximo. Bloque 128/44.100 = **2,90 ms**, targets de búfer custom TX **3,5 ms**, RX **1,8 ms**, más USB, periodos y remuestreo del Pi; no sumarlos como una latencia medida. Contrastar MIDI→audio <5 ms p95 y audio <1 ms del SSoT: A podría no cumplirlos; decisión pendiente, no se relajan aquí.

## 6. Contradicciones pendientes y límites

- E3/05 y comentarios `audio_graph.{cpp,h}` atribuyen 44.117,647 al firmware; el framework 1.60 local configura 44.100. Falta identificar el binario medido. Un FIFO finito no absorbe deriva constante indefinidamente.
- `03-arquitectura-capas.md` §1 dice que el sonido sigue si el Pi falla y que nada de tiempo real depende de Linux. Con A, la **sala del Brain depende de USB/PipeWire/Pi**. Su §2 dice dos canales por sentido; este experimento anuncia cuatro. No se corrige ese contrato aquí.
- `03` §2 declara pendiente autorizar CDC, pero `02-bridge-protocol.md` §1.3 ya incluye la enmienda CDC para Pi y dos Teensy; el bloque anterior aún dice TinyUSB/Full Speed. Estos cores usan PJRC High Speed (con fallback Full Speed), no TinyUSB.
- SSoT `01-architecture.md`/CLAUDE: salida Brain por SGTL5000 y latencia <1 ms; A sale por Audio+ y añade búferes. Resolver con Juan tras medir. La opción C futura de sala+cue de cuatro canales no se confunde con la opción C «un reloj» del informe E3.
- `05` ya señala `lineInLevel(5)`: comentario del grafo y escala del codec discrepan. Mantener nivel de prueba conservador y comprobar saturación.
- En este clon no están `06-teensy-s-e4.md` ni `08-superficie-pi.md` mencionados como estilo; se sigue el formato breve de `05`/`04`.

Fuentes auditadas: [medición E3](mediciones/e3-relojes-20261109.md), [E3 firmware](05-e3-teensy-a.md), [capas §1/4](03-arquitectura-capas.md), [bridge §1.3](../02-bridge-protocol.md), `platformio.ini`, grafo y parser del firmware; README, LICENSE y código de ambos clones locales, commits indicados arriba. No se consultó la red. Compilación y tests de escritorio no sustituyen ninguna medición de esta sección.

## 7. Resultado de escritorio (2026-11-09)

| Verificación | Resultado |
|---|---|
| `pio run -e consola_usb` | SUCCESS, 49,13 s; flash código 274.040 B; RAM1 variables 133.920 B, libres locales 95.456 B; RAM2 variables 187.776 B |
| `pio run -e consola` | SUCCESS, 73,25 s; flash código 266.164 B; RAM1 variables 129.568 B, libres locales 132.576 B; RAM2 variables 187.744 B |
| `pio run -e sketch49` | SUCCESS, 73,23 s |
| `pio test -e native` | **157/157**, 34,34 s; caso consola ampliado con USB válido y USB con argumento rechazado |
| `git diff --check` | Sin errores en cambios propios tracked |

Se usó `~/.platformio/penv/bin/pio` desde `apps/firmware-teensy`, con `PLATFORMIO_CORE_DIR=/tmp/gf-usb-pio-core` y `PLATFORMIO_SETTING_ENABLE_TELEMETRY=no`: el sandbox impide escribir estado/caché en `~/.platformio`. Los paquetes/plataformas instalados se referencian en lectura desde ese core temporal. Unity 2.6.1 se extrajo del archivo ya presente en la caché local a `.pio/libdeps/` (sin descarga). Un primer intento pidió instalar Unity y se detuvo; después se resolvió con esa caché. No se cambió la configuración versionada de los otros entornos. Logs completos de esta sesión: `/tmp/gf-consola-usb-build.log`, `/tmp/gf-consola-build.log`, `/tmp/gf-sketch49-build.log`, `/tmp/gf-native-test.log`.

Hay warnings de código existente (por ejemplo `strncpy` y `-fno-rtti` aplicado a C); los builds no son una certificación sin warnings. El experimento añade ~7,9 KB de código y ~4,4 KB de variables RAM1 frente a consola; la caída del espacio local incluye alineación/padding ITCM. **No hay medición de CPU, latencia ni continuidad todavía.**

Se escribió el documento antes del código y se intentó su commit primero. Git rechazó crear `.git/index.lock` con `Operation not permitted`; quedan documento e implementación sin commits. No se incluyó ni modificó el informe E3 preexistente sin seguimiento. No se flasheó ni se hizo push.
