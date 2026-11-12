# Consola v1 — fabricación económica

Decidido con Juan el 2026-11-12, tras cotizar CNC + nylon en ~90 USD cada uno: **chapa por corte láser local** y
**cuerpo en PETG en 4 piezas en la Ender 3 V3** (filamento propio). Todo sale de `build_console.py`.

## Chapa de aluminio 2 mm — corte láser

| Archivo | Para qué |
|---|---|
| `cupon-prueba-laser.dxf` | **Cortar primero** (120 × 60). Tecla en 14,0 / 14,6 / 15,0 / 15,4 / 16,0; EC11 en 7,1 y 7,4; PEM en 2,5 / 2,6 / 4,2 / 4,3; tramo de ranura de fader |
| `panel-aluminio-v1-laser.dxf` | Perfil completo, solo cortes pasantes (116 contornos) |
| `panel-aluminio-v1.step` | Para cotizar o revisar en 3D |

- **Sin rebajes de Choc:** el rebaje 16 × 16 × 0,8 por debajo obligaba a fresar por la otra cara y era lo que encarecía el
  CNC. Con la hoja de la PG1353 (`28` §3) la chapa hace de placa de montaje: probar primero **14,0** en el cupón con
  una tecla montada (las aletas se comprimen y la tecla queda firme sobre el zócalo); 14,6 si queda forzada; 15,4 solo
  si nada sirve. El modelo usa hoy 15,4: cambiar `CHOC_CUT_LASER` cuando se elija.
- **Agujeros de PEM** (37): FH-M2.5 → 2,5 (PCB-K); BSO-M2.5 / BSO-M3 → 4,2 (PCB-P, PCB-F y las 9 torres). Se prensan a
  mano en tornillo de banco con yunque plano. Verificar diámetros con la hoja del proveedor y en el cupón.
- Anodizado y rótulos aparte (grabado láser o vinilo); el corte no los incluye.

## Cuerpo PETG — 4 piezas para la Ender 3 V3 (cama 220 × 220)

| Pieza | Medidas (mm) | STL |
|---|---|---|
| FI frente izquierda | 160 × 165 × 83 | `cuerpo-petg-v1-fi.stl` |
| FD frente derecha | 180 × 165 × 83 | `cuerpo-petg-v1-fd.stl` |
| TI trasera izquierda (bahía de la Pi) | 160 × 175 × 83 | `cuerpo-petg-v1-ti.stl` |
| TD trasera derecha (PCB-M y jacks) | 180 × 175 × 83 | `cuerpo-petg-v1-td.stl` |

- Juntas en x = 160 (pasa entre LINE IN L y R) y en y = 165 (libre de la torre de 332, 178).
- **Unión:** 15 topes interiores (piso, frente, laterales y trasera). Cada uno lleva un M3 × 16 a un inserto térmico
  Ø4 × 6; la cabeza queda del lado con acceso para la llave. Desde fuera solo se ve la línea de la junta. La chapa,
  atornillada a las 9 torres, termina de amarrar las cuatro piezas.
- Imprimir con el piso sobre la cama, sin soportes: los agujeros horizontales (jacks Ø9,5/Ø6,2, pernos Ø3,4) puentean.
  Pared 3,2 = 8 perímetros con boquilla 0,4. Volumen 771 cm³: 0,98 kg de PETG si fuera macizo; con relleno del 20 % queda por debajo (un rollo de 1 kg alcanza justo, mejor tener otro).
- Herrajes: 15 M3 × 16 + 15 insertos M3 Ø4 × 6 (juntas), 9 M3 × 10 (chapa), 6 insertos M3 (torres de PCB-M).

## Cambios frente al diseño v5

- Torre trasera de (134, 333) a (134, 334): chocaba 0,12 mm con el módulo de pantalla.
- La variante CNC (rebajes de Choc) sigue disponible con `make_panel(..., laser=False)`.
