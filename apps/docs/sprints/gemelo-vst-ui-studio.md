# Gemelo VST: adaptación visual Studio

La referencia de diseño es `GrooveForge VST.dc.html` del paquete entregado por el usuario (1 de noviembre de 2026). Esta primera adaptación conserva las páginas y los parámetros existentes: aplica los colores, jerarquía, componentes y ventana base de 1200 × 760 del diseño al editor JUCE. El cian de HYBRID es la propuesta de la referencia; no modifica los contratos DSP ni de presets.

Los controles dibujados por JUCE mantienen su vinculación a los parámetros existentes. La geometría del editor anterior se escala uniformemente para no recortar ni superponer páginas mientras se migra a la distribución completa; las secciones, navegador, medidores y teclado que aparecen en los mockups no se presentan como funcionales hasta implementarse. Se verifica compilación, pruebas funcionales existentes y cambios de tamaño; la equivalencia visual y pruebas A/B con hardware se validan posteriormente.

La barra de navegación permite abrir directamente cada página disponible del motor, incluidos los controles independientes de las capas A y B en HYBRID. Las pestañas representan las páginas implementadas, conservando la navegación anterior para compatibilidad.

## HOME de HYBRID

La pantalla organiza el banco de capas y las tarjetas de A y B alrededor de MORPH. Cada tarjeta muestra el motor real, permite escoger el foco MIDI y cargar un GFP; las ganancias se editan en su tarjeta. El bloque central conserva delay y transposición de B con los rangos del motor existente. El color indica el foco, acompañado del texto FOCUS; no se dibujan formas de onda ni medidores simulados.

## Filtros y envolventes de HYBRID

La página compara los controles disponibles de A y B en dos paneles paralelos. El borde y el texto FOCUS identifican el destino MIDI sin ocultar la otra capa. CUTOFF, RESONANCE, ATTACK y RELEASE siguen asociados a los parámetros independientes de cada mitad; elegir el foco no altera sus valores. No se dibuja una curva ADSR ni respuesta de filtro estimada porque estos cuatro controles no describen por completo su comportamiento.

## Cadena SYNTH FX

La página FX presenta las cinco unidades implementadas en filas: DRIVE, TAPE, CHORUS, DELAY y REVERB. Cada control muestra su nombre concreto (por ejemplo, FEEDBACK o DECAY) en lugar de exigir que se consulte P1/P2/P3 mediante un tooltip. Los controles siguen conectados a los parámetros del motor seleccionado; en HYBRID controlan la cadena compartida de la capa A. La vista no presupone una unidad adicional ni muestra estados de efectos que no existan en el DSP.

## GROOVE

PUMP, ARP y CHORD se muestran en paneles separados; el estado ON/OFF se obtiene del parámetro de cada sección y los demás controles permanecen editables. El layout usa la misma geometría para DX7 y los demás motores; no se presentan como implementadas las columnas adicionales del mockup que requieren otra disposición de controles. En HYBRID la página corresponde a la cadena compartida de la capa A.

## Medidor de salida

El medidor estéreo toma el pico absoluto del bloque final, sin cambiar las muestras. Cada canal acumula el máximo mediante atómicos y la interfaz lo consume a 30 Hz; la caída visual es independiente del audio. La indicación roja se mantiene al detectar amplitud ≥ 1.0 (0 dBFS) y se borra al hacer clic; señala que la muestra alcanzó el límite nominal, no que se haya medido clipping analógico. Es un medidor de picos de muestra, no de true peak ni LUFS.

## Teclado de pantalla

El teclado desplegable cubre dos octavas y emite notas al motor activo. Al abrirse amplía el editor para no ocultar controles; al cerrarlo o destruir el editor libera sus notas. Los cambios del teclado cruzan al hilo de audio mediante atómicos y se aplican al inicio del siguiente bloque; no se reservan buffers MIDI ni se bloquea el hilo de audio. El teclado de pantalla no pretende reflejar notas provenientes del host.

## Selector de presets

El selector de la cabecera enumera los presets de fábrica disponibles para el motor seleccionado y carga su GFP con el mismo camino de importación existente. En HYBRID ofrece las capas del banco del proyecto y las carga mediante su ruta de capas. Seleccionar una entrada cambia inmediatamente el sonido; el selector no finge ofrecer etiquetas, favoritos ni preescucha que aún no existan.

## Navegador de presets

El botón BROWSE abre un panel sobre los controles con una lista filtrable por nombre. Los resultados mantienen su índice original en el banco para que buscar no cambie qué preset se carga. El contenido usa los mismos presets de fábrica y capas de proyecto que el selector; al elegir una fila se carga el sonido y se cierra el panel. El estado de favoritos, etiquetas, descarte y preescucha aún no tiene almacenamiento ni contrato en el plugin, por lo que no se representa como si existiera.

## Favoritos del navegador

La estrella de cada fila marca o desmarca el sonido sin cargarlo. El filtro FAVORITOS se combina con la búsqueda por nombre. Las marcas se guardan dentro del estado del proyecto del DAW, por motor y posición del banco de fábrica o de capas; restaurar el proyecto restaura las marcas. No se escriben ni modifican los GFP/GFL. El banco de capas solo agrega entradas, por lo que su índice permanece estable dentro del proyecto.

## Etiquetas de presets

El símbolo # de cada fila abre la edición de etiquetas para esa entrada. Las etiquetas son texto separado por comas y se guardan con el proyecto, por motor y posición, sin modificar el preset. La búsqueda coincide con el nombre o el texto de etiquetas y se combina con el filtro de favoritos. Aplicar una etiqueta no carga el sonido; la selección del nombre continúa cargándolo.

## Envolventes editables de Juno

La página ENV muestra AMP y FILTER con una representación esquemática ADSR calculada a partir de los parámetros efectivos editables del plugin. Arrastrar el primer punto cambia ATTACK; el segundo cambia DECAY y SUSTAIN; el tercero cambia RELEASE. Se usa una escala temporal compartida que crece con la envolvente y un segmento de sustain fijo solo para representar la duración de la nota en el diagrama. La línea es una guía de parámetros, no una medición de la curva exponencial ni del audio. Los cambios se notifican al DAW con gestos de automatización y conservan los controles numéricos existentes.

## Envolventes de OB-6 y Model D

La página ENV/LFO de cada motor ofrece gráficos editables de amplitud y filtro con una escala temporal compartida entre ambas gráficas. Los puntos editan los parámetros propios del motor y la envolvente de amplitud usa los controles comunes de ataque y release ya conectados al motor. Los gráficos representan los valores base del editor, no el resultado instantáneo de modulación, macros o la curva medida del DSP.

## Envolventes de ARP 2600 y DX7

ARP 2600 presenta diagramas editables para amplitud y filtro en ENV/LFO, enlazados a los controles del motor. La página VOICE de DX7 muestra la envolvente ADSR global de amplitud; no debe confundirse con las envolventes individuales de los cuatro operadores, que siguen editándose en OPERATORS. Los diagramas muestran valores base y no modelan la respuesta DSP instantánea ni las macros.

## Envolventes de capas HYBRID

Las páginas A VOICE y B VOICE muestran diagramas editables conectados a los parámetros de la mitad correspondiente. El diagrama de filtro se oculta cuando esa mitad usa DX7, cuyo motor no utiliza esa envolvente de filtro. Las curvas representan valores base y no incluyen el efecto instantáneo de macros o modulación. Las escalas de amplitud y filtro se comparten dentro de cada mitad.

## Visualización LFO de Juno

La página LFO muestra dos ciclos calculados con el mismo `dsp::Lfo` custom del firmware (`apps/docs/theory/lfo-modulacion.md`), usando los cinco tipos que el engine admite. El gráfico es una vista representativa: la fase y el valor S&H no se toman del LFO que está sonando. En SYNC se indica DAW sin inventar una frecuencia, y en modo libre se muestra RATE. Un clic en el gráfico cambia SHAPE mediante el parámetro automatizable; los demás controles permanecen disponibles.

## LFO de OB-6, Model D y ARP 2600

Cada motor dispone de una pestaña LFO con forma representativa, velocidad, división y destinos ya conectados. El gráfico usa el LFO custom compartido y permite cambiar su forma con un clic. La previsualización no representa la fase ni el valor aleatorio que está sonando; la indicación SYNC depende del parámetro global y el tempo efectivo procede del DAW. Los controles siguen disponibles también en ENV/LFO.

## LFO de DX7 y HYBRID

DX7 dispone de una pestaña LFO con forma, velocidad, división y destinos pitch/amp. HYBRID dispone de pestañas A LFO y B LFO con parámetros independientes y destinos de cada mitad; el tercer destino corresponde a AMP en DX7 y PWM en los motores que usan ese destino. El gráfico usa `dsp::Lfo` como representación de la forma y no de su fase en ejecución. Las asignaciones y automatizaciones se mantienen por capa.

## Preescucha aislada

El navegador permite pulsar PREV en una fila para escuchar el sonido con nuevas notas MIDI; CANCELAR o cerrar el navegador vuelve al sonido del proyecto. Un segundo procesador, creado y preparado fuera del callback al iniciar la primera preescucha, recibe el preset seleccionado y procesa las mismas notas, mientras el procesador principal sigue ejecutándose y conserva su estado y automatización. Se sustituye solo la salida durante la preescucha. El procesador auxiliar permanece vivo hasta cerrar la instancia para evitar destruirlo en el hilo de audio. Esta primera interacción es explícita y no se activa por hover; puede aumentar el uso de CPU y memoria mientras está activa. La conmutación no usa crossfade y no hay equivalencia de fase garantizada.

## Preescucha opcional por hover

La casilla PREVIEW HOVER activa la preescucha aislada al mantener el cursor 300 ms sobre una fila. La opción arranca apagada; al salir de la lista, desactivarla o cerrar el navegador se cancela la preescucha. La estrella y el editor de etiquetas no disparan el hover. Si se selecciona la fila, el preset se carga normalmente. El coste y la ausencia de crossfade son los mismos de PREV.

## Transición de preescucha

Al activar o cancelar la preescucha se aplica una mezcla lineal de 10 ms entre la salida principal y la del procesador auxiliar. Ambas rutas siguen recibiendo MIDI durante el fundido. La memoria temporal estéreo se reserva en `prepareToPlay`; el callback no la amplía. Si el DAW entrega un bloque mayor al tamaño reservado, ese bloque usa conmutación directa para evitar asignaciones en el hilo de audio. Cambiar de preset mientras ya se está preescuchando no garantiza un fundido entre los dos presets auxiliares.
