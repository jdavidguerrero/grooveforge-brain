# Fase 4 de carácter sonoro: banco Signature y macro MOVE

> **Fecha:** 2026-10-27 · **Estado:** implementado, a validar de oído (fase 5) · **Depende de:** `caracter-sonoro.md` §4
> fase 4, `fase2-engines.md`, `fase3-fx.md`, `variants/studio/theory/09-studio-groove.md` (macros).

## 1. MOVE: un control para los builds y los breaks

Un solo control de performance que mueve varias cosas a la vez, para subir la tensión en un build y soltarla en el
drop. Vive en la parada MACRO de GROOVE, en el lugar que estaba libre (**SHIFT + ENC R**, 0–100 %), se guarda en el
preset como las otras macros y se toca desde Live con **CC 19**.

| Destino | Con MOVE al 100 % | Por qué |
|---|---|---|
| Cutoff | × 2^1.5 (una octava y media arriba) | el filtro que se abre en el build |
| Resonancia | +10 % del rango | más presencia al abrir |
| DELAY: FEEDBACK y nivel | +0.35 y +0.30 | los ecos se acumulan |
| REVERB: MIX | +0.35 | el sonido se aleja antes del drop |
| CHORUS / PHASER: DEPTH | +0.30 | más movimiento |

- **Depende del preset**: solo mueve los FX que el preset tiene encendidos, así un pluck con delay y plate se abre
  distinto que un bajo seco. Es un desplazamiento sobre el patch (como GRIT y POCKET): no cambia lo guardado.
- En una capa híbrida mueve el filtro de las dos mitades (el de B sigue al de A) y los FX, que son de A.
- No cambia el formato de preset: el valor va en `groove[MACRO][7]`, que ya se guardaba (estaba en 0).

## 2. Banco Signature

Seis sonidos por estilo, prefijo **GF**, cada uno con su engine en su rol (`caracter-sonoro.md` §3b), sus FX, GROOVE y
macros guardados. Nombres propios, sin artistas ni temas. Se agregan al banco de fábrica: el firmware los escribe
solos al final de cada banco (theory/06), sin tocar lo guardado.

| Estilo | Preset | Engine | Rol · idea |
|---|---|---|---|
| **Orgánico / afro** | GF LOG DRUM | DX7 | log drum de afro house: portadora 1:1, modulador ×1.5 de decay corto, grave |
| | GF CHONTA | DX7 | marimba de chonta: madera brillante (modulador ×3.5 corto) con delay 1/8. |
| | GF KALIMBA TINE | DX7 | kalimba: modulador ×5 muy corto, plate corta |
| | GF AFRO SUB | Model D | sub redondo (triángulo + SHARK abajo), POCKET para el rebote |
| | GF BAMBOO PAD | Juno | pad de aire: PWM lento, CHORUS I, BBD HISS 20 %, reverb GUADUA |
| | GF TALKING PLUCK | Prophet | pluck con POLY-MOD del filtro, resonancia media |
| **Melódico / progressive** | GF SUNRISE PLUCK | Prophet | pluck con delay 1/8. ping-pong y PLATE |
| | GF NOCTURNE PAD | OB-6 | pad ancho, UNISON ×2, PHASER lento y PLATE |
| | GF ANTHEM SAW | Prophet | saws en UNISON ×3 con DRIFT, para leads de progressive |
| | GF ROLLING BASS | Model D | bajo rodante en semicorcheas: decay corto, OVERDRIVE suave |
| | GF GLASS ARP | DX7 | campana con ARP 1/16 y delay 1/8. |
| | GF STRING SWELL | Juno | cuerdas de ataque lento, CHORUS II, RELEASE largo del 106 |
| **House / disco** | GF DISCO STRINGS | Juno | cuerdas saw con PHASER (Solina / Small Stone) |
| | GF FILTER STAB | Juno | stab de house francés: CHORD en tonalidad y PUMP |
| | GF OCTAVE BASS | Model D | bajo de octavas de disco: ARP UP 1/16 en 2 octavas |
| | GF CLAV FUNK | ARP 2600 | clavinet funky: pulso angosto, envolvente corta, PHASER |
| | GF BRASS HIT | OB-6 | brass stab con PUMP y TAPE |
| | GF HOUSE KEYS | DX7 | piano eléctrico de house con CHORUS y PUMP suave |
| **Indie tech** | GF ACID LINE | ARP 2600 | línea ácida: saw resonante, glide, DRIVE DIST |
| | GF DARK DRONE | OB-6 | dron con X-MOD del VCO 2 a audio y NOTCH |
| | GF GRIT BASS | Model D | SHARK + FM de audio del VCO 3 + OVERDRIVE |
| | GF METAL PERC | DX7 | percusión metálica FM (razón ×1.41) |
| | GF TAPE LEAD | Prophet | lead con sync, TAPE y delay 1/4 |
| | GF NOISE RISER | ARP 2600 | riser de ruido para builds: MOVE pensado para subirlo |

Son puntos de partida: la fase 5 los ajusta de oído dentro de un loop, con el criterio "encaja en el mismo mix".

## 3. Cómo se prueba

| Qué | Cómo |
|---|---|
| MOVE 0 no cambia nada; MOVE 100 sube cutoff, feedback, mixes y depth solo de los FX encendidos | test native |
| Los 24 presets nuevos: parámetros dentro de rango, nombres de ≤ 16 caracteres sin repetir | test native (el de fábrica existente) |
| Cada preset en su rol, en un loop de 124 BPM | en el hardware (fase 5) |
