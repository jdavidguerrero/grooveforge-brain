# Correcciones de revisión — teoría antes del esquema

Spec previo: doc24 D24–D37, aprobado por Juan 2026-11-12. Las resistencias serie limitan corriente entre dominios apagados; no convierten límites absolutos en un modo de operación garantizado. Se verifica topología y cálculo, luego banco de pérdida de cada USB/rail fantasma. Un pull-down define reposo solo en entradas no forzadas. Para pot10k usar Thévenin Rth=10k·x·(1−x), con serie1k y carga1M: Vadc/Vcc=x·1M/(1M+1k+Rth). Su error máximo es ≈0,217% FS (x≈0,70); media escala ≈0,174% FS. Cable abierto: τ=1M·100nF=100ms.

TVS después del fusible permite que un cortocircuito sostenido del supresor abra/limite F1; cableado corto a GND y fusible junto a entrada. P-MOS con source al consumidor permite arranque por diodo de cuerpo y bloquea polaridad inversa; comprobar pad G/S/D contra fabricante. TVS 5V standoff no garantiza clamp a 5,5V. F1 no reemplaza límite de corriente firmware LED.

Pruebas: contratos independientes para UART físico, dominios, resistencias, EN separados y aislamiento HP; exportación real KiCad para BOM sin '?' y catálogo para identidad/encapsulado. Inyectar errores en checker de compras debe fallar. ERC no prueba descarga ESD, arranque, cupones mecánicos, ganancia analógica ni cero CV durante reset. Referencias primarias y límites en doc24, catálogo LCSC y REPORT-fixrev.
