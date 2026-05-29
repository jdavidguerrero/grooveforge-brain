# Skill: /fusion360-script

Genera o corrige un script Python para la Fusion 360 API, partiendo siempre del
API reference y los master parameters del proyecto.

---

## Cuándo invocar

Cuando el usuario pide:
- Crear un script nuevo para una pieza del enclosure
- Corregir un script existente que produce geometría incorrecta
- Agregar features a un script ya existente (cutouts, standoffs, bosses)
- Entender por qué un script no produce el resultado esperado

---

## Protocolo obligatorio antes de escribir código

### Paso 1 — Cargar fuentes autoritativas

Siempre leer estos dos archivos antes de escribir una línea:

```
apps/design/enclosure/parts/fusion360-api-reference.md
apps/design/enclosure/parts/00-master-parameters.md
```

Si el usuario menciona una pieza específica, leer también su spec:

| Pieza | Spec |
|-------|------|
| PETG body | `apps/design/enclosure/parts/01-petg-body.md` |
| Cheek izquierdo | `apps/design/enclosure/parts/03-cheek-left-guadua.md` |
| Cheek derecho | `apps/design/enclosure/parts/04-cheek-right-guadua.md` |
| Top panel | `apps/design/enclosure/parts/02-top-panel-aluminum.md` |

### Paso 2 — Identificar features requeridas

Listar explícitamente cada feature antes de codificarla:
- ¿Qué plano de sketch usa?
- ¿Qué coordenadas locales corresponden a las posiciones globales deseadas?
- ¿Es una operación NewBody, Join o Cut?

### Paso 3 — Escribir el script

Usar el template del agente `fusion360-script-writer`. Incluir:
- `print(f'StepN profiles={sk.profiles.count}')` después de cada sketch
- Verificación `healthState` en cada feature
- Comentarios con el valor en mm para cada constante en cm

### Paso 4 — Sincronizar si aplica

Si el script es para el PETG body, después de escribir `01-petg-body-build.py`:
```bash
cp apps/design/enclosure/parts/01-petg-body-build.py \
   apps/design/enclosure/parts/bodyPETG/bodyPETG.py
```

---

## Reglas de coordenadas (resumen ejecutivo)

```
xZConstructionPlane:  P.create(globalX_cm, globalZ_cm, 0)
yZConstructionPlane:  P.create(globalY_cm, globalZ_cm, 0)
xYConstructionPlane:  P.create(globalX_cm, globalY_cm, 0)

NUNCA: P.create(x, y, z) con z ≠ 0 en un sketch
```

---

## Output esperado

El agente entrega:
1. El script `.py` completo y funcional
2. Lista de prints de validación y sus valores esperados
3. Instrucciones de cómo ejecutarlo en Fusion 360
4. Comando de sincronización a `bodyPETG/` si aplica

---

## Invocar el agente especializado

Esta skill delega al agente `fusion360-script-writer` que tiene todas las reglas
del API internalizadas. El agente SIEMPRE empieza leyendo el API reference doc.
