# ADR-004: Hardware abierto y el camino a vivir de esto en 2 años

**Status:** Proposed. **Se revisa al cierre de la Temporada 1** (EP17, el live set con el Brain y GroovePush),
con los datos reales del canal (ADR-002, enmienda del 2026-10-31). Ideas a evaluar entonces: el canal como "el
artista ingeniero" (música + instrumentos + experimentos), la escalera de productos, los talleres con kit y un kit de
audio abierto para crear efectos con IA.
**Date:** 2026-10-31
**Deciders:** Juan Guerrero
**Modifica (parcial):** ADR-002 §1 ("no se monetiza todavía"): sigue valiendo **hasta el cierre del Acto 1** de la
Temporada 1; desde ahí se activan las fuentes de ingreso de abajo, por puntos de control.
**Choca con, y no se modifica sin instrucción de Juan:**
- `01-modelo-rampa-y-apertura.md` §3.3 (reserva el código completo de los engines y los gerbers de producción).
- `00-master-strategy.md` §3 (el Brain como producto cerrado a $599). Es spec inmutable: si este ADR se acepta, se
  anota la desviación aquí y en el `00` solo con instrucción explícita.

---

## Context

- Hay movimiento hacia el hardware abierto en la industria. Ejemplo verificado el 2026-10-26: Behringer publicó en
  GitHub el código de su KICK; no se sabe si es parte de algo más grande. Su negocio principal siguen siendo clones
  cerrados.
- Casos de hardware abierto que se sostienen: Mutable Instruments (diseños abiertos, vendió módulos durante años),
  Moritz Klein (serie educativa en YouTube + kits con Erica Synths), LookMumNoComputer (YouTube + kits + talleres),
  Electro-Smith (hardware con SDK abierto).
- Juan quiere que GrooveMakers sea su fuente de ingresos: **meta de 6.000 USD/mes en ~2 años**, sin dejar su trabajo
  antes de tiempo.
- Tiempo disponible: 10-18 h/semana. Un device a la vez (ADR-002).
- Juan tiene además un **laboratorio de IA local generativa** para sus producciones: es un proyecto aparte; podría
  servir a otros artistas en el futuro.

## Decision

### 1. El Brain se abre, por etapas

| Etapa | Cuándo | Qué se abre | Licencia |
|---|---|---|---|
| A | Ya, con los episodios | Theory docs y documentación | CC BY-SA 4.0 |
| B | Al cierre del Acto 1 (EP9) | Firmware del sketch49 y engines | GPL-3.0 (las mejoras vuelven abiertas) |
| C | Con el Brain probado en su caja (EP10-EP12) y la auditoría del 2026-10-22 cerrada | Esquemáticos, PCB, caja | CERN-OHL-S v2 |

- **Sigue cerrado:** GroovePilot (backend, modelos), pesos de ML, datos de clientes y de proveedores.
- **El nombre no se abre:** GrooveForge y GrooveMakers se registran como marca (ADR-003 §5, clases 9, 15 y 41) antes
  de la primera venta. Cualquiera puede fabricar el diseño; nadie puede venderlo con el nombre.
- Antes de la etapa B: revisar que las licencias de las dependencias (Teensy Audio Library, LVGL, FastLED) sean
  compatibles con GPL-3.0.
- No se abre el hardware antes de la etapa C: un diseño con errores conocidos genera soporte, no comunidad.

### 2. Fuentes de ingreso (orden de activación)

| # | Fuente | Se activa | Escenario a los 24 meses (hipótesis, no promesa) |
|---|---|---|---|
| 1 | Patrocinio de fabricantes de PCB (JLCPCB, PCBWay) | Mes 4-8, con el EP7 / EP10 | 500-1.000 USD/mes |
| 2 | Sample packs y proyectos de Ableton con el sonido del Brain (para **cualquier** productor) | Mes 5-6 (al cierre del Acto 1) | 600-1.000 |
| 3 | Membresía (YouTube o Patreon): acceso anticipado, proyectos, presets, llamada mensual | Mes 6-9 | 1.000-1.500 (≈ 150-200 miembros × 7 USD) |
| 4 | Curso / bootcamp en español "construye tu synth con Teensy" (online + talleres en Bogotá) | Mes 10-15 | 1.000-1.500 promedio (lanzamientos por cohorte) |
| 5 | Kit del Brain (PCB + piezas + guía), preventa en Crowd Supply o la web | Mes 15-20 | 1.000-2.000 promedio (≈ 15-25 kits/mes × 70-90 USD netos) |
| 6 | YouTube (anuncios) | Al cumplir los requisitos | 200-500 |
| — | Brain armado | No antes del año 3 (certificaciones, garantía, soporte) | — |
| — | GroovePilot y el laboratorio de IA local como producto para artistas | Solo si la comunidad lo pide (ADR-002) | potencial, fuera del escenario |
| | | **Total del escenario** | **≈ 4.300-7.500 USD/mes** |

Ninguna fuente sola llega a la meta; la meta exige 4-5 funcionando a la vez.

### 3. Lo que exige la meta (el supuesto más débil)

Con esa mezcla, 6.000 USD/mes suponen aproximadamente **30-50 mil suscriptores, 3-5 mil correos** y unos 150-200
miembros pagos. Para un canal nuevo de nicho **solo en español**, en 2 años, eso es el escenario optimista.
Palancas para hacerlo posible:

1. **Inglés desde el principio.** El público de synth DIY es sobre todo angloparlante. El canal es de voz en off, sin
   cara: se puede publicar cada episodio con **pista de audio en inglés** (la voz grabada por Juan o doblada) y
   subtítulos. Es la palanca más grande y la más barata.
2. **La lista de correo** abierta ya (pendiente desde ADR-002): es lo único que no depende del algoritmo.
3. **Constancia:** un episodio cada 2 semanas y 2 Shorts por semana, sin pausas largas.
4. **Productos para todos los productores** (packs, curso), no solo para quien tiene un Brain.

### 4. Puntos de control

| Mes | Meta | Si no se cumple |
|---|---|---|
| 6 | 1.000 suscriptores, 300 correos, primer pack publicado | Cambiar el formato o el idioma, no el proyecto |
| 12 | 1.000 USD/mes; el primer producto vende ≥ 50 unidades | Preguntar a la lista qué compraría; posponer el curso |
| 18 | 3.000 USD/mes; la preventa del kit llega a su mínimo | Sin kit: seguir con contenido, curso y membresía |
| 24 | 6.000 USD/mes | Ajustar el plazo a 36 meses con los datos reales |

**Regla de salida del trabajo:** Juan deja su fuente actual solo cuando GrooveMakers le dé, **3-6 meses seguidos**,
al menos la mitad de lo que necesita para vivir. Mientras tanto, reinvertir en tiempo: cuando los ingresos lo
permitan, comprar horas (bajar horas del trabajo, contratar edición) antes que equipo.

### 5. El laboratorio de IA local

Proyecto aparte, para la producción de Juan. En el canal aparece como herramienta en cameos, igual que GroovePilot.
No se construye como producto para artistas hasta que la comunidad lo pida; entonces se evalúa en un ADR propio.

## Consequences

- **Más fácil:** el canal gana sentido ("constrúyelo tú también"); los kits y el curso salen del mismo material de
  los episodios; la marca registrada protege lo que importa.
- **Más difícil:** cualquiera puede fabricar el Brain; el soporte a quienes lo armen consume tiempo (mitigar con
  foro o Discord y documentación); la versión en inglés suma trabajo por episodio.
- **Riesgo principal:** la dispersión. Activar las fuentes en orden, una por punto de control, nunca todas juntas.

## Action Items

1. [ ] Juan acepta, ajusta o rechaza este ADR.
2. [ ] Abrir la lista de correo antes de publicar el EP1 (ya pendiente en ADR-002).
3. [ ] Decidir el inglés: pista de audio doblada desde el EP1, o subtítulos primero.
4. [ ] Si se acepta: actualizar `01-modelo-rampa-y-apertura.md` §3.3 con las etapas A-C, y anotar la desviación del
   `00` §3 (con instrucción explícita).
5. [ ] Antes de la etapa B: revisión de licencias de dependencias.
6. [ ] Antes de la primera venta: registro de marca (ADR-003 §5).
