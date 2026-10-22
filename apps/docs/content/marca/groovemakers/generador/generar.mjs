// Exporta los archivos de la marca GrooveMakers (dirección 1b, "El surco") a PNG y SVG.
//
// Reproduce las piezas de fuente-claude-design/GrooveMakers Identidad.dc.html con sus medidas exactas.
// Uso (desde esta carpeta):
//   npm i playwright && npx playwright install chromium     # una vez
//   node generar.mjs                                         # escribe ../media/
//
// Satoshi (logo y títulos) no viene en el repo: descárgala gratis de fontshare.com y copia los archivos
// Satoshi-*.otf / .ttf / .woff2 en fonts/satoshi/. Sin ella, las piezas con texto en Satoshi salen con el sufijo
// "__vista-previa-sin-satoshi" (fuente de reemplazo); el resto ya es final.

import fs from 'node:fs';
import path from 'node:path';
import http from 'node:http';
import { fileURLToPath } from 'node:url';

const { chromium } = await import(process.env.PLAYWRIGHT_MODULE || 'playwright');

const HERE = path.dirname(fileURLToPath(import.meta.url));

// ── Tokens (hoja de estilo v1) ───────────────────────────────────────────────────────────────────────────────
const C = {
  bg: '#0A0A0A', s1: '#141414', s2: '#1C1C1C', line: '#242424', text: '#F2F2F2', muted: '#8A8A8A',
  teal: '#1D9E75', tealL: '#5DCAA5', tealD: '#0E5040', purple: '#7A6FE0', amber: '#E8B84A', frameGray: '#CFCFCF',
};

// ── Fuentes ─────────────────────────────────────────────────────────────────────────────────────────────────
function findSatoshi() {
  const dir = path.join(HERE, 'fonts', 'satoshi');
  const found = {};
  const weights = { Regular: 400, Medium: 500, Bold: 700, Black: 900 };
  const rank = { woff2: 0, woff: 1, otf: 2, ttf: 3 };
  const walk = (d) => {
    for (const e of fs.readdirSync(d, { withFileTypes: true })) {
      const p = path.join(d, e.name);
      if (e.isDirectory()) walk(p);
      const m = e.name.match(/^Satoshi-(Regular|Medium|Bold|Black)\.(woff2|woff|otf|ttf)$/);
      if (m) {
        const w = weights[m[1]];
        if (!found[w] || rank[m[2]] < rank[found[w].ext]) found[w] = { rel: path.relative(HERE, p), ext: m[2] };
      }
    }
  };
  if (fs.existsSync(dir)) walk(dir);
  return found;
}
const satoshi = findSatoshi();
const HAS_SATOSHI = [500, 700, 900].every((w) => satoshi[w]);
const OUT = path.join(HERE, '..', 'media');
const PREVIEW_SUFFIX = '__vista-previa-sin-satoshi';

const fmt = { woff2: 'woff2', woff: 'woff', otf: 'opentype', ttf: 'truetype' };
const FONT_CSS = [
  ...Object.entries(satoshi).map(([w, f]) =>
    `@font-face{font-family:'Satoshi';font-weight:${w};src:url('/${f.rel.split(path.sep).join('/')}') format('${fmt[f.ext]}')}`),
  ...[400, 600].flatMap((w) => ['latin', 'latin-ext'].map((s) =>
    `@font-face{font-family:'IBM Plex Mono';font-weight:${w};src:url('/fonts/plex-mono/ibm-plex-mono-${s}-${w}-normal.woff2') format('woff2')}`)),
].join('\n');
const SANS = "'Satoshi','Helvetica Neue',Arial,sans-serif";
const MONO = "'IBM Plex Mono',monospace";

// ── El isotipo 1b ───────────────────────────────────────────────────────────────────────────────────────────
// Mismas medidas que GM Logo.dc.html (dir b), con los trazos discontinuos en valores absolutos (sin pathLength)
// para que el SVG se vea igual en cualquier editor, y el agujero de la vía como máscara: transparente de verdad.
let maskSeq = 0;
function mark({ size = 100, fg = C.text, accent = C.teal, simple = false } = {}) {
  const sw = simple ? 8 : 5, centerR = simple ? 14 : 11, viaR = simple ? 9 : 7.5, viaH = simple ? 3.5 : 3;
  const o = 2 * Math.PI * 38, i = 2 * Math.PI * 26, id = `via${maskSeq++}`;
  const r = (n) => +n.toFixed(3);
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${size}" height="${size}" viewBox="0 0 100 100" style="display:block;flex:none">` +
    `<defs><mask id="${id}"><rect width="100" height="100" fill="#fff"/><circle cx="76.87" cy="23.13" r="${viaH}" fill="#000"/></mask></defs>` +
    `<g mask="url(#${id})">` +
    `<circle cx="50" cy="50" r="38" fill="none" stroke="${fg}" stroke-width="${sw}" stroke-dasharray="${r(o * 0.93)} ${r(o * 0.07)}" stroke-dashoffset="${r(o * 0.09)}"/>` +
    (simple ? '' : `<circle cx="50" cy="50" r="26" fill="none" stroke="${fg}" stroke-width="${sw}" stroke-dasharray="${r(i * 0.9)} ${r(i * 0.1)}" stroke-dashoffset="${r(i * 0.075)}"/>`) +
    `<line x1="50" y1="50" x2="76.87" y2="23.13" stroke="${accent}" stroke-width="${sw}" stroke-linecap="round"/>` +
    `<circle cx="50" cy="50" r="${centerR}" fill="${accent}"/>` +
    `<circle cx="76.87" cy="23.13" r="${viaR}" fill="${accent}"/>` +
    `</g></svg>`;
}

function logo({ layout = 'h', size = 64, fg = C.text, accent = C.teal, simple = false } = {}) {
  if (layout === 'mark') return mark({ size, fg, accent, simple });
  const stack = layout === 'stack';
  const gap = Math.round(size * (stack ? 0.2 : 0.28)), px = Math.round(size * (stack ? 0.34 : 0.46));
  return `<div style="display:inline-flex;flex-direction:${stack ? 'column' : 'row'};align-items:center;gap:${gap}px;line-height:1">` +
    mark({ size, fg, accent, simple }) +
    `<div style="font-family:${SANS};font-weight:700;font-size:${px}px;letter-spacing:-0.025em;color:${fg};white-space:nowrap">GrooveMakers</div></div>`;
}

const grooveBg = (x, y, inner) =>
  `background:radial-gradient(circle at ${x}px ${y}px, ${C.bg} 0 ${inner}px, transparent ${inner + 1}px), ` +
  `repeating-radial-gradient(circle at ${x}px ${y}px, ${C.bg} 0 56px, ${C.s1} 57px 60px)`;

// ── Piezas ──────────────────────────────────────────────────────────────────────────────────────────────────
const assets = [];
const add = (file, w, h, body, opts = {}) => assets.push({ file, w, h, body, ...opts });

// Logo: cuatro variantes de color. "oscuro" y "blanco" van sobre fondos oscuros; "claro" y "negro", sobre claros.
const VARIANTS = {
  oscuro: { fg: C.text, accent: C.teal },
  claro: { fg: C.bg, accent: C.teal },
  blanco: { fg: C.text, accent: C.text },
  negro: { fg: C.bg, accent: C.bg },
};
const svgFiles = [];
for (const [v, col] of Object.entries(VARIANTS)) {
  svgFiles.push([`logo/isotipo-${v}.svg`, mark({ size: 1024, ...col })]);
  add(`logo/isotipo-${v}-1024.png`, 1024, 1024, mark({ size: 1024, ...col }), { transparent: true });
  add(`logo/logo-horizontal-${v}.png`, 0, 0, logo({ layout: 'h', size: 400, ...col }), { transparent: true, element: true, pad: 88 });
  add(`logo/logo-apilado-${v}.png`, 0, 0, logo({ layout: 'stack', size: 520, ...col }), { transparent: true, element: true, pad: 114 });
}
svgFiles.push(['logo/isotipo-simplificado-oscuro.svg', mark({ size: 1024, simple: true })]);
svgFiles.push(['logo/isotipo-simplificado-claro.svg', mark({ size: 1024, simple: true, ...VARIANTS.claro })]);
for (const s of [16, 32, 48, 180, 512]) {
  // Favicon: versión simplificada (sin surco interior) desde 16 hasta 48 px, como pide la hoja de estilo.
  add(`logo/favicon-${s}.png`, s, s,
    `<div style="width:${s}px;height:${s}px;background:${C.bg};display:flex;align-items:center;justify-content:center">${mark({ size: Math.round(s * 0.86), simple: s <= 48 })}</div>`);
}

// YouTube
add('youtube/avatar-800.png', 800, 800,
  `<div style="width:800px;height:800px;background:${C.bg};display:flex;align-items:center;justify-content:center">${mark({ size: 520 })}</div>`);
const banner = (guides) =>
  `<div style="position:relative;width:2560px;height:1440px;overflow:hidden;${grooveBg(774, 720, 340)}">
    <div style="position:absolute;left:507px;top:508px;width:1546px;height:423px;display:flex;align-items:center;justify-content:center">
      <div style="display:flex;flex-direction:column;gap:22px">${logo({ layout: 'h', size: 220 })}
        <div style="margin-left:282px;display:flex;align-items:center;gap:18px;font-family:${SANS};font-weight:500;font-size:46px;letter-spacing:-0.01em;color:${C.text}">
          <span style="width:14px;height:14px;border-radius:50%;background:${C.teal};flex:none"></span>Construyendo instrumentos para hacer música</div>
      </div></div>
    ${guides ? `<div style="position:absolute;left:507px;top:508px;width:1546px;height:423px;border:3px dashed ${C.purple}"></div>
      <div style="position:absolute;left:507px;top:460px;font-family:${MONO};font-size:26px;color:${C.purple}">área segura · todos los dispositivos</div>` : ''}
  </div>`;
add('youtube/banner-2560x1440.png', 2560, 1440, banner(false));
add('youtube/banner-2560x1440-con-guias.png', 2560, 1440, banner(true));
add('youtube/marca-de-agua-150.png', 150, 150,
  `<div style="width:150px;height:150px;display:flex;align-items:center;justify-content:center">${mark({ size: 132 })}</div>`, { transparent: true });

const endScreen = (guides) => {
  const slot = (l, t, w, h, txt, round) => guides
    ? `<div style="position:absolute;left:${l}px;top:${t}px;width:${w}px;height:${h}px;${round ? 'border-radius:50%;' : ''}border:3px dashed #3A3A3A;background:${C.bg};display:flex;align-items:center;justify-content:center;font-family:${MONO};font-size:${round ? 20 : 28}px;color:${C.muted}">${txt}</div>`
    : `<div style="position:absolute;left:${l}px;top:${t}px;width:${w}px;height:${h}px;${round ? 'border-radius:50%;' : ''}background:${C.bg}"></div>`;
  return `<div style="position:relative;width:1920px;height:1080px;overflow:hidden;${grooveBg(1700, 1000, 120)}">
    <div style="position:absolute;left:120px;top:100px">${logo({ layout: 'h', size: 72 })}</div>
    ${slot(120, 300, 800, 450, 'video sugerido 1')}${slot(1000, 300, 800, 450, 'video sugerido 2')}
    <div style="position:absolute;left:120px;top:830px;display:flex;align-items:center;gap:36px">
      <div style="position:relative;width:160px;height:160px">${slot(0, 0, 160, 160, 'suscribir', true)}</div>
      <div style="display:flex;flex-direction:column;gap:10px">
        <div style="font-family:${SANS};font-weight:700;font-size:56px;letter-spacing:-0.02em;color:${C.text}">Sigue construyendo con nosotros</div>
        <div style="font-family:${MONO};font-size:26px;color:${C.tealL}">T1 · Construyendo mi synth</div>
      </div></div></div>`;
};
add('youtube/pantalla-final-1920x1080.png', 1920, 1080, endScreen(false));
add('youtube/pantalla-final-1920x1080-con-guias.png', 1920, 1080, endScreen(true));

// Miniaturas: capa para poner ENCIMA de la foto del banco (la zona de la foto queda transparente).
const LANES = {
  estudio: { label: 'ESTUDIO', color: C.teal, icon: `<span style="width:18px;height:18px;border-radius:50%;border:5px solid ${C.bg};box-sizing:content-box"></span>` },
  taller: { label: 'TALLER', color: C.amber, icon: `<span style="width:18px;height:18px;border:5px solid ${C.bg};box-sizing:content-box"></span>` },
};
const thumb = ({ lane, ep, title, photo }) => {
  const L = LANES[lane];
  return `<div style="position:relative;width:1280px;height:720px;overflow:hidden">
    ${photo ? `<img src="${photo}" style="position:absolute;left:440px;top:0;width:840px;height:720px;object-fit:cover;object-position:center 62%">` : ''}
    <div style="position:absolute;left:0;top:0;bottom:0;width:440px;background:${C.bg}"></div>
    <div style="position:absolute;left:439px;top:0;bottom:0;width:221px;background:linear-gradient(90deg, ${C.bg}, rgba(10,10,10,0))"></div>
    <div style="position:absolute;left:64px;top:64px;bottom:64px;width:720px;display:flex;flex-direction:column;justify-content:space-between">
      <div style="display:flex;gap:16px">
        <div style="display:flex;align-items:center;gap:14px;background:${L.color};color:${C.bg};font-family:${MONO};font-weight:600;font-size:32px;padding:10px 20px">${L.icon}${L.label}</div>
        ${ep ? `<div style="display:flex;align-items:center;border:3px solid ${C.text};color:${C.text};font-family:${MONO};font-weight:600;font-size:32px;padding:7px 18px">${ep}</div>` : ''}
      </div>
      <div><div data-fit="720" style="display:inline-block;white-space:nowrap;font-family:${SANS};font-weight:900;font-size:168px;line-height:0.9;letter-spacing:-0.045em;text-transform:uppercase;color:${C.text};padding-top:12px">${title}</div></div>
      ${logo({ layout: 'h', size: 56 })}
    </div></div>`;
};
// Títulos de los guiones (EP01_guion.md, EP02_guion.md, EP03_guion.md). Máximo 3 palabras.
const EPISODES = [
  { n: '01', lane: 'estudio', title: 'Mi propio<br>sonido', slug: 'mi-propio-sonido' },
  { n: '02', lane: 'estudio', title: 'Desde<br>cero', slug: 'desde-cero' },
  { n: '03', lane: 'taller', title: 'No<br>cabía', slug: 'no-cabia' },
];
add('youtube/miniaturas/plantilla-estudio.png', 1280, 720, thumb({ lane: 'estudio', ep: 'T1 · EP00', title: 'Título<br>aquí' }), { transparent: true });
add('youtube/miniaturas/plantilla-taller.png', 1280, 720, thumb({ lane: 'taller', ep: 'T1 · EP00', title: 'Título<br>aquí' }), { transparent: true });
for (const e of EPISODES) {
  add(`youtube/miniaturas/t1-ep${e.n}-${e.slug}.png`, 1280, 720, thumb({ lane: e.lane, ep: `T1 · EP${e.n}`, title: e.title }), { transparent: true });
}
add('youtube/miniaturas/ejemplo-t1-ep01-con-foto.png', 1280, 720,
  thumb({ lane: 'estudio', ep: 'T1 · EP01', title: EPISODES[0].title, photo: '/ejemplos/banco-2026-10-22.jpg' }));

// Tercios inferiores: 1920 × 1080 transparentes, cada uno ya en su posición.
const fileChip = (name, sub) => `<div style="position:relative;width:1920px;height:1080px">
  <div style="position:absolute;left:96px;bottom:96px;display:flex;align-items:center;gap:28px;background:rgba(10,10,10,0.9);border:1px solid ${C.line};padding:26px 40px 26px 28px">
    <svg width="44" height="44" viewBox="0 0 100 100" style="display:block;flex:none"><circle cx="50" cy="50" r="44" fill="${C.teal}"/><circle cx="50" cy="50" r="17" fill="${C.bg}"/></svg>
    <div style="display:flex;flex-direction:column;gap:8px;font-family:${MONO}">
      <div style="font-weight:600;font-size:46px;color:${C.text}">${name}</div>
      <div style="font-weight:400;font-size:28px;color:${C.tealL}">${sub}</div>
    </div></div></div>`;
const dataChip = (txt, dot = C.amber) => `<div style="position:relative;width:1920px;height:1080px">
  <div style="position:absolute;right:96px;top:96px;display:flex;align-items:center;gap:16px;background:rgba(10,10,10,0.9);border:1px solid ${C.line};padding:16px 24px;font-family:${MONO};font-weight:600;font-size:30px;color:${C.text}">
    <span style="width:14px;height:14px;border-radius:50%;background:${dot}"></span>${txt}</div></div>`;
const LOWER = {
  t1ep01: {
    archivos: [
      ['src/engines/', 'GrooveForge Brain / firmware'],
      ['fidelidad-engines.md §0', 'GrooveForge Brain / docs'],
      ['49-studio-brain.cpp', 'GrooveForge Brain / firmware'],
      ['ADR-004 · panel de Studio', 'GrooveForge Brain / docs'],
      ['07-studio-midi-expresion.md §3', 'GrooveForge Brain / docs'],
    ],
    datos: ['Teensy 4.1 · 600 MHz', 'SGTL5000 · Audio Shield', 'Raspberry Pi 3B · pantalla 5"', '6 motores · 16 presets c/u', 'SYNTH FX · ~6 % de CPU'],
  },
  t1ep02: {
    archivos: [
      ['fidelidad-engines.md §8', 'GrooveForge Brain / docs'],
      ['fidelidad-engines.md §9.3', 'GrooveForge Brain / docs'],
      ['polyblep_core.h', 'GrooveForge Brain / firmware'],
      ['05-studio-synth-fx.md §2', 'GrooveForge Brain / docs'],
    ],
    datos: ['Aliasing: −23 → −70 dB', 'Chorus I 0.513 Hz · II 0.863 Hz', 'Un motor a la vez'],
    ia: ['Idea MIDI: GroovePilot · Elección y edición: yo'],
  },
  t1ep03: {
    archivos: [
      ['claude-design-prompt-enclosure.md', 'GrooveForge Brain / diseño'],
      ['studio-enclosure.FCStd', 'GrooveForge Brain / 3D'],
      ['02-studio-pcb.md §2', 'GrooveForge Brain / docs'],
      ['panel-aluminio.dxf', 'GrooveForge Brain / 3D'],
      ['commit 2bc6abe', 'GrooveForge Brain / git'],
    ],
    datos: ['EC11 · cuerpo 5 mm', 'Choc V2 · placa a −3.4 mm', 'Caja 48 / 83 mm', 'PEM Ø4.22 → Ø3.0 mm'],
  },
};
const slug = (s) => s.toLowerCase().normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
for (const [ep, d] of Object.entries(LOWER)) {
  d.archivos.forEach(([n, s], i) => add(`video/tercios-inferiores/${ep}/archivo-${String(i + 1).padStart(2, '0')}-${slug(n)}.png`, 1920, 1080, fileChip(n, s), { transparent: true }));
  d.datos.forEach((t, i) => add(`video/tercios-inferiores/${ep}/dato-${String(i + 1).padStart(2, '0')}-${slug(t)}.png`, 1920, 1080, dataChip(t), { transparent: true }));
  // El púrpura es solo para la IA (hoja de estilo): el rótulo del cameo de GroovePilot lleva el punto púrpura.
  (d.ia || []).forEach((t, i) => add(`video/tercios-inferiores/${ep}/ia-${String(i + 1).padStart(2, '0')}-${slug(t)}.png`, 1920, 1080, dataChip(t, C.purple), { transparent: true }));
}
add('video/tercios-inferiores/plantilla-archivo.png', 1920, 1080, fileChip('nombre-de-archivo.md §0', 'GrooveForge Brain / docs'), { transparent: true });
add('video/tercios-inferiores/plantilla-dato.png', 1920, 1080, dataChip('Dato técnico · valor'), { transparent: true });

// Instagram / Reels / Shorts
add('instagram/avatar-1080.png', 1080, 1080,
  `<div style="width:1080px;height:1080px;background:${C.bg};display:flex;align-items:center;justify-content:center">${mark({ size: 702 })}</div>`);
const seal = (ep, guides) => `<div style="position:relative;width:1080px;height:1920px;${guides ? 'background:repeating-linear-gradient(135deg,#121212 0 24px,#161616 24px 48px);' : ''}overflow:hidden">
  ${guides ? `<div style="position:absolute;left:0;right:0;top:0;height:200px;border-bottom:4px dashed ${C.purple};display:flex;align-items:center;padding-left:40px;font-family:${MONO};font-size:28px;color:${C.purple}">UI de Instagram</div>
    <div style="position:absolute;left:0;right:0;bottom:0;height:440px;border-top:4px dashed ${C.purple};display:flex;align-items:center;padding-left:40px;font-family:${MONO};font-size:28px;color:${C.purple}">UI de Instagram · texto y audio</div>
    <div style="position:absolute;right:0;top:900px;width:160px;bottom:440px;border-left:4px dashed ${C.purple}"></div>` : ''}
  <div style="position:absolute;left:56px;top:240px;display:flex;align-items:center;gap:16px;background:rgba(10,10,10,0.72);padding:14px 22px 14px 16px">
    ${mark({ size: 44 })}<div style="font-family:${MONO};font-weight:600;font-size:28px;color:${C.text}">GrooveMakers · ${ep}</div></div></div>`;
for (const e of EPISODES) add(`instagram/sello-reels-t1-ep${e.n}.png`, 1080, 1920, seal(`T1 EP${e.n}`), { transparent: true });
add('instagram/sello-reels-con-guias.png', 1080, 1920, seal('T1 EP01', true));

// Marco de escritorio (impreso 13 × 18 cm a 300 dpi; pantalla de 7")
const PX_MM = 300 / 25.4;
const printFrame = (bleedMm) => {
  const b = Math.round(bleedMm * PX_MM), w = Math.round(130 * PX_MM), h = Math.round(180 * PX_MM), k = w / 520;
  return { w: w + 2 * b, h: h + 2 * b, body: `<div style="position:relative;width:${w + 2 * b}px;height:${h + 2 * b}px;background:${C.bg}">
    <div style="position:absolute;left:${b}px;top:${b}px;width:${w}px;height:${h}px;display:flex;align-items:center;justify-content:center">
      <div data-fit="${Math.round(w * 0.78)}" style="display:inline-block">${logo({ layout: 'stack', size: Math.round(230 * k) })}</div>
      <div style="position:absolute;left:0;right:0;bottom:${Math.round(44 * k)}px;text-align:center;font-family:${MONO};font-size:${Math.round(13 * k)}px;letter-spacing:.22em;color:${C.muted}">BOGOTÁ · COLOMBIA</div>
    </div></div>` };
};
for (const [name, bleed] of [['marco/marco-impreso-13x18cm-con-sangrado-3mm-300dpi.png', 3], ['marco/marco-impreso-13x18cm-sin-sangrado-300dpi.png', 0]]) {
  const f = printFrame(bleed);
  add(name, f.w, f.h, f.body);
}
// El anillo de 24 puntos con el LED que lo recorre (en el HTML animado gira en 60 s).
const ledRing = (animated) => {
  const dots = Array.from({ length: 24 }, (_, i) => {
    const a = (i * Math.PI) / 12;
    return `<circle cx="${(50 + 47 * Math.sin(a)).toFixed(3)}" cy="${(50 - 47 * Math.cos(a)).toFixed(3)}" r="0.9" fill="${C.s2}"/>`;
  }).join('');
  return `<svg viewBox="0 0 100 100" width="100%" height="100%" style="display:block">${dots}
    <g style="transform-origin:50px 50px;${animated ? 'animation:gmspin 60s linear infinite' : ''}">
      <circle cx="26.5" cy="9.3" r="0.9" fill="${C.tealD}"/><circle cx="37.84" cy="4.6" r="1.1" fill="#137A5B"/><circle cx="50" cy="3" r="1.4" fill="${C.teal}"/>
    </g></svg>`;
};
const frame7 = (animated) => `<div style="position:relative;width:1280px;height:720px;background:${C.bg};display:flex;align-items:center;justify-content:center;overflow:hidden">
  <div style="position:relative;width:600px;height:600px;display:flex;align-items:center;justify-content:center">
    <div style="position:absolute;inset:0">${ledRing(animated)}</div>${mark({ size: 320, fg: C.frameGray })}</div></div>`;
add('marco/marco-pantalla-7in-1280x720.png', 1280, 720, frame7(false));

// ── Render ──────────────────────────────────────────────────────────────────────────────────────────────────
const page = (body, transparent) => `<!doctype html><html><head><meta charset="utf-8"><style>
${FONT_CSS}
@keyframes gmspin{from{transform:rotate(0deg)}to{transform:rotate(360deg)}}
html,body{margin:0;padding:0;background:${transparent ? 'transparent' : C.bg};-webkit-font-smoothing:antialiased}
#root{display:inline-block}
</style></head><body><div id="root">${body}</div></body></html>`;

const TMP = path.join(HERE, '.tmp');
fs.mkdirSync(TMP, { recursive: true });
const server = http.createServer((req, res) => {
  const f = path.join(HERE, decodeURIComponent(req.url.split('?')[0]));
  if (!f.startsWith(HERE) || !fs.existsSync(f) || !fs.statSync(f).isFile()) { res.writeHead(404); return res.end(); }
  const ext = path.extname(f).slice(1);
  const type = { html: 'text/html', woff2: 'font/woff2', woff: 'font/woff', otf: 'font/otf', ttf: 'font/ttf', jpg: 'image/jpeg', png: 'image/png' }[ext] || 'application/octet-stream';
  res.writeHead(200, { 'content-type': type });
  fs.createReadStream(f).pipe(res);
});
await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
const port = server.address().port;

const browser = await chromium.launch();
const ctx = await browser.newContext({ deviceScaleFactor: 1 });
const pg = await ctx.newPage();
fs.rmSync(OUT, { recursive: true, force: true });

for (const [file, svg] of svgFiles) {
  const p = path.join(OUT, file);
  fs.mkdirSync(path.dirname(p), { recursive: true });
  fs.writeFileSync(p, svg.replace(' style="display:block;flex:none"', '') + '\n');
}
for (const a of assets) {
  fs.writeFileSync(path.join(TMP, 'pieza.html'), page(a.element ? `<div id="pad" style="padding:${a.pad}px">${a.body}</div>` : a.body, a.transparent));
  await pg.setViewportSize({ width: a.w || 3000, height: a.h || 2000 });
  await pg.goto(`http://127.0.0.1:${port}/.tmp/pieza.html`, { waitUntil: 'networkidle' });
  await pg.evaluate(() => document.fonts.ready);
  // data-fit: si el contenido pasa del ancho máximo, se reduce en proporción (títulos largos, margen de impresión).
  await pg.evaluate(() => document.querySelectorAll('[data-fit]').forEach((el) => {
    const w = el.getBoundingClientRect().width, max = Number(el.dataset.fit);
    if (w > max) el.style.zoom = String(max / w);
  }));
  const preview = !HAS_SATOSHI && a.body.includes("'Satoshi'");
  const p = path.join(OUT, preview ? a.file.replace(/\.png$/, `${PREVIEW_SUFFIX}.png`) : a.file);
  fs.mkdirSync(path.dirname(p), { recursive: true });
  const target = a.element ? pg.locator('#pad') : pg.locator('#root');
  await target.screenshot({ path: p, omitBackground: !!a.transparent });
  console.log(preview ? '◌' : '✓', path.relative(OUT, p));
}
// El marco animado para la pantalla de 7": un HTML autónomo (sin fuentes externas) para abrir en pantalla completa.
fs.writeFileSync(path.join(OUT, 'marco', 'marco-pantalla-7in-animado.html'), `<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>GrooveMakers</title>
<style>@keyframes gmspin{from{transform:rotate(0deg)}to{transform:rotate(360deg)}}
html,body{margin:0;height:100%;background:${C.bg};overflow:hidden;cursor:none}
body{display:flex;align-items:center;justify-content:center}
#f{width:1280px;height:720px;flex:none;transform-origin:center}</style></head>
<body><div id="f">${frame7(true)}</div>
<script>function fit(){var s=Math.min(innerWidth/1280,innerHeight/720);document.getElementById('f').style.transform='scale('+s+')'}addEventListener('resize',fit);fit();</script>
</body></html>
`);

// favicon.ico con los PNG de 16, 32 y 48 px adentro (el formato ICO admite PNG embebido tal cual).
const icoPngs = [16, 32, 48].map((s) => fs.readFileSync(path.join(OUT, 'logo', `favicon-${s}.png`)));
const icoHead = Buffer.alloc(6 + 16 * icoPngs.length);
icoHead.writeUInt16LE(0, 0); icoHead.writeUInt16LE(1, 2); icoHead.writeUInt16LE(icoPngs.length, 4);
let icoOffset = icoHead.length;
icoPngs.forEach((png, i) => {
  const s = [16, 32, 48][i], e = 6 + 16 * i;
  icoHead.writeUInt8(s, e); icoHead.writeUInt8(s, e + 1); icoHead.writeUInt16LE(1, e + 4); icoHead.writeUInt16LE(32, e + 6);
  icoHead.writeUInt32LE(png.length, e + 8); icoHead.writeUInt32LE(icoOffset, e + 12);
  icoOffset += png.length;
});
fs.writeFileSync(path.join(OUT, 'logo', 'favicon.ico'), Buffer.concat([icoHead, ...icoPngs]));

fs.rmSync(TMP, { recursive: true, force: true });
await browser.close();
server.close();
console.log(`\n${assets.length + svgFiles.length + 2} archivos en ${path.relative(process.cwd(), OUT)}`);
if (!HAS_SATOSHI) console.log(`⚠ Sin Satoshi en fonts/satoshi/: las piezas marcadas ◌ (sufijo ${PREVIEW_SUFFIX}) son solo vista previa.`);
