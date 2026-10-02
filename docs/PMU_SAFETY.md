# AXP2101: Seguridad Y Recuperacion

Reglas para tocar la PMU sin dejar la placa inservible, y que hacer si pasa. Escrito tras el incidente del 2026-10-01, que obligo a abrir la caja para desconectar la bateria.

La PMU alimenta toda la placa. Un error en ella puede dejar el ESP32-S3 sin corriente, y entonces no hay recuperacion por software: ni USB, ni modo descarga, ni `BOOT`. Por eso aqui la prudencia va por delante de la comodidad.

## Que Paso El 2026-10-01

1. Una prueba de consumo automatica (BleLab, en `ota_4`) apagaba `ALDO1`/`ALDO2`/`ALDO3` (reg `0x90`) e hibernaba el tactil (FT3168 `0xA5=3`) con la pantalla apagada, y al arrancar volvia a encender los tres rails (`ptest_restore_rails()`, antes del init del panel).
2. Tras un ciclo de esos, el arranque se quedo colgado en el init del panel. El USB desaparecio y la placa dejo de responder: PWR 2/10/20 s, `BOOT` + USB y modo descarga, nada.
3. El `otadata` apuntaba a `ota_4`, asi que cada arranque volvia a ejecutar la prueba. No habia forma de llegar al Launcher.
4. Unica salida: abrir la caja y desconectar la bateria. Despues se borraron `otadata` (`0xf000`, `0x2000`) y todo `ota_4`.
5. Ya arrancando el Launcher, PWR no hacia nada, la bateria salia siempre como "USB" y el reloj se dormia enchufado (perdiendo el USB). El AXP2101 respondia en `0x34` pero ensenaba otra pagina de registros (`0xFF=0x01`, ver mas abajo).
6. Arreglo: una sola escritura `0xFF=0x00`. Todo volvio a los valores de antes del incidente.

Lo que no se sabe: que puso `0xFF` a 1. Ningun codigo nuestro, ni el BSP, escribe ese registro. Ocurrio en algun punto entre los arranques fallidos y la reconexion de la bateria.

## Lo Que No Hay Que Hacer

- **No escribir registros de alimentacion o proteccion del AXP2101** sin explicar antes el riesgo y tener un si explicito del usuario. En concreto:
  - `0x80` (DCDC on/off; `DCDC1` es `VCC3V3`, todo el sistema), `0x82..0x85` (voltajes DCDC).
  - `0x90`/`0x91` (LDO on/off) y `0x92..0x9A` (voltajes LDO). **Nunca cortar `ALDO2`**: es el enable del panel (`DSI_PWR_EN`) y el panel sigue alimentado por `VCC3V3`.
  - `0x10` (bit 0 apaga, bit 1 reinicia la PMU, bit 2 activa el apagado por PWR de 16 s).
  - `0x12` (BATFET), `0x22`/`0x23`/`0x24` (protecciones: con `0x23=0x3F` la PMU apaga toda la placa si un DCDC cae un 15 %), `0x25`..`0x2B` (secuencias de encendido y apagado), `0x26` (sleep/wakeup).
  - `0x61..0x69` (carga), `0x50` (TS), `0xA1`/`0xA2` (memoria de bateria del fuel gauge).
  - `0xFF` (pagina de registros, no documentado). Solo para volver a `0x00`, ver abajo.
- **No meter escrituras de PMU en pruebas automaticas**, ni en codigo que se ejecute en cada arranque. Un fallo se repite en bucle y no deja llegar al Launcher.
- **No dejar `otadata` apuntando a un firmware de pruebas** que toque hardware. Si se cuelga, la placa arranca siempre en el. Las apps de prueba deben volver solas al Launcher (`watch_launcher_exit()`), y al acabar se borra o se deja inofensivo su slot.
- **No hibernar el tactil ni cortar rails "para medir consumo".** Las pruebas de consumo solo varian software: sleep, BLE, frecuencia de sondeo, brillo.
- **No usar escrituras a ciegas.** Leer antes, escribir solo si el valor leido es el esperado, y comprobar despues.
- **No asumir que reflashear arregla la PMU.** La configuracion del AXP2101 vive en el chip (se carga de su memoria interna al encenderse) y en sus registros mientras tenga corriente. Ningun firmware la guarda en flash.
- **No desconectar la bateria como primer recurso.** Es la ultima opcion (hay que abrir la caja). Antes, agotar las rutas de abajo.

## Lo Que Si Escribe Core

Son las unicas escrituras de PMU en uso (desde core v0.4.0, 2026-09-30; sin problemas desde entonces):

| Registro | Escritura | Donde |
| --- | --- | --- |
| `0x30` | bit 0 (ADC de VBAT) | `watch_battery_init()` |
| `0x68` | bit 0 (deteccion de bateria) | `watch_battery_init()` |
| `0x41` | bit 3 (IRQ de PWR corto) | `watch_buttons` |
| `0x49` | `0x08` (limpiar IRQ de PWR) | `watch_buttons` |
| `0x10` | bit 0 (apagado por software) | `watch_power` |

Cualquier escritura nueva entra en esta tabla solo despues de validarla en la placa con el usuario al lado.

## Como Proponer Una Escritura Arriesgada

Si de verdad hace falta escribir un registro de la lista de arriba:

1. Diagnosticar antes solo leyendo (ver "Diagnostico"). Tener los datos que justifican la escritura.
2. Explicar al usuario que se va a escribir, por que, que puede salir mal y como se recupera. Esperar un si explicito.
3. Firmware minimo, en un slot de pruebas, que:
   - lea el estado y solo escriba si es exactamente el esperado;
   - haga una sola escritura;
   - lea y compruebe el resultado, y si no es el esperado, deshaga la escritura (vuelva al valor leido);
   - sea idempotente: si se vuelve a ejecutar (abrir el puerto serie reinicia el chip), no haga nada;
   - vuelva solo al Launcher al cabo de unos minutos.
4. Nunca dejar ese firmware como arranque. Al acabar, apuntar el resultado aqui.

## Diagnostico (Solo Lectura)

Leer siempre primero. Un firmware de solo lectura no puede empeorar nada. Leer registro a registro (`i2c_master_transmit_receive` de 1 byte, igual que `watch_pmu_read()`), a 100 kHz.

1. `i2c_master_probe` de `0x08..0x77`. En la placa sana responden `18` (ES8311), `34` (AXP2101), `40` (ES7210), `51` (PCF85063), `6b` (QMI8658) y, a ratos, `38` (FT3168, desaparece cuando duerme).
2. `0x03` (IC type). **Debe ser `0x4A`.** Si no, mirar `0xFF`.
3. `0xFF`. Debe ser `0x00`.
4. Volcado completo `0x00..0xFF` y comparar con la referencia.

Volcado de referencia de la placa sana (2026-10-01, con USB y bateria; `0x00`/`0x01`, los ADC `0x34..0x3D`, las IRQ `0x48..0x4A` y el porcentaje `0xA4` cambian con el estado):

```
00: 38 33 00 4a 00 00 00 00 04 00 00 00 00 00 00 00
10: 34 00 08 03 65 06 04 00 0a 06 a1 00 00 00 09 00
20: 01 01 06 3f 00 18 08 14 00 00 00 00 00 00 00 00
30: 03 00 00 00 10 0f bf ed 00 00 00 00 00 00 00 00
40: ff fc 5f 00 00 00 00 00 10 ab 68 00 00 00 00 00
50: 12 00 02 01 29 58 3e 4c 00 14 37 1e 02 58 00 00
60: 02 05 08 15 03 02 01 e6 01 01 03 00 00 00 00 00
80: 0f 00 12 28 46 64 00 00 00 00 00 00 00 00 00 00
90: ff 01 1c 1c 19 0d 07 17 0e 00 00 00 00 00 00 00
a0: 00 79 00 ea 64 00 00 53 00 00 00 03 00 00 00 00
(70 y b0..ff: todo 00)
```

Lo esencial: `0x03=4a`, `0x10=34`, `0x30=03`, `0x41=fc`, `0x68=01`, `0x80=0f`, `0x90=ff`, `0x91=01`, `0xFF=00`.

## Recuperacion Por Sintoma

### Firmware colgado o sin USB, pero la placa tiene corriente

1. Mantener `BOOT` y reconectar el USB (o reiniciar) para entrar en modo descarga.
2. Si el firmware malo esta en un slot OTA, borrar `otadata` (`0xf000`, tamano `0x2000`): el bootloader arranca `factory` (Launcher).
3. Si el reloj se duerme y macOS pierde el puerto, encender la pantalla, pulsar `BOOT` y reconectar el USB mientras un script espera al puerto (ver "Herramientas").

### La placa no enciende de ninguna forma

1. Mantener PWR al menos 6-8 s, soltar y pulsar PWR otra vez (FAQ de Waveshare).
2. Conectar el USB y esperar unos minutos por si la bateria estaba agotada. Probar otro cable o cargador.
3. Mantener `BOOT` y conectar el USB para el modo descarga.
4. Si nada de eso funciona, la PMU ha cortado la alimentacion y no hay ruta por software: abrir la caja y desconectar la bateria (conector MX1.25) con el USB quitado, esperar unos segundos y volver a conectarla. Con eso la PMU vuelve a sus valores de encendido.
5. Al volver a arrancar, mirar si `otadata` apunta a un firmware de pruebas y borrarlo antes de que se repita el fallo. Despues, hacer el diagnostico de arriba.

### La PMU responde pero los datos no cuadran (PWR muerto, bateria "USB")

Sintomas del 2026-10-01: PWR no hace nada, la bateria sale siempre como "USB" (sin porcentaje, tambien desenchufado) y el Launcher se duerme con el USB conectado.

Lectura: `0xFF=0x01`, `0x03=0x28`, `0x00=20 01=a8 10=64 12=9c`, y todo `0x20..0xFE` a `00`. Con la pantalla funcionando, `0x90=00` es imposible en la pagina normal (`ALDO2` alimenta el panel): no son los registros de verdad. En `0x0E..0x10` aparecen `28 46 64`, los voltajes de DCDC por defecto que en la pagina normal estan en `0x83..0x85`. Es una pagina interna de configuracion de fabrica.

Las escrituras del Launcher en esa pagina no se quedan (siguen leyendo `00`), asi que no estropean nada.

Arreglo (aprobado por el usuario y aplicado el 2026-10-01): escribir `0xFF=0x00` y comprobar que `0x03` vuelve a `0x4A`. Si no, volver a escribir `0xFF=0x01` para dejarlo como estaba. Cambiar de pagina no toca la alimentacion, solo que registros se ven. Resultado: todos los registros identicos al volcado de antes del incidente.

```c
// Solo si 0xFF=01 y 0x03 != 0x4A. Una escritura, comprobacion y vuelta atras.
uint8_t page, id;
if (rd(0xFF, &page) && rd(0x03, &id) && page == 0x01 && id != 0x4A) {
    wr(0xFF, 0x00);
    vTaskDelay(pdMS_TO_TICKS(20));
    if (!(rd(0x03, &id) && id == 0x4A)) wr(0xFF, 0x01); // deshacer
}
```

## Herramientas

Las pruebas van en un slot libre (`ota_4`, `0x9a0000`, `0x200000`) sin tocar el Launcher ni `factory`:

```sh
source "$HOME/.espressif/tools/activate_idf_v5.5.4.sh"   # en este Mac export.sh no encuentra el venv
P=$(ls /dev/tty.usbmodem* | head -1)
# Escribir solo el slot de pruebas (no reinicia):
python -m esptool --chip esp32s3 -p $P -b 921600 --after no_reset write_flash \
  --flash_mode dio --flash_freq 80m --flash_size 32MB 0x9a0000 build/<app>.bin
# Arrancar ese slot la proxima vez:
python $IDF_PATH/components/app_update/otatool.py -p $P --esptool-args after=no_reset \
  --partition-table-file partitions.csv switch_ota_partition --slot 4
# Abrir el puerto reinicia el chip y arranca el slot: capturar ahi la salida.
```

- Abrir el puerto serie reinicia el chip, por eso los firmwares de diagnostico tienen que ser idempotentes.
- Con el chip en light sleep el USB-JTAG desaparece. El firmware de diagnostico debe quedarse despierto (no usar `watch_power`).
- La app vuelve sola al Launcher con `watch_launcher_exit()`, que deja de nuevo el arranque en `factory`.

Relacionado: [GOTCHAS](GOTCHAS.md#no-cortar-los-rails-del-axp2101-del-panel), [HARDWARE](HARDWARE.md#pmu-axp2101).
