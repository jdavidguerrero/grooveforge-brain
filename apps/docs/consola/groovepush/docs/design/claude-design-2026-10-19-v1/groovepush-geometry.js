// GroovePush · geometría compartida (vista superior, corte, panel trasero, 3D y reglas)
// Coordenadas del panel: x desde el borde izquierdo, y desde el borde frontal, sobre la chapa (no en planta).
const f1 = v => (Math.round(v * 10) / 10).toFixed(1);
const rectD = (a, b) => Math.hypot(Math.max(a.x0 - b.x1, b.x0 - a.x1, 0), Math.max(a.y0 - b.y1, b.y0 - a.y1, 0));
const circR = (c, b) => Math.hypot(Math.max(b.x0 - c.x, 0, c.x - b.x1), Math.max(b.y0 - c.y, 0, c.y - b.y1)) - c.r;
const circC = (a, b) => Math.hypot(a.x - b.x, a.y - b.y) - a.r - b.r;
const dist = (a, b) => a.r && b.r ? circC(a, b) : a.r ? circR(a, b) : b.r ? circR(b, a) : rectD(a, b);
const R = (x, y, w, h) => ({ x, y, x0: x - w / 2, x1: x + w / 2, y0: y - h / 2, y1: y + h / 2, w, h });

export const MMPX = 155 / 1280;
export const CELLS = { uniforme: Array(8).fill(160), redistribuida: [100, 180, 180, 180, 180, 180, 180, 100] };

export function build(o = {}) {
  const knob = +(o.knob ?? 18), gui = CELLS[o.gui] ? o.gui : 'redistribuida', ang = +(o.angle ?? 12);
  const th = ang * Math.PI / 180, r = knob / 2, pitch = knob + 8;
  const ext = Math.max(3.5 * pitch + r, 105.5);
  const W = Math.round(2 * (ext + 12.5)), cx = W / 2;

  const pads = [], padRows = [44.5, 59.5, 74.5, 89.5];
  padRows.forEach((y, j) => { for (let i = 0; i < 8; i++) pads.push({ ...R(cx - 52.5 + i * 15, y, 12, 12), i, j }); });
  const padTop = 95.5;
  const encY = padTop + 6.5 + r;
  const encs = [...Array(8)].map((_, i) => ({ n: 'E' + (i + 1), x: cx + (i - 3.5) * pitch, y: encY, r }));
  const act = { x0: cx - 77.5, x1: cx + 77.5, y0: encY + r + 15 };
  act.y1 = act.y0 + 87.2; act.cy = (act.y0 + act.y1) / 2;
  const win = { x0: act.x0 - 1.5, x1: act.x1 + 1.5, y0: act.y0 - 1.5, y1: act.y1 + 1.5 };
  const mod = { x0: cx - 85, x1: cx + 85, y0: act.cy - 53, y1: act.cy + 53 };
  const D = Math.round(act.y1 + 17.8);

  const keys = [];
  const K = (n, g, x, y) => keys.push({ n, g, ...R(x, y, 18, 17) });
  const mb = encY + r + 6.5 + 8.5;
  ['BROWSE', 'MIX', 'DEVICE', 'NOTE', 'SESSION'].forEach((n, i) => K(n, 'Modos', cx - 93, mb + i * 18.5));
  [['DELETE', 48], ['DUPLICATE', 66.5], ['QUANTIZE', 85]].forEach(([n, y]) => K(n, 'Edición', cx - 73.5, y));
  [['NEW', 57.25], ['UNDO', 75.75]].forEach(([n, y]) => K(n, 'Edición', cx - 93, y));
  K('SHIFT', 'Modificador', cx - 93, 24);
  ['PLAY', 'STOP', 'RECORD', 'LOOP', 'METRO'].forEach((n, i) => K(n, 'Transporte', cx - 67.5 + i * 19.5, 24));
  ['BANK ◀', 'BANK ▶', 'OCT ▼', 'OCT ▲'].forEach((n, i) => K(n, 'Navegación', cx + 36 + i * 19.5, 24));

  const faders = [0, 1, 2, 3].map(i => {
    const x = cx + 68.5 + i * 11;
    return { n: 'F' + (i + 1), x, y: 67, ...R(x, 67, 8, 59), cap: R(x, 67, 8, 14), slot: R(x, 67, 3, 48) };
  });

  // GUI: celdas de soft-key
  const cell = CELLS[gui];
  const centers = sc => { let acc = 0; return CELLS[sc].map(w => { const c = acc + w / 2; acc += w; return c; }); };
  const align = encs.map((e, i) => {
    const pu = centers('uniforme')[i], pr = centers('redistribuida')[i];
    const du = e.x - (act.x0 + pu * MMPX), dr = e.x - (act.x0 + pr * MMPX);
    return { n: e.n, x: e.x, px: (e.x - act.x0) / MMPX, pu, du, pr, dr };
  });
  const offMax = s => Math.max(...align.map(a => Math.abs(s === 'uniforme' ? a.du : a.dr)));

  // corte: chapa inclinada. (y, n) → planta (X, Z). n = profundidad bajo la cara superior.
  const FEET = 6, HF = 28, FLOOR = FEET + 3;
  const sin = Math.sin(th), cos = Math.cos(th);
  const toP = (y, n) => [y * cos + n * sin, HF + y * sin - n * cos];
  const Pb = D * cos, Hb = HF + D * sin;
  const pi = { y1: mod.y1 - 0.6 }; pi.y0 = pi.y1 - 85; pi.x0 = cx - 78; pi.x1 = pi.x0 + 56;
  const STACK = { al: [0, 2], mod: [2, 8], std: [8, 12], pcb: [12, 13.6], cool: [13.6, 23], hat: [29.6, 31.2], hatParts: [31.2, 42.2] };
  const piLow = toP(pi.y0, STACK.hatParts[1])[1];
  const piClear = piLow - FLOOR;
  const carrier = { X0: Pb - 3 - 100, X1: Pb - 3, Z0: FLOOR + 2, Z1: FLOOR + 22, x0: cx + 8, x1: cx + 105 };

  // panel trasero (x del equipo, z desde la mesa)
  const conns = [
    { n: 'ETHERNET', s: 'RJ45 keystone', x: 30, z: 24, w: 18, h: 16, hole: [16, 14] },
    { n: 'USB-A PI', s: 'extensión de panel', x: 56, z: 24, w: 18, h: 11, hole: [14, 7] },
    { n: 'PWR PI', s: 'USB-C 5 V 5 A · 27 W', x: 80, z: 24, w: 14, h: 9, hole: [10, 4.5] },
    { n: 'microSD', s: 'extensor FPC', x: 106, z: 48, w: 14, h: 3, hole: [13, 2] },
    { n: 'IN (RESERVA)', s: 'Ø 6.2 · tapón', x: 106, z: 26, r: 6, hole: 3.1 },
    { n: 'OUT L', s: '1/4" TS · Ø 9.5', x: 132, z: 26, r: 10, hole: 4.75 },
    { n: 'OUT R', s: '1/4" TS · Ø 9.5', x: 158, z: 26, r: 10, hole: 4.75 },
    { n: 'USB TEENSY', s: 'USB-C datos', x: 186, z: 22, w: 14, h: 9, hole: [10, 4.5] },
    { n: 'PWR CTRL', s: 'USB-C 5 V 3 A', x: 210, z: 22, w: 14, h: 9, hole: [10, 4.5] }
  ].map(c => c.r ? { ...c, y: c.z } : { ...c, ...R(c.x, c.z, c.w, c.h), y: c.z });
  let connMin = 99, connPair = '';
  conns.forEach((a, i) => conns.slice(i + 1).forEach(b => { const d = dist(a, b); if (d < connMin) { connMin = d; connPair = a.n + ' ↔ ' + b.n; } }));
  const exhaust = { n: 12, w: 3, h: 24, x0: 20, pitch: 6, z0: 38 };
  const intake = { n: 14, w: 3, h: 30 };
  const exA = exhaust.n * exhaust.w * exhaust.h / 100, inA = intake.n * intake.w * intake.h / 100;

  // reglas
  let r2 = 99, r2w = '';
  encs.forEach(e => [...keys, ...pads].forEach(k => { const d = dist(e, k); if (d < r2) { r2 = d; r2w = e.n + ' ↔ ' + (k.n || 'pad'); } }));
  let r3 = 99, r3w = '';
  keys.forEach((a, i) => keys.slice(i + 1).forEach(b => { const d = rectD(a, b); if (d < r3) { r3 = d; r3w = a.n + ' ↔ ' + b.n; } }));
  const r4 = Math.min(...keys.map(k => k.y0), ...pads.map(p => p.y0), ...faders.map(f => f.y0), ...encs.map(e => e.y - e.r));
  const sight = (42 + ang) * Math.PI / 180;
  const g5 = act.y0 - (encY + r), req5 = 20 / Math.tan(sight), gp = act.y0 - padTop, reqp = 50 / Math.tan(sight);
  let r7 = 99, r7w = '';
  faders.forEach(f => [...keys, ...encs, ...pads].forEach(k => { const d = dist(f, k); if (d < r7) { r7 = d; r7w = f.n + ' ↔ ' + (k.n || 'pad'); } }));
  const off = offMax(gui);
  const P = (ok, cond) => cond ? 'COND' : ok ? 'PASA' : 'NO PASA';
  const rules = [
    { i: 1, t: 'Perilla–perilla ≥ 8 mm', v: f1(pitch - knob) + ' mm', s: P(pitch - knob >= 8 - 1e-6), m: `Paso ${pitch} con perilla Ø ${knob}.` },
    { i: 2, t: 'Perilla–keycap y perilla–pad ≥ 6 mm', v: f1(r2) + ' mm', s: P(r2 >= 6), m: `Mínimo en ${r2w}.` },
    { i: 3, t: 'Keycap–keycap ≥ 1.5 mm', v: f1(r3) + ' mm', s: P(r3 >= 1.5 - 1e-6), m: 'Paso 19.5 × 18.5 con keycap 18 × 17; grupos separados 6 mm extra.' },
    { i: 4, t: 'Ningún control a < 15 mm del frente', v: f1(r4) + ' mm', s: P(r4 >= 15), m: 'Fila frontal (SHIFT, transporte, navegación) en y 24.' },
    { i: 5, t: 'Manos no tapan la pantalla', v: `${f1(g5)} ≥ ${f1(req5)} · ${f1(gp)} ≥ ${f1(reqp)}`, s: P(g5 >= req5 && gp >= reqp), m: `Línea de vista a ${42 + ang}° del panel. Yema a 20 mm sobre la perilla; nudillos a 50 mm sobre los pads.` },
    { i: 6, t: 'Etiqueta ≤ 25 mm de su encoder', v: f1(off) + ' mm', s: P(off <= 25), m: `GUI ${gui} (${cell.join(' · ')} px). ${gui === 'uniforme' ? 'Redistribuida: ' + f1(offMax('redistribuida')) : 'Uniforme: ' + f1(offMax('uniforme'))} mm.` },
    { i: 7, t: 'Faders: 4 a la vez con 2 dedos, sin choques', v: f1(r7) + ' mm libres', s: P(r7 > 3), m: `Paso 11, cap 8: cada dedo cubre 2 caps (19 mm). Mínimo en ${r7w}.` },
    { i: 8, t: 'Cable DSI sin tensión ni filo', v: '~ 60 mm, R ≥ 5', s: P(true, true), m: 'Pi atornillada al módulo: recorrido corto con un pliegue en S. Depende del largo del FPC y de dónde salga el conector del módulo.' },
    { i: 9, t: 'Ventilación Pi 5', v: `entra ${f1(inA)} · sale ${f1(exA)} cm²`, s: P(inA >= 8 && exA >= 8), m: `Piso bajo la Pi → hueco de ${f1(STACK.hat[0] - STACK.cool[1])} mm entre cooler y HAT → ranuras traseras. Holgura HAT–piso ${f1(piClear)} mm.` },
    { i: 10, t: 'Conectores accesibles con cables', v: f1(connMin) + ' mm', s: P(connMin >= 6), m: `Separación mínima entre envolventes de macho: ${connPair}.` },
    { i: 11, t: 'Huella frente al Push 3', v: `${W} × ${Math.round(Pb)} × ${Math.round(Hb)}`, s: P(W < 378 && Pb < 293), m: `${Math.round(100 * W * Pb / (378 * 293))} % del área del Push 3 (378 × 293).` },
    { i: 12, t: 'Pantalla táctil sin pasar sobre encoders', v: f1(g5) + ' mm al área activa', s: P(true, true), m: 'El dedo pasa sobre la fila de perillas (20 mm de alto). La franja de soft-keys se opera con los push de los encoders; los toques van de 24 mm hacia atrás.' },
    { i: 13, t: '20 teclas agrupadas, ubicables sin mirar', v: '5 grupos', s: P(true), m: 'Modos junto a la pantalla, edición junto a los pads, fila frontal SHIFT | transporte | navegación con huecos de 6 mm.' }
  ];

  return { knob, gui, ang, th, r, pitch, W, cx, D, pads, padTop, encY, encs, act, win, mod, keys, faders, cell, align, offMax,
    FEET, HF, FLOOR, toP, Pb, Hb, pi, STACK, piClear, carrier, conns, connMin, exhaust, intake, exA, inA, rules, f1 };
}
