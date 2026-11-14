# UART Connections — NeoTrellis M4 ↔ Teensy 4.1

Consola única, D24/doc18: **Serial7** en Teensy y **Serial1 predeterminado** del core
Adafruit `trellis_m4` en M4 (SERCOM4). Las etiquetas SDA/SCL del conector describen
su función alternativa I2C, no la dirección UART. No hay bit-banging ni remapeo SERCOM.

| NeoTrellis M4 | Función | Teensy 4.1 | Función |
|---|---|---|---|
| Pin21 SDA / CONN1.3 | TX1 | Pin28 RX7 / J3_M4.3 | RX |
| Pin22 SCL / CONN1.4 | RX1 | Pin29 TX7 / J3_M4.4 | TX |
| CONN1.1 GND | GND | J3_M4.1 GND | GND |
| CONN1.2 3V3 | NC | J3_M4.2 NC | NC |

M4 se alimenta por su USB; no unir su salida3V3 al rail Teensy. GND común.
Teensy0/1 son EN_K/EN_A: no conectar a ellos la M4 ni iniciar Serial1 Teensy en consola_uno.

```cpp
// M4: core trellis_m4 configura TX21=SDA y RX22=SCL.
Serial1.begin(UART_BAUD_RATE);  // 1000000, 8N1
// Teensy consola_uno: TX29 y RX28.
Serial7.begin(1000000);
```

Ambas direcciones usan BinaryProtocol: `[0xAA][CMD][LEN][PAYLOAD][XOR]`, 1Mbps, 8N1,
sin flow control. Handshake/PING y press/release/LED se conservan; las descripciones
antiguas de SysEx a2Mbps eran incorrectas para este enlace. Ver `shared/BinaryProtocol.h`.

El target histórico Brain carrier en `src/teensy` conserva Serial5 TX20/RX21 hacia M4 y
Serial1 hacia Pi; no es el cableado consola_uno y se solapa con I2S del shield de consola.
No cambia el Serial1 físico de M4. Build sin upload: `pio run -e consola_s_m4`.
Comprobar físicamente TX21→RX28 y TX29→RX22, handshake y press/release antes de cerrar
la puerta de banco; un build no certifica el cable ni la integridad de señal.
