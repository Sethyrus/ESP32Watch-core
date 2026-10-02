# Gotchas

Problemas conocidos y decisiones que deben recordarse antes de tocar el firmware.

## No Editar `sdkconfig` Para Cambios Duraderos

`sdkconfig` es generado y esta git-ignored. Si un cambio debe sobrevivir a un checkout limpio, ponerlo en `sdkconfig.defaults`, `partitions.csv` o el manifest correspondiente.

## ESP-IDF 5.5.4 Es La Base

El repo esta alineado con `ESP-IDF 5.5.4`. Evitar migrar a `6.x` sin una razon concreta, porque Brookesia, BSPs y ejemplos oficiales estan mas alineados con ramas `5.x`.

## Mantener Compatibles LVGL Y esp_lvgl_port

El BSP depende de `espressif/esp_lvgl_port`. La combinacion resuelta y verificada en este repo es `esp_lvgl_port 2.9.0` + `lvgl 9.3.0`. El intento con `lvgl 9.2.0` fallo por simbolos esperados por el port, como `LV_COLOR_FORMAT_RGB565_SWAPPED`.

## PSRAM Es Obligatoria Para UI Real

La placa tiene 8 MB de PSRAM octal. El `sdkconfig` generado anterior no la tenia activa. LVGL, buffers de display, audio/video y demos grandes necesitan PSRAM.

Config minima esperada:

```text
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
```

## Flash: Wiki 32 MB, Ejemplos 16 MB

La wiki anuncia 32 MB de flash y el esquematico monta `GD25Q256EYIGR` (256 Mbit / 32 MB), pero los ejemplos ESP-IDF oficiales de Waveshare usan `CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`.

Confirmado en placa real: el chip es de 32 MB. Con la config de 16 MB de los ejemplos el boot log avisa `Detected size(32768k) larger than the size in the binary image header(16384k)` (inocuo).

Los proyectos ESP32Watch usan `CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y`. Reglas:

- El codigo (bootloader, launcher, apps) va por debajo de los 16 MB. Ejecutar codigo por encima (`CONFIG_BOOTLOADER_CACHE_32BIT_ADDR_QUAD_FLASH`) es experimental en ESP-IDF 5.5 y no se usa.
- Los datos por encima de 16 MB funcionan con la API de particiones. Validado en esta placa (GD25Q256): borrado, escritura y lectura en 0x1f00000, sin solapamiento con la mitad baja, y un FAT de solo lectura montado desde 0x1000000.
- El GD25Q256 esta en la lista oficial de chips con direcciones de 32 bits de ESP-IDF.

Aunque `CONFIG_ESPTOOLPY_FLASHMODE_QIO=y`, el comando final de esptool puede mostrar `--flash_mode dio`. En ESP-IDF 5.5 esto es normal para QIO/QOUT: esptool flashea el bootloader en modo DIO y el bootloader cambia a quad durante la inicializacion.

## CO5300 Vs SH8601

La wiki y ejemplos Arduino mencionan `CO5300`. El BSP ESP-IDF oficial usa `waveshare/esp_lcd_sh8601` y comandos SH8601.

Decision: para ESP-IDF, usar BSP oficial y tratar `SH8601` como fuente practica de verdad.

## Brillo No Es PWM

El BSP tiene comentarios antiguos que hablan de PWM, pero en esta placa el brillo se controla enviando comando QSPI `0x51` con parametro `0x00..0xFF`.

API BSP recomendada:

```c
bsp_display_brightness_set(75);
bsp_display_backlight_on();
bsp_display_backlight_off();
```

`bsp_display_start()` inicializa el brillo al 100%. Si la app quiere otro brillo inicial, llamarlo justo despues de arrancar display.

## Caveats BSP v1.0.7

El BSP tiene comentarios heredados de otros paneles/placas. Priorizar el codigo real y no la prosa del header.

- `bsp_display_start_with_config()` recibe `bsp_display_cfg_t`, pero el codigo actual calcula el buffer LVGL desde Kconfig (`CONFIG_BSP_DISPLAY_LVGL_BUF_HEIGHT` o full-screen si avoid-tear), no desde todos los campos del struct.
- Las opciones/ayudas Kconfig mencionan RGB LCD y LEDC PWM, pero esta placa usa panel QSPI `SH8601` y brillo por comando `0x51`.
- El header I2C menciona dispositivos QMA7981/OV2640, pero en esta placa los dispositivos relevantes son FT3168, QMI8658, PCF85063, AXP2101 y codecs.
- El BSP aplica `esp_lcd_panel_set_gap(panel_handle, 0x16, 0)` al panel. Si se reemplaza la ruta de display, no olvidar validar el offset X.
- `LCD_TE` existe en el esquematico como `GPIO13`, pero el BSP v1.0.7 no lo usa directamente en su ruta LVGL.

## BSP Registra El Panel Como RGB

`bsp_display_start()` (BSP 1.0.7) registra el panel QSPI con `lvgl_port_add_disp_rgb()`. En el S3 eso llama a `esp_lcd_rgb_panel_register_event_callbacks()` sobre un `sh8601_panel_t`, que es mucho mas pequeno que `esp_rgb_panel_t`: escribe 5 punteros fuera de la estructura, en el heap. Ademas `flush_ready` se da sin esperar a la DMA.

Funciona en pruebas cortas, pero es corrupcion de heap latente. Para firmwares que corren horas, crear el display con `bsp_display_new()` + `lvgl_port_add_disp()` (validado en placa, ver BRINGUP "Energia Y Sleep"). `watch_display_start()` (core v0.4.0) ya lo hace asi: Launcher, Recorder, template y Maze lo usan desde core v0.5.1. Doom y Fluid usan `bsp_display_new()` con su propio framebuffer.

## ES7210: `0x40` En Scan, `0x80` En Macro

El esquematico marca el ES7210 como `0x40` 7-bit. `esp_codec_dev` define `ES7210_CODEC_DEFAULT_ADDR` como `0x80`, que se usa en la API del codec. Para un I2C scan normal, esperar `0x40`.

## LVGL No Es Thread-Safe

Toda llamada `lv_*` hecha fuera del task interno de LVGL debe estar protegida:

```c
if (bsp_display_lock(0)) {
    lv_label_set_text(label, "Hello");
    bsp_display_unlock();
}
```

No actualizar widgets desde tareas FreeRTOS sin lock.

## Touch Ya Lo Registra `watch_display`

`watch_display_start()` (o `bsp_display_start()`, que no se usa) inicializa display, touch y el input device LVGL. No crear otro driver touch salvo que la app no use esp_lvgl_port (Doom y Fluid leen el FT3168 directamente con `esp_lcd_touch_read_data()` y toleran errores).

## QMI8658: Unidades Y Ejes

Por defecto el driver `waveshare/qmi8658` devuelve aceleracion en milli-g. Dividir entre `1000.0f` para obtener g.

Si se llama `qmi8658_set_accel_unit_mps2(&dev, true)`, ya no dividir por 1000; los datos estan en m/s2.

Mapeo probado para pantalla:

```c
float screen_x = -data.accelY / 1000.0f;
float screen_y =  data.accelX / 1000.0f;
```

Centralizar el mapeo en una sola funcion para evitar aplicar doble inversion en juegos o fisicas.

## BSP No Expone IMU, RTC Ni PMU

Aunque la placa los tiene, `BSP_CAPS_IMU` y `BSP_CAPS_BUTTONS` son 0. Para IMU usar `waveshare/qmi8658`; para RTC/PMU crear componente propio o portar lo minimo de los ejemplos oficiales. Para botones, `BOOT` es GPIO0, pero `PWR` aparece en wiki como `EXIO6`, no como GPIO directo del ESP32-S3.

Pines utiles del esquematico que tampoco son APIs BSP: motor `GPIO18` (sin motor en la placa), QMI INT `GPIO21`, RTC INT `GPIO39`, LCD TE `GPIO13`, `SYS_OUT/GPIO10` y pads externos USB/I2C/UART. Validar antes de usarlos.

## Un Solo Owner Para El Bus I2C

El BSP inicializa el bus I2C en port `1` para touch y lo deja disponible con `bsp_i2c_get_handle()`. Al anadir QMI8658, PCF85063, AXP2101 u otros dispositivos, colgarlos de ese handle.

No copiar literalmente ejemplos independientes como `01_AXP2101` que crean su propio bus con `i2c_new_master_bus()` si la app ya llamo a `bsp_display_start()`: en ese caso puede haber conflicto de ownership del puerto I2C o dobles inicializaciones dificiles de depurar.

## PWR Puede Apagar La Placa

El boton `PWR` tiene comportamiento de alimentacion ademas de posible input de usuario.

- Pulsado unos 6 s en encendido apaga la placa.
- Pulsacion en apagado enciende la placa.
- En runtime la wiki dice que se lee por `EXIO6` con nivel alto al pulsar; el esquematico muestra el boton en `PWRON` del AXP2101.
- No usar `GPIO10` como sustituto de PWR: el esquematico lo etiqueta como `SYS_OUT/GPIO10`, una ruta de sistema/PMU que requiere validacion propia.
- No implementar long-press de app cercano a 6 s sin gestionar el riesgo de apagado.

## AXP2101: Porcentaje Y Rails

La wiki avisa que el porcentaje estimado puede fluctuar, especialmente con cargador conectado, cambios de carga o envejecimiento de bateria. Preferir voltaje y tendencia para decisiones importantes.

El ejemplo ESP-IDF oficial llama `PMU.disableTSPinMeasure()` porque la placa no tiene medida de temperatura de bateria por TS; dejar esa deteccion activa puede causar carga anomala.

No copiar el ejemplo `01_AXP2101` como politica final de energia sin revisar rails. El esquematico asigna `DCDC1` a `VCC3V3`, `DCDC2` a `0.9V`, `DCDC3` a `1.2V`, `DCDC4` a `1.8V`, `RTCLDO` a `VCC-RTC`, `ALDO1/ALDO2` a rails de 3.3 V, `ALDO4` a 1.8 V y `BLDO2` a 2.8 V. Apagar canales a ciegas puede cortar display, touch, sensores, RTC o audio.

## Seguridad De Bateria Y Agua

La wiki incluye advertencias de LiPo que deben respetarse en cualquier app que gestione energia:

- Usar bateria compatible, segura y con proteccion; evitar baterias/cargadores baratos o de baja calidad.
- No invertir polaridad al cargar/descargar.
- Evitar humedad, altas temperaturas, golpes, sobrecarga y sobredescarga.
- Para almacenamiento largo, retirar la bateria y evitar dejarla en estado de carga muy bajo.
- Reemplazar baterias envejecidas al final de su vida util o tras unos dos anos.
- La placa no es waterproof; mantenerla seca.

## microSD: GPIO17 Es Solo Referencia Arduino

La wiki/ejemplos Arduino etiquetan la TF card como SPI con `CS GPIO17`, `DI/MOSI GPIO1`, `DO/MISO GPIO3`, `SCK GPIO2`. En ESP-IDF el BSP usa SDMMC 1-bit con `CLK GPIO2`, `CMD GPIO1`, `D0 GPIO3`, sin CS ni card-detect.

Decision: para ESP-IDF usar `bsp_sdcard_mount()` y no configurar `GPIO17` salvo que se porte deliberadamente un driver SPI-style y se valide en hardware.

## Factory Firmware Puede Asumir 32 MB

El repo oficial contiene firmware factory/test para self-check. Uno de los bins vistos mide unos 29 MB, asi que no encaja con los primeros 16 MB donde vive el codigo de este repo. No flashear factory bins grandes sin confirmar flash real y offsets.

## Arduino LVGL Menos Fluido Que ESP-IDF

La wiki indica que los ejemplos LVGL en Arduino son menos fluidos porque la ruta Arduino TFT/DMA acelera menos. Los ejemplos ESP-IDF usan configuraciones de buffering/anti-tearing mas adecuadas. No extrapolar rendimiento Arduino al baseline ESP-IDF.

## Brookesia No Es Baseline

ESP-Brookesia es util si se necesita launcher tipo telefono, lifecycle de apps, SquareLine o servicios/AI. Para una base de reloj, Poketch, juegos simples o validacion hardware, LVGL+BSP reduce dependencias y riesgo.

## Linker Y Registro Estatico

Si en el futuro se implementa un sistema de apps con auto-registro por constructores estaticos, cada componente de app debe usar `WHOLE_ARCHIVE` en `idf_component_register(...)`. Si no, el linker puede eliminar los registros.

## Arduino No Es Fuente Principal

Los ejemplos Arduino son utiles para entender sensores y comportamiento, pero este repo es ESP-IDF. Para pines, display y touch, preferir el BSP ESP-IDF oficial.

## Recovery

Si el firmware crashea y el USB no responde, mantener `BOOT` y encender/resetear para entrar en modo descarga antes de flashear de nuevo.

El Type-C de flashing/debug es USB nativo del ESP32-S3 y la placa tiene auto-download; en flujo normal no hace falta pulsar `BOOT`. `BOOT` es la ruta de rescate cuando el firmware o el estado USB impiden entrar automaticamente.

Notas de FAQ:

- Si el flasheo falla porque el monitor ocupa el puerto, cerrar monitor y reintentar.
- Si la placa entra en modo descarga forzado, puede no salir automaticamente tras flashear; apagar y reiniciar.
- Si el monitor queda en `waiting for download...`, volver a alimentar/reiniciar la placa.
- Para volver a encender tras apagado completo, la FAQ indica mantener `PWR` al menos 6 s y luego pulsar `PWR` otra vez.

## microSD: Aviso Falso De Nombres Largos

`bsp_sdcard_mount()` (BSP v1.0.7) avisa "Long filenames on SD card are disabled in menuconfig!" aunque esten activos: comprueba `CONFIG_FATFS_LONG_FILENAMES`, que ya no existe en ESP-IDF 5.5. Lo que cuenta es `CONFIG_FATFS_LFN_HEAP=y` (o `_STACK`) en `sdkconfig.defaults`; por defecto es `CONFIG_FATFS_LFN_NONE` (solo nombres 8.3). Un `sdkconfig` ya generado no recoge el cambio de `sdkconfig.defaults`: borrarlo y recompilar.

## USB-OTG Deja El Puerto Sin Consola Tras Reiniciar

El ESP32-S3 tiene un solo PHY USB: por defecto lo usa USB-Serial-JTAG (consola y flasheo); TinyUSB (p. ej. disco USB) lo pasa a USB-OTG. El selector (`RTCCNTL.usb_conf.sw_hw_usb_phy_sel` / `sw_usb_phy_sel`) esta en el dominio RTC, asi que `esp_restart()` no lo devuelve: tras salir del modo USB el Mac no ve ni consola ni dispositivo. Solo lo arregla un apagado completo (PWR 6 s). Solucion (Launcher `os_usb_restore_port()`): poner los dos bits a 0 antes de reiniciar y tambien al arrancar.

## No Borrar `dependencies.lock` Por Rutina

La wiki de Waveshare recomienda borrar `build`, `managed_components` y `dependencies.lock` en algun troubleshooting de demos. En este repo `dependencies.lock` es parte del estado reproducible: borrar `build/` y `managed_components/` es limpieza local; cambiar o regenerar `dependencies.lock` solo si se aceptan nuevas versiones resueltas.

## Light Sleep Automatico (PM) Con LVGL Y BLE

Lecciones de BleLab (2026-09-30), para cuando el launcher pase de `esp_light_sleep_start()` manual a `CONFIG_PM_ENABLE` con tickless idle:

- `CONFIG_PM_POWER_DOWN_CPU_IN_LIGHT_SLEEP` y `..._TAGMEM_...` vienen activos por defecto y colgaron el chip en el primer sleep (codigo y rodata en PSRAM). Desactivarlos en `sdkconfig.defaults`.
- `lvgl_port_stop()` desactiva los timers de LVGL; entonces `lv_timer_handler()` devuelve 1 ms y la tarea `taskLVGL` despierta cada ~2 ms, lo que impide entrar en light sleep (0 % del tiempo). Con la pantalla apagada hay que suspenderla (`vTaskSuspend(xTaskGetHandle("taskLVGL"))` con el lock de LVGL tomado) y reanudarla al encender.
- El tactil tambien aborta en sleep: ver "Tactil En Sleep: Abort Al Despertar" abajo.
- La pantalla encendida no gana nada durmiendo: tomar un lock `ESP_PM_NO_LIGHT_SLEEP` mientras este encendida y mientras haya USB.
- Con USB y auto light sleep, el Mac pierde el dispositivo y no siempre lo recupera al despertar (desenchufar y enchufar). Abrir el puerto reinicia el chip. Para medir en bateria, registrar en NVS y leerlo despues.
- El driver I2C toma `NO_LIGHT_SLEEP` durante cada transaccion: sondear PWR cada 200 ms tiene coste.

## Tactil En Sleep: Abort Al Despertar

El tactil va por interrupcion (GPIO38): cada flanco deja pendiente en `esp_lvgl_port` una lectura I2C del FT3168. En light sleep (manual o automatico) el aislamiento de GPIO da picos en esa linea, y la lectura se hace en cuanto `taskLVGL` vuelve a correr, justo al despertar. Si falla, el `ESP_ERROR_CHECK(esp_lcd_touch_read_data())` de `esp_lvgl_port_touch.c` hace abort y el chip se reinicia. En el launcher se ve como un parpadeo al despertar con BOOT; con el arranque desde una app, como volver al launcher.

Traza (BleTest, 2026-10-02): `abort` <- `_esp_error_check_failed` <- `lvgl_port_touchpad_read` <- `lv_indev_read` <- `lvgl_port_task`.

Arreglo (core v0.5.1, en `watch_display_sleep()`/`watch_display_wake()`, y por tanto en `watch_power`):

- Al dormir: `gpio_intr_disable(BSP_LCD_TOUCH_INT)` y `lv_indev_enable(touch, false)`; un indev desactivado no se lee.
- Al despertar: panel encendido, 100 ms, y entonces indev e interrupcion activos otra vez.
- `gpio_sleep_sel_dis()` en SCL/SDA (14/15) y en GPIO38 para que el sleep no los aisle.

Una app que duerma por su cuenta (sin `watch_power`) debe hacer lo mismo.

## BLE Y RAM Interna

El controlador BLE ocupa ~52 KB de RAM interna. Con los buffers de LVGL del display solo quedaban ~12 KB y un `xTaskCreate` de 8 KB fallo sin aviso (la app se quedo congelada con el BLE funcionando). `CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL=y` lleva el host NimBLE a PSRAM. Comprobar siempre el retorno de `xTaskCreate`.

## No Cortar Los Rails Del AXP2101 Del Panel

2026-10-01: una prueba apago `ALDO1`/`ALDO2`/`ALDO3` (reg `0x90`) e hiberno el tactil con la pantalla en sleep in; al volver a encenderlos, el siguiente arranque murio en el init del panel, el USB desaparecio y la placa dejo de encender (sin USB ni en modo descarga; PWR 2/10/20 s sin efecto). `ALDO2` es el enable de alimentacion del AMOLED (`DSI_PWR_EN`) mientras `VCC3V3` sigue alimentando el panel, y el AXP2101 apaga toda la placa si un DCDC cae un 15 % (`0x23=0x3F`). No escribir rails del AXP2101 desde una app sin una via de recuperacion probada.

Recuperacion, reglas de que no escribir y el caso de la pagina `0xFF`: [PMU_SAFETY](PMU_SAFETY.md).

## Diagnostico De Cuelgues En Bateria

Con bateria y light sleep no hay consola USB, y abrir el puerto reinicia el chip: un cuelgue no deja rastro.

- `esp_reset_reason()` sobrevive al cambio de firmware (lo guarda un registro RTC, no la RAM), asi que el launcher ve el panic o watchdog de una app al arrancar. Lo apunta en NVS y lo muestra en Ajustes > Acerca de.
- `RTC_NOINIT` no sirve para pasar datos de una app al launcher: los primeros 32 bytes de RTC slow (`0x50000000`) los reescribe cada imagen al arrancar (segmento `.rtc.force_slow`), y el launcher usa la RTC fast como heap.
- Para depurar a fondo, una app de prueba puede envolver el panic con `-Wl,--wrap=esp_panic_handler` (funcion en IRAM, sin tocar flash ni PSRAM) y guardar el backtrace en RTC; al volver a arrancar, pasarlo a NVS. Leer la NVS con `esptool read_flash 0x9000 0x6000` y `nvs_tool.py`.
