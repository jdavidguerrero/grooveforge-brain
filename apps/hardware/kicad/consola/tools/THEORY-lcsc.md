# Cierre de suministro — 2026-11-12

Spec de esta corrección: encargo de Juan sobre base 751d949, compras verificadas por Claude
el 2026-11-12 y aprobadas por Juan; contexto doc24 §6 (JLC SMD en M, THT a mano),
D25 y REPORT-fixrev, «Límites físicos y compras pendientes».

Una compra resuelta tiene una identidad LCSC compatible con valor/huella, o un suministro
externo explícito. `Supply=owner` significa suministrado por Juan; `Supply=local`, compra
local. Ambos requieren `SupplyDescription` y coincidencia exacta de tipo, descripción,
valor y huella con `external_supplies` en lcsc_catalog.json. Se prohíbe mezclar LCSC y
suministro externo. Un campo vacío sigue siendo pendiente; un campo inválido es error.
DNP y rasgos de cobre mantienen sus exenciones previas. La BOM exporta ambos campos
para que el responsable del montaje pueda distinguir proveedor y pieza.

El catálogo registra C41376163 (socket hembra 1×24), C225519 (header macho 2×4) y
C7469972 (47 µF 16 V, 5×5,4 mm), con fuente «buscador LCSC» y fecha 2026-11-12,
según la verificación aportada en el encargo. Los dos IDC son compra local; las 26
referencias SW incluyen tecla PG1353 y zócalo CPG135001S30 (AliExpress); los ocho EC11
son de Juan. Declarar suministro no certifica stock reservado ni compatibilidad mecánica
más allá de lo aprobado; el cupón hot-swap continúa pendiente según REPORT-fixrev.

Polaridad: Device:C_Polarized tiene positivo en pin 1 y negativo en pin 2; la huella
KiCad 10 Capacitor_SMD:CP_Elec_5x5.4 usa pad 1 positivo, en el lado marcado «+».
Se verifica contra las librerías instaladas y contra la red: 1=+5V_CV, 2=GND.
No cambia el circuito ni la capacidad; la tensión nominal pasa de 10 V a 16 V.

Pruebas: aceptación de externos válidos y rechazo de tipo, descripción, huella, valor o
LCSC conflictivos; exportación real de Supply en BOM, cierre de las cuatro placas y
polaridad del condensador. ERC/netlist/harness verifican el contrato eléctrico existente.
No prueban layout/DRC, stock al pagar, inserción mecánica ni montaje físico. PCB-M SMD
va en la BOM de ensamble JLC; solo sus THT manuales van en el pedido suelto LCSC.
