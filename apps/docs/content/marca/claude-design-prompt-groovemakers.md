# Prompt para Claude Design — marca GrooveMakers

> 2026-10-22. Pegar en un chat nuevo de Claude Design. Los datos salen de `apps/docs/00-master-strategy.md` §8,
> `apps/studio-head/src/ui/theme.h`, ADR-0003 de GroovePush y `content/adr/ADR-002-comunidad-primero-un-device.md`.
>
> **Contradicción a decidir:** `00` §8.2 dice tipografía *Satoshi + General Sans*; las interfaces del Brain y de
> GroovePush usan *IBM Plex Mono* (ADR-0003). El prompt pide una propuesta que combine las dos. Elige tú; si cambia
> la regla, se registra en un ADR.

---

## Prompt

Diseña la identidad visual de **GrooveMakers**, un canal de YouTube y una comunidad sobre **construir instrumentos
electrónicos para hacer música**. Lo dirige Juan Guerrero, productor de música electrónica e ingeniero, desde Bogotá,
Colombia.

### Qué es GrooveMakers

- La primera temporada se llama **"Construyendo mi synth"**: Juan construye su propio sintetizador (GrooveForge
  Brain) para encontrar su sonido, primero en sus producciones y después en su live set. Cada episodio alterna
  **Estudio** (diseño de sonido y producción) y **Taller** (caja, esquemáticos, PCB).
- Es el paraguas de un ecosistema:
  - **GrooveForge**: la línea de hardware (instrumentos abiertos y modulares). Primer producto: **GrooveForge Brain**,
    un synth con 6 motores inspirados en clásicos, sobre un microcontrolador Teensy, con pantalla.
  - **GroovePush**: un controlador hecho a mano, inspirado en el Ableton Push (temporada 2).
  - **GroovePilot**: un copiloto de mezcla con IA. La IA aparece como herramienta que propone, nunca como la que crea.
- No vende nada todavía: es comunidad y proceso abierto. El tono es **de creador a creador**: honesto, técnico,
  cálido, latinoamericano. Nada corporativo, nada de hype de IA.

### ADN visual (ya existe en los instrumentos; respetarlo)

- **Colores** (de la interfaz del Brain):
  - Fondo `#0A0A0A`, superficies `#141414` / `#1C1C1C`, líneas `#242424`, texto `#F2F2F2`.
  - **Teal** `#1D9E75` (principal), `#5DCAA5` (claro), `#0E5040` (oscuro): el color del sonido y del hardware.
  - **Púrpura** `#534AB7`, `#7A6FE0`, `#2B2758`: el color de la IA. **Usarlo poco**, como acento.
  - Acentos de estado: ámbar `#E8B84A`, naranja `#FF6A35`, rojo `#E84A4A`.
- **Tipografía:** las pantallas de los instrumentos usan **IBM Plex Mono** (400 / 600). La estrategia de marca
  mencionaba *Satoshi* y *General Sans*. Propón una pareja: una para el logo y los títulos, otra (probablemente Plex
  Mono) para datos, etiquetas y textos técnicos.
- **Materiales del instrumento:** aluminio anodizado negro, cuerpo de PETG negro, **mejillas de guadua** (bambú
  colombiano laminado). La sensación: **artesanía boutique + precisión técnica + identidad latinoamericana**.
- **Objetos del mundo del canal** que pueden inspirar la forma: el anillo de 12 LEDs alrededor de un encoder, la
  traza de un osciloscopio, una pista de PCB con su vía, el surco de un vinilo ("groove"), la perilla con su marca.

### Qué evitar

- Clichés de música: audífonos, notas musicales, ecualizadores de barras genéricos, ondas de sonido de stock.
- Clichés de IA: cerebros, robots, circuitos brillantes, rayos, degradados neón.
- Mascotas, ilustraciones infantiles, demasiados elementos. Tiene que funcionar a **32 px** y en un **círculo**.

### Entregables

1. **Tres direcciones de logo**, cada una con una frase que explique la idea. Para cada una: isotipo, logotipo
   horizontal, versión apilada, versión monocromática (blanco sobre negro y negro sobre blanco), y prueba a 32 px y a
   16 px.
2. De la dirección que recomiendes, el **sistema completo**:
   - **Avatar** 800 × 800 (se muestra recortado en círculo en YouTube e Instagram).
   - **Banner de YouTube** 2560 × 1440, con todo lo importante dentro del área segura de 1546 × 423 al centro.
     Texto: "GrooveMakers · Construyendo instrumentos para hacer música".
   - **Plantilla de miniatura** 1280 × 720: zona para una foto real del banco, título de máximo 3 palabras en grande,
     etiqueta de episodio (por ejemplo "T1 · EP02") y etiqueta de carril (**Estudio** o **Taller**, con un color o
     ícono cada uno). El logo pequeño, en una esquina.
   - **Tercio inferior** (lower third) 1920 × 1080 para nombres de archivo o datos en pantalla (por ejemplo
     "fidelidad-engines.md §8"), en Plex Mono.
   - **Pantalla final** de 20 s, 1920 × 1080, con espacio para dos videos sugeridos y el botón de suscribirse.
   - **Sello para Reels / Shorts** 1080 × 1920: la cuenta de Instagram es la personal de Juan, así que la marca va
     como una firma pequeña ("GrooveMakers · T1 EP02"), no como protagonista.
   - **Marco de escritorio**: el logo para poner en la mesa del set, visible al fondo de las tomas.
     - Versión **impresa** 13 × 18 cm (vertical) para un portarretrato.
     - Versión **digital** 1280 × 720 para una pantalla de 7" (fondo `#0A0A0A`, el logo al centro, opcionalmente una
       animación muy lenta, por ejemplo la traza de un osciloscopio o el anillo de LEDs girando). Tiene que verse bien
       **filmada**: sin blancos puros grandes que se quemen en cámara.
3. **Arquitectura de marca:** cómo conviven GrooveMakers (canal y comunidad), GrooveForge (hardware), GroovePush y
   GroovePilot. Propón el sistema de respaldo (por ejemplo "GrooveForge Brain · un proyecto de GrooveMakers") y cómo
   se ve cada submarca junto al logo principal.
4. Una **hoja de estilo** de una página: colores con hex, tipografías con pesos, tamaños mínimos, margen de
   protección del logo, y 3 usos incorrectos.

Muestra todo sobre fondo oscuro, que es donde vive la marca, y al menos una prueba del logo **en una foto de un
banco de trabajo** (protoboard, cables, osciloscopio) para ver cómo se ve en el contexto real del canal.
