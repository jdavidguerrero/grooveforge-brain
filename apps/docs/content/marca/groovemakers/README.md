# GrooveMakers — marca y media

> Identidad visual v1 de Claude Design (dirección **1b, "El surco"**: el surco del vinilo que se vuelve pista de PCB
> y sale por una vía). Nombre y cuentas: `../../adr/ADR-003-nombre-y-cuentas-groovemakers.md`.

| Carpeta | Qué es |
|---|---|
| `media/` | **Los archivos listos para usar** (PNG, SVG, ICO, HTML) |
| `fuente-claude-design/` | El diseño original. Ábrelo en Claude Design para ver la hoja de estilo completa o cambiar algo |
| `generador/` | El script que exporta `media/` con las medidas exactas del diseño |

---

## ⚠ Antes de usar: falta Satoshi

El logo con nombre y los títulos usan **Satoshi** (gratis, licencia ITF FFL, de fontshare.com), que no se puede
descargar desde el entorno donde se generaron estos archivos. Por eso hay dos tipos de archivo en `media/`:

- **Finales:** todo lo que no lleva texto en Satoshi. El isotipo, los avatares, el favicon, la marca de agua, el marco
  de 7", los sellos de Reels y los tercios inferiores (estos usan IBM Plex Mono, que sí está incluida).
- **Vista previa** (terminan en `__vista-previa-sin-satoshi.png`): el logo con nombre, el banner, la pantalla final,
  las miniaturas y el marco impreso. El texto sale con una fuente de reemplazo. **No los publiques así.**

**Para tener los finales (5 minutos):**
1. Descarga Satoshi de **fontshare.com** (botón "Download family").
2. Copia los archivos `Satoshi-Medium`, `Satoshi-Bold` y `Satoshi-Black` (`.otf`, `.ttf` o `.woff2`) en
   `generador/fonts/satoshi/`. Esa carpeta no se sube al repo.
3. En `generador/`:
   ```bash
   npm i playwright && npx playwright install chromium   # solo la primera vez
   node generar.mjs
   ```
   Se regenera todo `media/` y desaparecen los sufijos de vista previa.

O súbeme el zip de Satoshi en una sesión y lo regenero yo.

---

## Qué va en cada lugar

### YouTube (`media/youtube/`)

| Archivo | Dónde se sube |
|---|---|
| `avatar-800.png` | YouTube Studio → Personalización → Imagen de perfil |
| `banner-2560x1440.png` | Personalización → Imagen del banner. `…-con-guias` muestra el área segura (no se sube) |
| `marca-de-agua-150.png` | Personalización → Marca de agua del video (aparece en la esquina de todos los videos) |
| `pantalla-final-1920x1080.png` | Los últimos 20 s de cada video. En YouTube Studio → Pantalla final, pon los dos videos sugeridos y el botón de suscribirse **sobre las zonas oscuras** (`…-con-guias` muestra dónde) |
| `miniaturas/t1-epNN-….png` | **Capa para poner ENCIMA de la foto del banco**: la zona de la derecha es transparente. En tu editor: foto abajo, esta capa arriba, exportar 1280 × 720. `ejemplo-t1-ep01-con-foto` muestra el resultado con tu foto del banco |
| `miniaturas/plantilla-estudio.png`, `plantilla-taller.png` | Para episodios futuros. Mejor: agrega el episodio al generador (abajo) y sale con su título |

Carril **Estudio** = teal y anillo (el sonido). Carril **Taller** = ámbar y cuadrado (el hardware).

### Instagram (`media/instagram/`)

| Archivo | Dónde se usa |
|---|---|
| `avatar-1080.png` | Foto de perfil de `@groovemakers.co` |
| `sello-reels-t1-epNN.png` | Capa transparente de 1080 × 1920 **encima de cada Reel / Short** del episodio. Queda fuera de las zonas que tapa la interfaz de Instagram (`sello-reels-con-guias` las muestra) |

### Edición de video (`media/video/tercios-inferiores/`)

PNG transparentes de 1920 × 1080, **ya en su posición**: se ponen en una pista encima del video, sin moverlos.

- `archivo-…` (abajo a la izquierda, vía teal): el archivo que se ve en pantalla. Uno por cada archivo de los guiones
  del EP1, EP2 y EP3.
- `dato-…` (arriba a la derecha, punto ámbar): un dato técnico.
- `ia-…` (punto **púrpura**): el rótulo del cameo de GroovePilot del EP2. El púrpura es solo para la IA.

### Logo (`media/logo/`)

| Archivo | Para qué |
|---|---|
| `isotipo-{oscuro,claro,blanco,negro}.svg` | El símbolo en vector: imprenta, láser, vinilo de corte, cualquier tamaño |
| `isotipo-…-1024.png` | El símbolo con fondo transparente |
| `isotipo-simplificado-….svg` | Versión sin el surco interior, para tamaños muy pequeños (16-24 px) |
| `logo-horizontal-…`, `logo-apilado-…` | Símbolo + nombre, fondo transparente |
| `favicon.ico`, `favicon-*.png` | Para groovemakers.co (180 = ícono de iPhone, 512 = Android) |

**Variantes:** `oscuro` (blanco + teal) y `blanco` van sobre fondos oscuros; `claro` (negro + teal) y `negro`, sobre
fondos claros. **No** usar el púrpura en el logo, ni estirarlo, rotarlo o ponerle brillos (hoja de estilo).

### El set (`media/marco/`)

| Archivo | Para qué |
|---|---|
| `marco-impreso-13x18cm-con-sangrado-3mm-300dpi.png` | Para la imprenta (portarretrato de la mesa). Pide **papel mate** y, si se puede, **negro enriquecido C60 M40 Y40 K100** para que no se vea gris en cámara |
| `marco-impreso-13x18cm-sin-sangrado-300dpi.png` | Para imprimir en casa |
| `marco-pantalla-7in-animado.html` | **Para la pantalla de 7"**: ábrelo en el navegador de la Pi en pantalla completa. Un LED recorre el anillo en 60 s. No necesita internet |
| `marco-pantalla-7in-1280x720.png` | La versión fija, si prefieres una imagen |

---

## Agregar un episodio nuevo

En `generador/generar.mjs`:
- **Miniatura y sello de Reels:** agrega una línea a `EPISODES`, por ejemplo
  `{ n: '04', lane: 'estudio', title: 'Seis<br>motores', slug: 'seis-motores' }` (máximo 3 palabras; si el título no
  cabe, se achica solo).
- **Tercios inferiores:** agrega `t1ep04` a `LOWER` con sus `archivos` y `datos`.

Después `node generar.mjs`.
