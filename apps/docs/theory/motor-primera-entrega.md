# Motor de asistencia: primera entrega

> **Fecha:** 2026-11-05. **Estado:** en prueba. **Spec:** `apps/docs/produccion/motor-asistencia-v2.md` §7. **Código:** `tools/motor/`.
> Es Python propio, fuera de tiempo real; no toca firmware ni Teensy Audio Library. **No modifica** `00–06`.

## 1. Qué valida

La pregunta es si este flujo ahorra tiempo frente a hacerlo a mano:

```text
organic, La menor, 122 → pista guía → borrador de kick → 3 kicks → elegir uno → editar → queda registrado
```

**No cubre:** percusión, bajo, armonía, niveles por rol, cargar samples solo, ni ningún modelo de ML. La plantilla de arrangement es provisional hasta
tener ejemplos de Juan.

## 2. Por qué escribir en Live es delicado

Live solo deja modificar el set desde su hilo principal. El Remote Script recibe mensajes JSON por TCP, los pone en cola y los ejecuta cuando Live llama a
`update_display()`, más o menos cada 100 ms y sin garantía (Ableton, *Live Object Model*). Una escritura tarda lo que Live permita, y el motor debe esperar la respuesta,
no suponerla.

Tampoco hay una función para crear clips directamente en Arrangement. El script crea un clip temporal en Session y usa `duplicate_clip_to_arrangement`.
De aquí salen tres riesgos que el motor debe evitar:

| Comportamiento del puente | Riesgo | Qué hace el motor |
|---|---|---|
| Duplicar a Arrangement ocupa ese tramo de la pista | Podría pisar un clip de Juan | Solo escribe en una pista sin clips de Arrangement; si no, crea otra |
| Leer un tiempo sin clip devuelve **el clip más cercano** | Comparar contra el clip equivocado | Comprueba `clip_start_time` antes de comparar |
| `lom_id` puede repetirse entre pistas (visto en el set de Juan) | Escribir en la pista equivocada | Busca la pista por nombre exacto antes de cada escritura; si hay dos iguales, se detiene |

**Bloques de la guía:** cada clip lleva una nota inaudible (nota 0, velocity 1), porque un clip vacío obliga al script a usar una API antigua de notas que puede no existir
en Live 12. La pista guía no tiene instrumento, así que no suena.

## 3. Puente: script instalado, sin copia

El spec §5 proponía una copia parcial del Remote Script. El de GroovePilot ya está activo en `127.0.0.1:9877` y admite varios clientes a la vez, así que esta entrega
habla con él directamente: no instala nada ni exige reiniciar Live. **Alternativa:** una copia evita depender de cambios futuros de GroovePilot, a cambio de otra ranura de
superficie de control. Se decide si esa dependencia molesta.

## 4. Editar es el dato

Comparar el borrador con lo que queda después de editarlo es un problema de emparejamiento:

1. Las notas con igual altura e inicio son la misma; se anotan cambios de velocity y duración.
2. Entre las restantes, cada una se empareja con la más cercana de igual altura, a menos de medio tiempo: es una nota **movida**.
3. Lo que sobra es una nota **quitada** o **añadida**.

Es una heurística: dos notas intercambiadas se ven como dos movidas. Alcanza para documentar el criterio. El borrador guardado es inmutable y cada revisión agrega un evento nuevo.

## 5. Tres kicks distintos

Se usan `sintetizar_en_genero` y la selección *farthest-first* (`percusion-kits-808-organica.md` §10): se generan más candidatos de los necesarios y se conservan los tres más
distintos. Todos quedan afinados a la tonalidad con el perfil `organic`. Solo el elegido se copia a `00 Elegidos` en la Library (`apps/docs/produccion/library-grooveforge.md`), para no llenar el navegador de Live.

**Mientras no exista carga automática** (spec §9), Juan arrastra el kick elegido a `KICK`. El borrador usa la nota 60, donde un Simpler toca sin transponer. La pista `SC GHOST`
ya viene a negras en el template y el borrador también, así que el motor no la toca.

## 6. Protección del template

Un set guardado como `TEMPLATE*.als` es la base de trabajo, y escribir en él invita a sobrescribirla con un Cmd+S. El motor rechaza cualquier escritura si el archivo abierto se llama
así. Un set nuevo creado desde el template no tiene ruta y se acepta.

## 7. Plantilla provisional

| Bloque | Compases | Kick |
|---|---|---|
| Intro | 32 | Sí |
| Build | 16 | Sí |
| Drop | 48 | Sí |
| Break | 32 | No |
| Build | 16 | No |
| Drop | 48 | Sí |
| Outro | 20 | Sí |

Son 212 compases: 6:57 a 122 BPM, igual que el clip de `SC GHOST`. Es una **hipótesis** para probar el flujo, no una regla de arrangement. `SC GHOST` sigue sonando en los tramos sin kick; solo afecta a lo que
se envíe al Rumble, y el kick no envía nada ahí.

## 8. Cómo se valida

**Tests** contra un Live simulado que imita el protocolo, el clip más cercano y la sobrescritura: reglas de escritura, diferencias de notas, protección del template, registro.

**Con Live real, lo que el simulado no prueba:** que los clips aparezcan como se espera, que las notas leídas sean relativas al clip y qué revierte Cmd+Z, porque `create_midi_track` no abre un paso de undo propio.

**Lo que decide es Juan:** si le ahorró tiempo frente a hacerlo a mano.
