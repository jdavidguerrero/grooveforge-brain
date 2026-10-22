# Plan de grabación mientras llegan las piezas

> 2026-10-22. Mientras esperas las PCBs y las piezas del Push, se puede grabar **todo el Acto 1** (EP1-EP9).
> Biblia: [`BIBLIA_T1_LIVESET.md`](BIBLIA_T1_LIVESET.md). Cámara: [`GUIA_CAMARA.md`](GUIA_CAMARA.md) (cada guion trae su plan
> de sesión ordenado por posición).

## Principio: grabar en bloque, publicar espaciado

Grabar varios episodios en pocas sesiones y publicar uno cada 2 semanas (ADR-002 §7) te da **colchón**: si una
semana no puedes, el canal sigue. Con lo de abajo tienes material para ~4 meses.

## Sesiones

| Sesión | Qué se graba | Episodios | Duración | Necesita |
|---|---|---|---|---|
| **S1 · Banco + Ableton** | Piloto completo + toda la **sesión global de audio** (abajo) | EP1 | 2 h 15 | protoboard, Ableton, interfaz |
| **S2 · El Juno** | Bajo y acordes del primer track | EP2 | 2 h | S1 hecha (loop y tonalidad) |
| **S3 · Pantalla: la caja** | Claude Design, FreeCAD, piezas en mano | EP3 | 1 h 30 | solo el computador |
| **S4 · Los 6 motores** | Prueba de los motores, uno por uno, en bruto | EP4 | 2 h | S2 hecha (el loop) |
| **S5 · Pantalla: esquemático y simulación** | KiCad + `sim/input_stage_sim.py` | EP5 | 1 h 30 | solo el computador |
| **S6 · Pantalla: PCB** | Scripts de colocación y ruteo, DRC, auditoría | EP7 | 1 h 30 | solo el computador |
| **S7 · Filtro + Ableton + expresión** | Barridos, automatización, encoders, lead | EP6, EP8, EP9 | 3 h (o dos sesiones) | el track en marcha |

Las sesiones de pantalla (S3, S5, S6) sirven para días sin energía para el banco: se graban sentado, en cualquier
momento.

## Sesión global de audio (en S1, una vez)

Con la misma cadena (LINE OUT → interfaz → Ableton, 48 kHz / 24 bit) graba en bruto, sin pensar en el guion:
- El mismo acorde por los 6 motores (aunque alguno falle: guarda la falla).
- Barridos del cutoff con resonancia alta hasta la auto-oscilación, en 2-3 motores.
- El cutoff bajando por debajo de 100 Hz con Ableton conectado.
- Arranque de la cabeza (sonido ambiente).

Este banco de audio alimenta los cold opens y los Shorts de toda la temporada.

## Los motores que no has probado

No es un problema: es el **EP4**. Reglas:
1. Antes del EP2, prueba solo el Juno (5 min). Si falla, el plan B del guion usa Model D + Prophet.
2. El resto se prueba **con la cámara encendida** en S4. Lo que falle se anota (síntoma + commit) y se queda en el
   episodio: es honesto y es contenido.
3. Lo que haya que arreglar en el firmware se arregla fuera de cámara y puede ser un Short de "bug" después.

## Orden de publicación sugerido

| Semana | Publicar | Shorts de esa quincena |
|---|---|---|
| 1 | EP1 | 3 del EP1 |
| 3 | EP2 | 3 del EP2 |
| 5 | EP3 | 3 del EP3 |
| 7 | EP4 | 3 del EP4 |
| 9 | EP5 | 3 del EP5 |
| 11 | EP6 | 3 del EP6 |
| 13 | EP7 | 3 del EP7 |
| 15 | EP8 | 3 del EP8 |
| 17 | EP9 (cierre del Acto 1 + el track completo) | 3 del EP9 |

Si las placas llegan antes, el **EP10 (llegaron las placas)** se graba en cuanto lleguen, aunque se publique después:
el unboxing y el primer encendido no se repiten.

## Cosas a grabar en cuanto pasen (B-roll que no vuelve)

- El paquete de JLC llegando y abriéndose (EP10).
- El primer encendido de la portadora con la fuente con límite de corriente.
- La primera vez que una pieza impresa o el aluminio se monta.
- Cualquier momento de "funcionó" o "no funcionó" en el banco: prende la cámara aunque no sepas para qué episodio
  es (playbook §1: "captura ahora, edita después").
