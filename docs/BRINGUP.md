# Bring-Up Checklist

Checklist para validar la placa antes de construir apps grandes. Completar resultados reales aqui o en un log corto de hardware.

## Objetivo

Confirmar que el baseline `ESP-IDF 5.5.4 + LVGL 9.3.0 + Waveshare BSP` controla correctamente la placa real `ESP32-S3-Touch-AMOLED-2.06`.

## Preparacion

El core no es un proyecto ESP-IDF: el bring-up se hace desde `ESP32Watch-template` (UI `ESP32S3Watch`) o, sin display, desde `examples/basic` de este repo.

```sh
source "$HOME/.espressif/v5.5.4/esp-idf/export.sh"
cd ../ESP32Watch-template
idf.py set-target esp32s3
idf.py build
```

Flash/monitor esperado:

```sh
idf.py -p <PORT> flash monitor
```

Salir de monitor: `Ctrl+]`.

## Validacion Inicial

| Paso | Esperado | Resultado |
| --- | --- | --- |
| Build limpio | `idf.py build` termina OK. | Pendiente hardware. |
| Flash | Escribe bootloader/app/partition table sin errores. | Pendiente hardware. |
| Boot log | Sin panic/reboot loop. | Pendiente hardware. |
| Display | UI `ESP32S3Watch` visible. | Pendiente hardware. |
| Brillo | Brillo cambia con `bsp_display_brightness_set(80)`. | Pendiente hardware. |
| Touch | Widgets LVGL responden si se anade boton/gesture de prueba. | Pendiente hardware. |
| Auto-download USB | `idf.py flash` entra solo por Type-C sin pulsar `BOOT`. | Pendiente hardware. |
| Recovery | Mantener `BOOT` al alimentar permite volver a flashear si hay crash. | Pendiente hardware. |

## Flash Real

La wiki y el esquematico indican 32 MB (`GD25Q256EYIGR`, 256 Mbit), pero ejemplos oficiales usan 16 MB. El boot log de la placa real ya lo confirma (`spi_flash: Detected size(32768k) larger than the size in the binary image header(16384k)`). Para ver fabricante/dispositivo:

```sh
esptool.py -p <PORT> flash_id
```

Anotar aqui:

| Dato | Resultado |
| --- | --- |
| Manufacturer | Pendiente |
| Device | Pendiente |
| Detected flash size | 32 MB (boot log `spi_flash`) |

Los proyectos usan `CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y` con el codigo por debajo de 16 MB. Validado: lectura/escritura de datos por encima de 16 MB y FAT de solo lectura montado desde `0x1000000` (ver [GOTCHAS](GOTCHAS.md)).

## I2C Scan

Bus esperado: `SDA=GPIO15`, `SCL=GPIO14`, `400 kHz`, port `1` por BSP.

Direcciones esperadas:

| Dispositivo | Direccion esperada | Resultado |
| --- | --- | --- |
| FT3168 touch | `0x38` | OK (todas las apps) |
| QMI8658 IMU | `0x6B` preferida, `0x6A` posible | OK `0x6B` (Maze, Fluid) |
| PCF85063 RTC | `0x51` tipica | OK (`watch_rtc`) |
| AXP2101 PMU | `0x34` | OK (`watch_battery`, PWR) |
| ES8311 speaker codec | `0x30` | OK (el altavoz suena: Launcher, Doom) |
| ES7210 mic ADC | `0x40` 7-bit (`0x80` macro en `esp_codec_dev`) | OK (graba: Recorder) |

Si falta un dispositivo, comprobar alimentacion/PMU antes de asumir fallo del sensor.

Para hacer el scan desde una app que ya usa pantalla/touch, usar el handle de `bsp_i2c_get_handle()` y no crear otro bus sobre el mismo puerto.

## Display Y LVGL

Validar:

| Area | Esperado |
| --- | --- |
| Resolucion | `410 x 502`. |
| Orientacion | Portrait por BSP; rotacion solo si la app lo decide. |
| Color | RGB565 correcto, sin swap visual. |
| Refresco | Sin areas corruptas; el BSP ya redondea invalidate areas. |
| Offset | Origen correcto con gap BSP `0x16, 0`. |
| Locking | Toda modificacion desde tareas FreeRTOS usa `bsp_display_lock()`. |
| Brillo | 0 apaga/dim, 100 maximo; control por comando `0x51`, no PWM. |
| TE | `LCD_TE GPIO13` existe en esquematico, pero BSP v1.0.7 no lo usa directamente. |

## Touch

Validar con una pantalla simple que muestre coordenadas o cambie un boton LVGL.

| Area | Esperado |
| --- | --- |
| Init | `watch_display_start()` registra el input device (`bsp_touch_new()` + `lvgl_port_add_touch()`). |
| Coordenadas | Rango aproximado `0..409`, `0..501`. |
| Gestos | No bloquearlos con contenedores clickables innecesarios. |
| Reset/INT | `RST GPIO9`, `INT GPIO38` por BSP. |

## IMU QMI8658

Integracion recomendada cuando se pase de demo de pantalla:

```c
i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
qmi8658_dev_t dev;
ESP_ERROR_CHECK(qmi8658_init(&dev, bus, QMI8658_ADDRESS_HIGH));
ESP_ERROR_CHECK(qmi8658_set_accel_range(&dev, QMI8658_ACCEL_RANGE_8G));
ESP_ERROR_CHECK(qmi8658_set_accel_odr(&dev, QMI8658_ACCEL_ODR_500HZ));
```

Validar:

| Area | Esperado |
| --- | --- |
| Direccion | `QMI8658_ADDRESS_HIGH` (`0x6B`) funciona. |
| Unidades default | Accel en milli-g; `1000.0` aproximadamente 1 g. |
| Mapeo pantalla | `screen_x = -accelY / 1000.0f`, `screen_y = accelX / 1000.0f`. |
| Calibracion | 100-200 muestras quieto, aplicar mismo mapeo antes de promediar. |
| Deadzone | Empezar con `0.015f` a `0.05f` en g. |

Si se activa `qmi8658_set_accel_unit_mps2(&dev, true)`, no dividir por `1000.0f`.

## RTC PCF85063

Driver minimo en `watch_rtc` (core v0.3.0, validado en Fluid): lee y escribe hora/fecha en BCD, modo 24 h, y detecta la perdida de hora por el flag de oscilador parado.

| Area | Resultado |
| --- | --- |
| Direccion | `0x51`, confirmada. |
| Persistencia | Conserva la hora entre reinicios (Fluid). Tras apagados largos o sin bateria: sin validar. |
| Interrupcion | `RTC_INT GPIO39` segun esquematico; sin usar ni validar (alarmas pendientes). |
| API | `watch_rtc.h`. |

## PMU AXP2101

Wrapper propio desde core v0.4.0 (`watch_pmu_priv.h`, `watch_battery.h`, PWR en `watch_buttons.h`), sin XPowersLib. Antes de escribir cualquier registro, leer [PMU_SAFETY.md](PMU_SAFETY.md).

Validar:

| Area | Esperado |
| --- | --- |
| Direccion | `0x34`. |
| Datos | Temperatura chip, charging, VBUS, battery voltage, system voltage. |
| Porcentaje | Solo orientativo; preferir voltage/tendencia. |
| TS pin | Llamar equivalente a `disableTSPinMeasure()` si se usa XPowersLib. |
| PKEY | Usar para `PWR` si se decide integrar power key. |
| Rails | No copiar `01_AXP2101` sin revisar mapa de rails en `docs/HARDWARE.md`. |

Politica de sleep: `watch_power` (core v0.4.0), validada abajo en "Energia Y Sleep".

## Energia Y Sleep (validado 2026-09-30)

Prueba aislada en la placa (firmware de prueba fuera de los repos), con USB y en bateria:

| Area | Resultado |
| --- | --- |
| Panel sleep | `0x28` + `0x10` (off + sleep in) y `0x11` + 120 ms + `0x29` (sleep out + on) por `esp_lcd_panel_io_tx_param(io, (0x02 << 24) \| (cmd << 8), NULL, 0)`: 50/50 ciclos OK. |
| Light sleep | 200/200 ciclos OK con LVGL parado (`lvgl_port_stop()`/`lvgl_port_resume()`); el I2C responde justo al despertar (200/200). |
| Despertar | BOOT (GPIO0, nivel bajo), tactil (GPIO38, nivel bajo) y PWR por timer de 200 ms + lectura de `INTSTS2`: todos OK. Pantalla de vuelta en ~150 ms (dominan los 120 ms del sleep out). |
| USB | En light sleep el USB-Serial-JTAG no responde (ni log ni flasheo); vuelve al quedarse despierto. Regla: no dormir con VBUS. Para recuperar un reloj dormido: PWR 6 s y encender normal, o mantener BOOT y reconectar el USB para modo descarga (ver [PMU_SAFETY](PMU_SAFETY.md)). |
| Perifericos | `esp_restart()` no resetea el IMU: una app lo dejo a 500 Hz (`CTRL7=0x03`). El launcher debe apagarlo al arrancar (`CTRL7=0x00`), igual que el amplificador (GPIO46). |
| Display con LVGL | Registrar el panel con `lvgl_port_add_disp()` (camino SPI) funciona; ver "BSP Registra El Panel Como RGB" en GOTCHAS. |
| Arranque | `app_main` a 0,80 s, primer frame a 1,28 s. El test de PSRAM (`CONFIG_SPIRAM_MEMTEST`) cuesta ~260 ms; pantalla + tactil ~370 ms. |
| Consumo en reposo | Ver "Bluetooth Y Consumo" abajo: con auto light sleep y BLE, ~8-9 %/h (~11-12 h). El launcher (light sleep manual, sin BLE): ~2,9 %/h (~1,4 dias). |

AXP2101 leido por I2C sin libreria: `0x00` bit 3 bateria presente, bit 5 VBUS good; `0x01` bits 7:5 estado (1 cargando, 2 descargando), bit 3 = 0 con VBUS; VBAT en mV en `0x34`/`0x35` (5+8 bits, requiere bit 0 de `0x30`); porcentaje del gauge en `0xA4`; apagado con bit 0 de `0x10`.

## Bluetooth Y Consumo (2026-09-30 / 2026-10-01)

Prueba con un firmware desechable (BleLab, en `ota_4`): NimBLE como periferico Nordic UART que Gadgetbridge (Android) adopta como Bangle.js, light sleep automatico (`CONFIG_PM_ENABLE` + tickless idle) y registro de bateria en NVS cada 5 min.

| Area | Resultado |
| --- | --- |
| Gadgetbridge | Funciona: hora y zona horaria (`setTime`/`E.setTimeZone`), notificaciones con tildes, llamada, buscar movil, reconexion cifrada. |
| Flash | NimBLE + PM suman ~345 KB (BleLab 1017 KB frente a 673 KB de la plantilla). |
| RAM interna | El controlador BLE ocupa ~52 KB. Con los buffers de LVGL quedan 12-30 KB libres segun la config; ver GOTCHAS. |
| Reloj de bajo consumo | No hay cristal de 32 kHz (GPIO15/16 son SDA e I2S MCLK): con conexion solo vale el XTAL principal encendido en sleep (`CONFIG_BT_CTRL_LPCLK_SEL_MAIN_XTAL` + `CONFIG_BT_CTRL_MAIN_XTAL_PU_DURING_LIGHT_SLEEP`). |
| Intervalo de conexion | Se pide 500-600 ms, latencia 2. Android lo baja a 30 ms al enviar y no lo sube: hay que volver a pedirlo tras ~10 s sin trafico. |
| Consumo, LVGL despertando cada 2 ms | ~24 %/h con BLE y ~20 %/h sin BLE (pantalla apagada): el chip casi no dormia. |
| Consumo, LVGL suspendido | Light sleep 84-90 % del tiempo (~6 despertares/s: BLE y sondeo de PWR cada 200 ms). Noche entera con BLE conectado: ~8-9 %/h, 100 % a 23 % en 7,6 h (bateria 400 mAh). |
| Registros en reposo | Ningun periferico despierto: IMU apagado, codecs en valores de fabrica, tactil pasa a monitor a los 10 s (`0x86=01`, `0x87=0x0A`). Sospechoso principal: el panel sigue alimentado por `ALDO2` aunque este en sleep in. |
| Launcher sin BLE (base) | Light sleep manual, sondeo de PWR cada 200 ms, pantalla apagada. Noche del 2026-10-01: 100 % (recien desenchufado) a 75 % / 3,87 V en 8,6 h, ~2,9 %/h (~12 mA). El BLE con auto light sleep suma ~5-6 %/h (~20-24 mA): la mayor parte del gasto de BleLab. Aun asi la base queda lejos de los ~1-2 mA esperables del ESP32-S3 dormido. |

## Botones

| Boton | Ruta esperada | Validacion |
| --- | --- | --- |
| BOOT | `GPIO0`, bajo al pulsar | Debounce, click/long press, recovery. |
| PWR | AXP2101 `PWRON`; `EXIO6` alto al pulsar segun wiki | Validar via PMU/expander; no asumir GPIO10. |

Regla: no usar long press de `PWR` cercano a 6 s para funciones de app porque apaga la placa.

## Pines Sin Wrapper BSP

Validar solo cuando se necesiten:

| Senal | Pin/net | Resultado |
| --- | --- | --- |
| Motor | `GPIO18` | Sin motor: nivel alto/bajo y PWM 200 Hz/20 kHz durante 2 s sin vibracion (la wiki tampoco lo lista) |
| QMI8658 INT1 | `GPIO21` | Pendiente |
| RTC INT | `GPIO39` | Pendiente |
| LCD TE | `GPIO13` | Pendiente |
| SYS_OUT | `GPIO10` / `SYS_OUT` | Pendiente |
| USB pads | `D+/IO20`, `D-/IO19`, `VBUS`, `GND` | Pendiente |
| I2C pads | `IO15`, `IO14`, `3V3`, `GND` | Pendiente |
| UART pads | `RXD/U0RXD`, `TXD/U0TXD`, `3V3`, `GND` | Pendiente |

## microSD

Usar BSP ESP-IDF:

```c
ESP_ERROR_CHECK(bsp_sdcard_mount());
FILE *f = fopen(BSP_SD_MOUNT_POINT "/file.txt", "r");
```

Validar:

| Area | Esperado |
| --- | --- |
| Bus | SDMMC 1-bit. |
| Pines BSP | `CLK GPIO2`, `CMD GPIO1`, `D0 GPIO3`. |
| Mount | `/sdcard`. |
| Card detect | No declarado. |
| GPIO17 | Solo referencia Arduino/SPI-style, no usar sin validar. |

Validado 2026-09-30 (Recorder y Launcher) con una microSD de 2 GB en FAT: `bsp_sdcard_mount()` monta en ~50 ms; escritura continua de 32 KB/s en bloques de 16 KB sin desbordes en un buffer de 256 KB. Nombres largos con `CONFIG_FATFS_LFN_HEAP=y` (ver GOTCHAS). Tambien como disco USB en un Mac con TinyUSB (Launcher, Ajustes > Conectar al ordenador).

## Audio

Validar en fase propia porque audio toca I2S, codecs y amplificador.

| Area | Esperado |
| --- | --- |
| Speaker | `bsp_audio_codec_speaker_init()`, ES8311, amp `GPIO46`. |
| Mic | `bsp_audio_codec_microphone_init()`, ES7210. |
| I2S | MCLK `GPIO16`, BCLK `GPIO41`, WS `GPIO45`, DOUT `GPIO40`, DIN `GPIO42`. |
| Default BSP | Mono duplex, 16-bit, 22050 Hz si `bsp_audio_init(NULL)`. |

Validado 2026-09-30:

- Altavoz: ES8311 a 16 kHz mono (avisos del Launcher) y 22050 Hz estereo (Doom). A volumen 100 suena bajo para escuchar voz.
- Micro: ES7210 en estereo a 16 kHz, 16 bits (`MIC1` y `MIC2` en L/R, niveles casi iguales). Con 30 dB de ganancia, voz a la distancia del brazo da ~-35 dBFS; la Recorder usa 37,5 dB (maximo) y +6 dB digitales, con mas ruido de fondo.
- Micro y altavoz comparten el puerto I2S (TX y RX full duplex): abiertos a la vez deben ir a la misma frecuencia.

## Bateria Y Termica

Validar solo con bateria segura y compatible.

| Area | Esperado |
| --- | --- |
| Bateria recomendada | `4*27*28`, `400 mAh`, 3.7 V MX1.25. |
| Full brightness | Aproximadamente 1 h segun FAQ. |
| Pantalla apagada | Aproximadamente 3-4 h segun FAQ. |
| Low-power | Aproximadamente 6 h segun FAQ. |
| Temperatura | Wiki midio hasta 46 C con Wi-Fi STA/AP y carga; sin Wi-Fi/BLE aprox. 36 C. |

No asumir esas autonomias para la app final; medir consumo real.

## Criterio Para Pasar A Apps

Pasar de bring-up a arquitectura de apps solo cuando esten cerrados:

- Build limpio reproducible desde checkout.
- Display + brillo + touch OK.
- `flash_id` documentado.
- I2C scan documentado.
- Decision tomada para primer periferico extra: IMU, RTC, PMU, SD o audio.
- Si se usa bateria, comportamiento PWR/PMU validado.
