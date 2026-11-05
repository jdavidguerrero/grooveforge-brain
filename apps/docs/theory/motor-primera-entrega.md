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

Tampoco se pueden crear clips directamente en Arrangement desde un script: se crea un clip temporal en Session y se usa `duplicate_clip_to_arrangement`.
Duplicar ocupa el tramo destino, así que podría pisar un clip de Juan: el motor solo escribe en una pista sin clips de Arrangement y, si no, crea otra.

**Pistas por nombre:** el motor busca la pista por nombre exacto antes de cada escritura. Si hay dos iguales, se detiene.

**Bloques de la guía:** cada clip lleva una nota inaudible (nota 0, velocity 1). La pista guía no tiene instrumento, así que no suena. Nombre y color se ponen en el clip temporal y la copia los hereda, sin depender del orden de `arrangement_clips`.

## 3. Puente: script propio

La primera versión usó el Remote Script de GroovePilot, que lo tiene todo mezclado y le faltan comandos como cargar racks. **Decisión de Juan:** un script paralelo solo para el motor, donde se van agregando cosas. GroovePilot queda intacto, y los dos pueden estar activos a la vez.

`GFMotor` escucha en `127.0.0.1:9878` y separa tres piezas: red (`superficie.py`), comandos sobre el set (`comandos.py`) y búsqueda en el navegador (`buscar.py`). Los comandos corren solo en el hilo principal de Live, y se prueban fuera con un set simulado. Comparado con el puente anterior:

- **Lectura exacta:** `read_clip` nunca devuelve el clip más cercano.
- **Sin ejecución tardía:** si un pedido agota su tiempo, se cancela, para que no actúe después de informar el fallo.
- **Lista corta y explícita:** solo existen los comandos que el motor usa, sin escritura genérica de propiedades.

**Costo:** Live solo lee estos scripts al arrancar, así que cada comando nuevo exige reiniciarlo. Cargar plugins se agregará al llegar al bajo, con el VST del Brain.

## 4. Editar es el dato

Comparar el borrador con lo que queda después de editarlo es un problema de emparejamiento:

1. Las notas con igual altura e inicio son la misma; se anotan cambios de velocity y duración.
2. Entre las restantes, cada una se empareja con la más cercana de igual altura, a menos de medio tiempo: es una nota **movida**.
3. Lo que sobra es una nota **quitada** o **añadida**.

Es una heurística: dos notas intercambiadas se ven como dos movidas. Alcanza para documentar el criterio. El borrador guardado es inmutable y cada revisión agrega un evento nuevo.

## 5. Tres kicks en un Drum Rack

Cada tanda tiene un mismo golpe duro y gordo, y cinco colas de madera o de piel (`percusion-kits-808-organica.md` §12), afinadas a la tonalidad. Así la elección depende solo de lo que viene después del golpe.

**Decisión de Juan:** el clip son 16 compases a negras en el compás 1. Él genera las tandas que quiera y elige tres kicks, con un contador. Al elegir el tercero, el motor crea el Drum Rack (C1, C#1 y D1, en el orden elegido) y lo pone en `KICK`.

Esto corrige un fallo de la primera versión: cada tanda creaba su propio rack, y elegir en una tanda nueva movía la nota mientras seguía cargado el rack de otra, así que el clip sonaba con otro kick. Ahora hay un único rack por selección.

- **Se oye en contexto:** con el rack puesto, `Usar` mueve la nota del clip al pad, si el clip sigue igual a como lo escribió el motor. Si Juan lo editó, no se toca (regla 3) y se le indica la nota.
- **Datos para la IA:** el evento `motor_seleccion` guarda lo elegido junto a todo lo presentado; lo no elegido también informa. `escuchadas` queda `unknown`, porque el motor no sabe qué se oyó.
- **Poner el rack en la pista:** `GFMotor` selecciona la pista y usa `browser.load_item` con el item de Places. Después borra los racks anteriores del motor, reconocidos por el nombre exacto que él genera; un rack de Juan no coincide. Va en un solo paso de deshacer, y si la carga no encuentra el archivo no borra nada. Live tarda en ver un archivo nuevo en Places, así que reintenta hasta 20 s. Si falla, el rack queda creado y la interfaz indica el motivo y dónde está.

**El `.adg` parte de un rack real de Juan** (`808 Core Kit.adg`), porque el formato no está documentado y cambia entre versiones de Live: se conserva el molde de un pad y se cambia el sample, igual que `make_gf_racks.py`. No crea ni modifica `.als` (regla 7). La pista `SC GHOST`
ya viene a negras en el template, así que el motor no la toca.

## 6. Protección del template

Un set guardado como `TEMPLATE*.als` es la base de trabajo, y escribir en él invita a sobrescribirla con un Cmd+S. El motor rechaza cualquier escritura si el archivo abierto se llama
así. Un set nuevo creado desde el template no tiene ruta y se acepta.

## 7. Plantilla provisional

| Bloque | Compases | Color |
|---|---|---|
| INTRO | 32 | Teal |
| BUILD | 16 | Ámbar |
| DROP | 48 | Naranja |
| BREAK | 32 | Púrpura |
| BUILD | 16 | Ámbar |
| DROP | 48 | Naranja |
| OUTRO | 20 | Menta |

Son 212 compases: 6:57 a 122 BPM, igual que el clip de `SC GHOST`. Es una **hipótesis** para probar el flujo, no una regla de arrangement. Cada tipo de sección tiene su color para reconocerla de un vistazo; el color se asigna por la posición en `arrangement_clips`.

## 8. Cómo se valida

**Tests** contra un puente y un set simulados, con la sobrescritura al duplicar: reglas de escritura, diferencias de notas, protección del template, registro.

**Con Live real, lo que el simulado no prueba:** que el rack se cargue y reemplace al anterior, que los clips hereden nombre y color, que las notas leídas sean relativas al clip y qué revierte Cmd+Z.

**Lo que decide es Juan:** si le ahorró tiempo frente a hacerlo a mano.
