# Sprint 1.3 — Matching Jig + Transistor Pairs

> **Fase:** 1 — Audio Core
> **Estimado:** 1 sesión (~2-3h)
> **Status:** 🟡 Pending re-medición con protocolo térmico
> **Refs:** `apps/docs/06-implementation-roadmap.md` §2 Sprint 1.3
> **Demo target:** batch de 2N3904 medidos y etiquetados con Vbe, 8 pares con ΔVbe < 2mV

---

## Theory

### La juntura base-emisor (Vbe) — el transistor como diodo

Un transistor NPN (como el 2N3904) tiene tres terminales: Base (B), Colector (C)
y Emisor (E). Entre la base y el emisor hay una juntura PN — un diodo. Cuando esa
juntura está polarizada en directa, conduce:

```
         Colector (C)
              │
         ┌────┴────┐
Base (B)─┤  2N3904 │
         └────┬────┘
              │
         Emisor (E)
```

El voltaje base-emisor **(Vbe)** es el voltaje de conducción de esa juntura. Para
el 2N3904 a temperatura ambiente y corriente de ~100µA:

```
Vbe ≈ 0.58V – 0.65V
```

El valor exacto depende de la corriente y de las variaciones de fabricación entre
transistores del mismo lote. Dos 2N3904 del mismo rollo pueden tener Vbe diferentes
en ±30mV — suficiente para degradar seriamente el sonido del filtro ladder.

### Por qué el ΔVbe destruye el sonido del ladder

El ladder filter de Moog usa **4 etapas idénticas** en cascada, cada una con un par
de transistores (Q1/Q2). El sonido del filtro depende de que estas etapas sean
matemáticamente simétricas:

```
Etapa ladder:
       Vcc
        │
       [R]
        │
   Q1──┤ ├──Q2      ← par diferencial: Q1 y Q2 deben tener Vbe idéntico
   E ──┴─┴── E
        │
       [Ie]          ← fuente de corriente constante
```

Si Q1 y Q2 tienen Vbe distintos, la diferencia aparece como un **offset de DC** en
la transferencia de señal de esa etapa. El offset se acumula a través de las 4 etapas
y resulta en:

- Distorsión asimétrica (armónicos pares — suena "sucio", no el tipo deseable)
- Desplazamiento del punto de operación → rango dinámico reducido
- Variación del cutoff real vs el programado
- Resonancia irregular — no sube de forma predecible

**Target del proyecto:** ΔVbe < 2mV entre los dos transistores de cada par.
Con ΔVbe < 2mV la distorsión de offset es imperceptible y el ladder se comporta
como el diseño teórico.

### El par diferencial — por qué se necesita matching

El par diferencial (Q1/Q2 con emisores compartidos) es el bloque fundamental del
ladder. Su función es amplificar la *diferencia* entre dos señales mientras cancela
el ruido y la distorsión comunes a ambas:

```
señal (+) ──→ Base Q1 ─┐
                        ├──→ [Ie corriente constante] ──→ output
señal (-) ──→ Base Q2 ─┘

Cancela: ruido de Vcc, ruido térmico, variaciones de temperatura
Amplifica: diferencia entre señal (+) y señal (-)
```

La cancelación solo funciona si Q1 y Q2 son **idénticos**. Si Vbe(Q1) ≠ Vbe(Q2),
la cancelación es imperfecta y el offset residual contamina la señal. Por eso
el matching manual es esencial en un ladder discreto.

### Corriente constante para medir — por qué no basta con voltaje

Vbe no es fijo — depende de la corriente que pasa por la juntura (ecuación de Shockley):

```
Vbe = (kT/q) × ln(Ic / Is)

donde:
  k = constante de Boltzmann
  T = temperatura absoluta (Kelvin)
  q = carga del electrón
  Is = corriente de saturación (específica de cada transistor)
```

Si medís dos transistores con distintas corrientes, obtenés Vbe distintos aunque
los transistores sean idénticos. Para comparar transistores correctamente hay que
forzar **la misma corriente** en todos y medir el Vbe resultante.

**Corriente target: 100µA** — valor estándar en la industria para matching de
transistores de señal pequeña. Es representativa del rango de operación del ladder.

### Cómo forzar 100µA con un resistor — la fuente simple

Con la Teensy 4.1 el circuito más simple es conectar el transistor en configuración
**diodo** (Base conectada a Colector) y poner un resistor en serie con Vcc:

```
Teensy 3.3V ──[R]──┬── Colector = Base (diodo-connected)
                   │
                   └── Teensy A0 (ADC lee Vbe aquí)
                     2N3904
                   ── Emisor ──→ GND
```

Con Base = Colector, el transistor se comporta como un diodo con Vbe ≈ 0.62V.
La corriente que pasa:

```
I = (Vcc - Vbe) / R = (3.3 - 0.62) / 27.000 ≈ 99µA ≈ 100µA
```

**R = 27kΩ** (valor más cercano a 27kΩ exacto en serie E24: 27kΩ).
Con 33kΩ: I ≈ 81µA. Con 22kΩ: I ≈ 122µA. Cualquiera sirve — lo que importa es
que sea la MISMA corriente para todos los transistores medidos.

### Precisión del ADC del Teensy 4.1

El Teensy 4.1 tiene un ADC de 12 bits con referencia a 3.3V:

```
Resolución = 3300 mV / 4096 = 0.806 mV / LSB
```

Con ΔVbe target de 2mV, necesitamos medir diferencias de ~1mV — cerca del límite
de 1 LSB. Solución: **promediado (oversampling)**.

Con `analogReadAveraging(32)`, el Teensy promedia 32 muestras consecutivas. Por el
teorema del ruido, la resolución efectiva mejora en √32 ≈ 5.7×:

```
Resolución efectiva ≈ 0.806 / 5.7 ≈ 0.14 mV
```

0.14mV de resolución efectiva es suficiente para detectar diferencias de 1-2mV
con confianza. El jig mide cada transistor con este promediado.

### Referencias

- Paul Horowitz & Winfield Hill, "The Art of Electronics" 3rd ed. — Cap. 2.3
  (transistor como diodo, Vbe, Shockley equation)
- Bob Moog, "A Voltage-Controlled Low-Pass High-Pass Filter" (AES 1965) —
  matching requirements para el ladder original
- PJRC, Teensy 4.1 ADC docs — `analogReadResolution()`, `analogReadAveraging()`
- `apps/docs/03-filter-design.md` §3 — specs de matching del proyecto

---

## Wiring (Cableado)

### Hardware del sprint

| Componente | Cantidad | Notas |
|---|---|---|
| Teensy 4.1 | 1 | Ya montada del Sprint 1.1 |
| 2N3904 NPN | batch (~20-100) | Los transistores a matchear |
| Resistor 27kΩ | 1 | Fuerza ~100µA. Sirve 22kΩ–33kΩ |
| Protoboard + jumpers | — | Ya montado |

> No se necesita socket ZIF ni LCD — el spec original usaba Arduino + LCD,
> adaptado para usar la Teensy + Serial monitor que ya están en uso.

### Tabla de conexiones

| Componente | Teensy pin | Señal |
|---|---|---|
| Resistor 27kΩ (extremo 1) | 3.3V | Alimentación del circuito |
| Resistor 27kΩ (extremo 2) | A0 (pin 14) | Punto de medición Vbe |
| 2N3904 Colector + Base | A0 (pin 14) | Nodo Vbe (diodo-connected) |
| 2N3904 Emisor | GND | Referencia |

### Diagrama

```
Teensy 4.1
  3.3V ──────[27kΩ]──────┬─── A0 (pin 24, ADC)
                          │
                        Base + Colector (cortocircuitados)
                          │
                        [2N3904]
                          │
                        Emisor
                          │
  GND ───────────────────┘
```

### Montaje paso a paso

1. Sin USB conectado.
2. Colocá el resistor 27kΩ entre el riel 3.3V del protoboard y un nodo intermedio.
3. Conectá ese nodo intermedio a Teensy A0 (pin 14) con un jumper.
4. Dejá tres filas libres cerca del nodo para insertar los transistores.
5. Cuando insertes cada 2N3904: Base y Colector al nodo (A0), Emisor a GND.
   El 2N3904 en DIP tiene el pinout: `EBC` de izquierda a derecha (flat side facing you).
6. Conectá USB — no hay audio en este sprint, solo ADC.

### Pinout 2N3904 DIP-3

```
Flat side hacia vos:
  [E] [B] [C]
  Emisor  Base  Colector
```

### Verificación pre-medición

- [ ] Resistor 27kΩ entre 3.3V y nodo A0 — verificar continuidad
- [ ] GND conectado al riel de tierra del protoboard
- [ ] Sin transistor insertado: A0 debe leer ~3.3V (el resistor jala a 3.3V sin carga)
- [ ] Con transistor insertado: A0 debe leer ~0.62V (Vbe típico)

---

## Implementation

### Archivos

| Archivo | Descripción |
|---|---|
| `apps/firmware-teensy/src/sketches/03-matching-jig.cpp` | Sketch del jig de medición |
| `apps/firmware-teensy/platformio.ini` | Actualizar `build_src_filter` |

### Código

```cpp
// apps/firmware-teensy/src/sketches/03-matching-jig.cpp
// Sprint 1.3 — Matching Jig: mide Vbe de 2N3904 a ~100µA via ADC.
// Theory y wiring: apps/docs/sprints/03-matching-jig.md

#include <Arduino.h>

// Pin A0 = Teensy pin 24
constexpr uint8_t VBE_PIN       = A0;
constexpr uint8_t ADC_BITS      = 12;
constexpr uint8_t ADC_AVERAGING = 32;
constexpr float   VREF          = 3.3f;
constexpr int     ADC_MAX       = (1 << ADC_BITS) - 1;  // 4095

// Historial de mediciones de la sesión
constexpr uint8_t MAX_READINGS = 100;
float readings[MAX_READINGS];
uint8_t reading_count = 0;

float measure_vbe() {
    // Promedio de 5 lecturas adicionales sobre el analogReadAveraging interno
    long sum = 0;
    for (int i = 0; i < 5; i++) {
        sum += analogRead(VBE_PIN);
        delayMicroseconds(100);
    }
    float raw = sum / 5.0f;
    return (raw / ADC_MAX) * VREF * 1000.0f;  // en mV
}

void print_session_stats() {
    if (reading_count < 2) return;

    // Ordenar para análisis de pares (bubble sort — N pequeño)
    float sorted[MAX_READINGS];
    memcpy(sorted, readings, reading_count * sizeof(float));
    for (int i = 0; i < reading_count - 1; i++)
        for (int j = 0; j < reading_count - i - 1; j++)
            if (sorted[j] > sorted[j+1]) {
                float tmp = sorted[j]; sorted[j] = sorted[j+1]; sorted[j+1] = tmp;
            }

    Serial.println("\n─────────────────────────────────");
    Serial.printf("Transistores medidos: %d\n", reading_count);
    Serial.printf("Vbe min: %.2f mV\n", sorted[0]);
    Serial.printf("Vbe max: %.2f mV\n", sorted[reading_count-1]);
    Serial.printf("Spread total: %.2f mV\n", sorted[reading_count-1] - sorted[0]);

    // Buscar mejores pares (ΔVbe < 2mV)
    Serial.println("\nMejores pares (ΔVbe < 2mV):");
    int pair_count = 0;
    for (int i = 0; i < reading_count - 1; i++) {
        float delta = sorted[i+1] - sorted[i];
        if (delta < 2.0f) {
            Serial.printf("  Par: %.2f / %.2f mV → ΔVbe = %.2f mV ✓\n",
                sorted[i], sorted[i+1], delta);
            pair_count++;
        }
    }
    if (pair_count == 0)
        Serial.println("  Ninguno con ΔVbe < 2mV — necesitás más transistores");
    Serial.printf("\nPares válidos encontrados: %d / 8 necesarios\n", pair_count);
    Serial.println("─────────────────────────────────");
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);

    analogReadResolution(ADC_BITS);
    analogReadAveraging(ADC_AVERAGING);

    Serial.println("═══════════════════════════════════");
    Serial.println("  Sprint 1.3 — Matching Jig 2N3904");
    Serial.println("  Objetivo: ΔVbe < 2mV por par");
    Serial.println("═══════════════════════════════════");
    Serial.println("Comandos:");
    Serial.println("  r  → medir transistor actual");
    Serial.println("  s  → resumen de sesión + mejores pares");
    Serial.println("  c  → limpiar sesión");
    Serial.println();
    Serial.println("Insertar 2N3904, presionar 'r' para medir.");
}

void loop() {
    if (!Serial.available()) return;

    char cmd = Serial.read();

    switch (cmd) {
        case 'r': {
            float vbe = measure_vbe();
            Serial.printf("#%02d  Vbe = %.2f mV", reading_count + 1, vbe);

            if (reading_count > 0) {
                // Mostrar delta respecto al último medido
                float delta = fabsf(vbe - readings[reading_count - 1]);
                Serial.printf("  (Δ vs anterior: %.2f mV%s)",
                    delta, delta < 2.0f ? " ✓" : "");
            }
            Serial.println();

            if (reading_count < MAX_READINGS)
                readings[reading_count++] = vbe;
            break;
        }
        case 's':
            print_session_stats();
            break;
        case 'c':
            reading_count = 0;
            Serial.println("Sesión limpiada.");
            break;
    }
}
```

### Decisiones de implementación

- **Teensy en vez de Arduino + LCD:** el spec original indicaba Arduino + LCD pero
  la Teensy ya está montada y el Serial monitor da más información que un LCD.
  Resultado equivalente, menos hardware extra.
- **5 lecturas adicionales sobre analogReadAveraging(32):** doble promediado para
  maximizar la resolución efectiva. Total: 32 × 5 = 160 muestras por medición.
  Resolución efectiva ≈ 0.1mV.
- **Análisis de pares en sesión:** el sketch acumula todas las mediciones y al
  comando `s` calcula automáticamente qué pares tienen ΔVbe < 2mV. No hace falta
  hacer cuentas manuales.
- **Delta vs anterior:** al medir, muestra inmediatamente la diferencia con el
  transistor anterior — feedback rápido para decidir si forman par.

---

## Demo

### Qué valida este demo

Que podemos medir Vbe con suficiente precisión para matchear transistores al nivel
que el ladder filter requiere. Sin este proceso, el filtro sonará desequilibrado.

### Cómo reproducirlo

```bash
cd apps/firmware-teensy
# Actualizar platformio.ini: build_src_filter = +<sketches/03-matching-jig.cpp>
~/.platformio/penv/bin/pio run -e sketch -t upload
~/.platformio/penv/bin/pio device monitor -b 115200
```

Protocolo de medición:
```
1. Insertar 2N3904 #1 en el jig → presionar 'r'
2. Anotar Vbe en el transistor (marcador fino en el cuerpo: "621" = 621mV)
3. Retirar, insertar 2N3904 #2 → presionar 'r'
4. Repetir hasta procesar todo el batch
5. Presionar 's' → el sketch muestra los mejores pares automáticamente
6. Etiquetar los pares seleccionados
```

### Evidencia a capturar

- [ ] Screenshot del serial monitor con al menos 10 mediciones y el resumen `s`
- [ ] Foto de los transistores etiquetados con sus valores Vbe
- [ ] Lista de los 8 pares seleccionados con sus ΔVbe

### Criterios de pass

- [ ] Resolución de medición ≤ 0.5mV (verificar midiendo el mismo transistor 3 veces)
- [ ] Al menos 8 pares con ΔVbe < 2mV identificados
- [ ] Todos los transistores seleccionados etiquetados con su Vbe

---

## Tests

```bash
cd apps/firmware-teensy
~/.platformio/penv/bin/pio run -e sketch 2>&1 | grep -E "warning:|error:" || echo "OK"
```

Tests unitarios: N/A — este sprint es 100% hardware/medición. La validación es
el resultado físico (pares etiquetados).

---

## Learnings

### Qué salió diferente al plan

- **Efecto térmico no contemplado en el protocolo inicial:** el Vbe del 2N3904 cambia
  ~-2mV/°C. Al medir transistores secuencialmente con distintos niveles de manejo
  (algunos recién tocados, otros reposando), los valores absolutos variaban hasta 4-6mV
  entre mediciones del mismo transistor. Dos transistores podían aparecer como "par"
  por coincidencia térmica, no por matching real.

- **El jig funciona correctamente** — el circuito, el ADC con averaging y el código
  son válidos. El problema era el protocolo de medición, no el hardware.

- **Protocolo correcto descubierto durante el sprint:**
  1. Dejar todos los transistores reposar 5 minutos sin tocarlos (temperatura ambiente)
  2. Manipular solo por las patas (pinzas o uñas), nunca por el cuerpo
  3. Insertar → esperar 5 segundos → medir
  4. Retirar rápido y pasar al siguiente

- **Sprint 1.4 diferido:** componentes TL072 y CD4066 aún no disponibles.
  Sprint 1.4 marcado como technical debt — se retoma cuando lleguen los componentes.
  Se avanza a Sprint 1.5 (Moog Model D engine, digital) sin el filter analógico.

### Qué tomaría diferente

- Incluir el protocolo térmico en el sprint doc ANTES de la primera sesión de medición.
- Agregar al checklist de wiring: "manipular transistores solo por las patas".
- Medir el mismo transistor 3 veces antes de empezar el batch para verificar
  repetibilidad — si varía > 0.5mV, hay problema térmico o de contacto.

### Dependencias para el siguiente sprint

- Re-medición del batch con protocolo térmico correcto → pendiente antes de Sprint 1.4
- Sprint 1.4 diferido hasta conseguir TL072CP × 2 y CD4066 × 1
- Sprint 1.5 no depende del filter — arranca directamente con Teensy + SGTL5000

### Tiempo real vs estimado

- Estimado: 1 sesión (~2-3h)
- Real: 1 sesión — jig funcional, matching pendiente de re-medición con protocolo correcto

---

*Siguiente sprint: [04-filter-ladder.md](04-filter-ladder.md) — Filter Discreto Ladder Protoboard*
