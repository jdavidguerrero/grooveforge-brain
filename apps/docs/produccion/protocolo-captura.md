# Protocolo de captura de la paleta (Sesión B)

> **Fecha:** 2026-10-27 · **Para qué:** que los sonidos grabados en sesiones distintas se puedan comparar y reutilizar,
> y que cada audio quede unido a su preset, su MIDI y una decisión tuya. Depende de `ia-local-refinamiento.md` §3 y §6,
> `variants/studio/paleta-v1.md` y el bibliotecario (`tools/preset-librarian`).

## 1. Montaje fijo (se anota una vez y no se cambia entre sesiones)

| Qué | Valor |
|---|---|
| Brain | VOL de salida en **0 dB**; SYNTH FX como los trae el preset (eso es "seco del Brain") |
| Cable | LINE OUT L / R del Brain → entradas 1 / 2 de la Quantum ES 2 (estéreo: el chorus del Juno y las capas lo usan) |
| Ganancia de la interfaz | Fijarla con el preset más fuerte de la sesión para que los picos queden en **−12 dBFS** (nunca sobre −6); **anotar la posición** y no tocarla en toda la sesión |
| Live | 48 kHz / 24 bit; pista de audio estéreo con Monitor en **Off** al grabar (se escucha por la interfaz) |
| Reloj | Live manda clock al Brain (Sync en la salida GrooveForge) para que PUMP y ARP caigan en fase |
| Tempo | **124 BPM** salvo que el estudio pida otro |

## 2. MIDI de prueba por rol (clips neutros, siempre los mismos)

Son patrones de **medición**, como un tono de prueba: no son música ni material de identidad (rev. 2026-10-27: ya no
salen de "Run", que es un remix sobre la melodía de otra canción). Los genera `tools/paleta-lab/make_test_clips.py`
en `GrooveForge_Dataset_v1/midi/<rol>/`, en La menor a 124 BPM (`--key` los transpone).

| Rol | Clip | Por qué ese |
|---|---|---|
| `bass` | `bass_ghost_notes`: 2 compases, notas fuertes 78–84, ghost notes 44–52 y un salto de octava | Mide bounce y respuesta a la velocity |
| `chords` | `chords_sostenido_y_stabs`: i–VI–III–VII sostenidos (1 compás cada uno) y después stabs sincopados | Sostenido y articulado en el mismo sonido |
| `pluck` | `pluck_motivo_y_nota_larga`: motivo de 4 notas en semicorcheas, una octava arriba y una nota larga | Ataque en notas cortas y cola en la larga |
| `pad` | `pad_nota_y_acorde`: La3 sostenido 4 compases y el acorde i 4 compases | Movimiento sin tocar nada |
| `perc` | `perc_dinamica`: un golpe por compás en dos alturas a velocity 40, 80 y 120 | Ataque y rango dinámico |
| registro | `registro_la1_la5`: una nota por negra de La1 a La5 | Dónde suena bien cada sonido (`key_range` de la ficha) |
| Capa híbrida | El clip del rol de la capa, tres veces: MORPH 0 %, 50 % y 100 % | Qué aporta cada mitad |

Los clips viven en un set plantilla (`GF_Paleta_Lab.als`) para no rehacerlos cada vez.

## 3. Qué se graba por sonido

1. **Seco:** solo el clip, el Brain sin procesamiento de Live (8 compases).
2. **En contexto:** el mismo clip sonando con el loop de prueba (kick de Kick 3, percusión de Tribe Instruments, hat).
   Se graba solo la pista del Brain, sin mezclarla con el loop.
3. **Procesado** (opcional): si en Live le pones algo que es parte del sonido (por ejemplo un delay), una toma más con eso.
4. **Ruido de fondo:** 5 segundos de silencio al empezar la sesión, con todo conectado (la protoboard mete clicks:
   así queda documentado qué ruido es del montaje y no del sonido).

Para comparar sonidos se escuchan **a niveles semejantes** (igualando por oído o con un medidor de LUFS en Live), pero
los archivos se guardan **sin normalizar** para no perder la dinámica.

## 4. Nombres y carpetas

```
GrooveForge_Dataset_v1/
  patches/brain/<engine>/<nombre>.json      ← gf_presets.py export (preset + ficha)
  midi/<rol>/<clip>.mid                     ← los clips de la §2
  renders/<AAAAMMDD>/<rol>_<nombre-preset>_<seco|contexto|proc>_t<toma>.wav
  renders/<AAAAMMDD>/ruido_montaje.wav
  metadata/sesiones/<AAAAMMDD>.json         ← montaje, ganancia y decisiones de la sesión (§5)
```

Ejemplo: `renders/20261028/bass_gf-pocket-bass_seco_t1.wav`.

## 5. Ficha de sesión y decisiones

Al empezar, `tools/paleta-lab/gf_session.py new <rol> <candidatos…>` crea la ficha; durante la escucha, `decide`, `reject` y
`permiso` la completan; al terminar, `gf_presets.py export` guarda los presets tal como sonaron. La ficha queda así:

```json
{
  "fecha": "2026-10-28",
  "montaje": {"ganancia_interfaz": "1 y 2 a las 11 h", "bpm": 124, "vol_brain_db": 0, "firmware": "<commit>"},
  "rol": "bass",
  "candidatos": ["JD RUN FUNKBASS", "GF POCKET BASS", "HY WOODBASS B"],
  "decisiones": [
    {"a": "JD RUN FUNKBASS", "b": "GF POCKET BASS", "elegido": "a",
     "por_que": "las ghost notes se sienten y deja espacio al kick", "contexto": "loop afro 124"}
  ],
  "rechazados": [{"sonido": "HY WOODBASS B", "por_que": "el golpe de madera tapa la tambora"}],
  "origen_permisos": {"JD RUN FUNKBASS": "propio · entrenar sí · publicar sí"}
}
```

- **Decisiones pareadas** (A contra B, cuál y por qué) valen más que una puntuación. Los rechazados también se
  guardan, para no volver a probar lo mismo.
- **Origen y permisos** por sonido: propio, Tribe Instruments, Kick 3, Splice o Loopcloud, y si se puede usar para
  entrenar y para publicar. Un sample procesado sigue teniendo el permiso de su origen.
- Máximo **tres candidatos por decisión**, para no bloquearse con demasiadas opciones.
