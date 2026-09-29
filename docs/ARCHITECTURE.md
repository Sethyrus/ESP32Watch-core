# Architecture

Este documento define la arquitectura base actual y los criterios para evolucionarla.

## Decision Actual

Usar `ESP-IDF 5.5.4 + LVGL + Waveshare BSP` como base principal.

Motivos:

- Es la ruta mas directa para validar display, touch, brillo, SD, audio e I2C.
- El BSP oficial ya encapsula la parte delicada del panel QSPI y touch.
- Reduce dependencias y complejidad frente a ESP-Brookesia.
- Permite construir UI propia sin adoptar un launcher o lifecycle de telefono.
- Encaja mejor con ideas tipo reloj, Poketch, demos hardware y juegos simples.

## Por Que No Brookesia Ahora

ESP-Brookesia aporta valor real, pero no para el baseline minimo.

Usarlo ahora introduciria:

- Framework de sistema y apps con lifecycle propio.
- Dependencias extra como Boost y componentes Brookesia.
- Registro de apps por constructores estaticos y posible necesidad de `WHOLE_ARCHIVE`.
- Mas reglas de navegacion, status bar, recents y gestos.
- Mayor superficie de problemas antes de validar la placa.

Brookesia se considerara si el producto necesita:

- Launcher multi-app tipo telefono.
- Apps aisladas con lifecycle formal.
- Integracion fuerte con SquareLine.
- Servicios de audio/video/AI de Espressif.
- Experiencia mas cercana a un sistema operativo movil.

## Organizacion En Repos

Cada firmware (app) vive en su propio repo y comparte lo comun a traves de este:

| Repo | Contenido |
| --- | --- |
| `ESP32Watch-core` | Componente `watch_board` (servicios de placa) y documentacion de hardware. |
| `ESP32Watch-template` | Esqueleto de proyecto para crear apps nuevas. |
| `ESP32Watch-Maze`, `ESP32Watch-Doom`, ... | Un firmware por repo, con su README, licencia y releases. |
| `ESP32Watch-Launcher` | Launcher de arranque: graba todas las apps a la vez y abre la elegida (ver "Modo Launcher"). |

Las apps dependen de `watch_board` via ESP Component Manager (`git` + `path` + `version` en `main/idf_component.yml`), fijado en `dependencies.lock`. Lo especifico de una app se queda en su repo; algo pasa a `core` cuando es hardware puro o lo usan dos apps.

## Estructura Recomendada De Una App

Mantener `main` pequeno. Cuando una pieza sea reutilizable o crezca, moverla a un componente.

Estructura inicial:

```text
main/
├── main.c
└── idf_component.yml
```

Estructura sugerida al crecer:

```text
components/
├── ui_shell/            # navegacion global, tema, pantallas base
└── apps/                # apps o demos si se adopta arquitectura modular
main/
└── main.c               # solo bootstrap
```

Si se usa `components/apps/*`, actualizar el `CMakeLists.txt` raiz con `EXTRA_COMPONENT_DIRS`.

## Reglas LVGL

- Crear y modificar objetos LVGL solo con `bsp_display_lock()` tomado.
- No hacer trabajo pesado dentro de callbacks de UI.
- Si una tarea FreeRTOS procesa sensores, que publique estado y haga update UI bajo lock.
- Evitar buffers grandes en RAM interna; usar PSRAM cuando aplique.

## Hardware Services

La app no deberia hablar directamente con todos los registros de hardware una vez que crezca. Encapsular:

- `imu_service`: init QMI8658, calibracion, ejes de pantalla, filtros.
- `rtc_service`: hora/fecha PCF85063, fallback SNTP si aparece Wi-Fi.
- `power_service`: AXP2101, voltajes, bateria, PWR key.
- `storage_service`: NVS, SPIFFS y SD.
- `audio_service`: speaker/mic sobre BSP codec APIs.

Estado actual en `components/watch_board` de este repo:

- `imu_service`: implementado (QMI8658, calibracion, ejes de pantalla, suavizado). Validado en ESP32Watch-Maze.
- `watch_buttons`: primitivas raw de `BOOT` (GPIO0) y pulsacion corta de `PWR` (IRQ del AXP2101). El debounce y la logica de pulsacion corta/larga quedan en cada app. Validado en ESP32Watch-Doom (PWR) y Maze/Doom (BOOT).
- `rtc_service`, `power_service`, `storage_service`, `audio_service`: pendientes.

Antes de crear servicios permanentes, completar o actualizar `docs/BRINGUP.md` con resultados reales de hardware. No convertir suposiciones de wiki en APIs definitivas sin validacion si afectan energia, botones, bateria o pinout externo.

## Entrada Y Energia

Reglas iniciales:

- `BOOT` puede ser input directo por `GPIO0`, activo bajo.
- `PWR` debe tratarse como evento de PMU: el esquematico lo lleva a `PWRON` del AXP2101 y la pulsacion corta se lee por su IRQ (`watch_pwr_key_take_short_press()`). La wiki habla de `EXIO6`; no usar `SYS_OUT/GPIO10` por arrastre de experimentos previos.
- El long press de `PWR` cercano a 6 s apaga la placa, asi que la UX no debe depender de mantenerlo pulsado demasiado tiempo.
- Toda politica de sleep, dimming o wake debe vivir en `power_service`, no dispersa en pantallas/apps.

### Convencion De Botones

Los dos botones estan a la derecha: `BOOT` arriba, `PWR` abajo. Todas las apps siguen el mismo reparto, que es el habitual en relojes con dos botones laterales (Samsung Galaxy Watch: Home/Back; Garmin: START/BACK):

| Boton | Pulsacion corta | Pulsacion larga |
| --- | --- | --- |
| `BOOT` (arriba) | Aceptar / seleccionar / accion principal (disparar, saltar...) | Opcional: accion secundaria definida por la app (umbral ~700 ms) |
| `PWR` (abajo) | Atras / cancelar / "No". En partida abre la pausa; con la pausa abierta, vuelve al juego | No usar: ~6 s apaga la placa |

Por que asi: `BOOT` es un GPIO con estado instantaneo (sirve para mantener pulsado, doble click, long press); `PWR` solo entrega eventos de pulsacion corta por I2C (~100 ms de latencia, sin estado de mantenido), asi que solo vale para acciones discretas.

Reglas:

- El tactil sigue siendo la navegacion principal; los botones son atajos y nunca la unica forma de hacer algo.
- Si una pantalla no tiene accion principal, `BOOT` no hace nada. No se reutiliza como "atras".
- En la raiz de una app, `PWR` vuelve al launcher si la app se arranco desde el (`watch_launcher_is_available()`); en modo standalone no hace nada.
- En el propio launcher, que no tiene "atras", `PWR` pasa a la siguiente app y `BOOT` abre la seleccionada.

## Modo Launcher

`ESP32Watch-Launcher` permite tener todas las apps grabadas y elegir cual abrir sin reflashear. Cada app sigue siendo su propio firmware:

- Tabla de particiones comun (la define el launcher; cada app lleva una copia en su `partitions.csv`): el launcher en `factory`, una app por slot OTA (`ota_0`, `ota_1`...) y `storage` FAT para el WAD de Doom.
- El launcher apunta el siguiente arranque al slot elegido y reinicia. Cambiar de app es un reinicio (~1-2 s), asi que cada app arranca siempre limpia y ninguna necesita codigo para "descargarse".
- Cada app llama a `watch_launcher_boot_once()` lo primero en `app_main`: devuelve el siguiente arranque a `factory`, de modo que cualquier reinicio (salir, cuelgue, apagado con `PWR`) vuelve al launcher.
- Para salir, `watch_launcher_exit()` (un `esp_restart()`), ofrecido solo si `watch_launcher_is_available()`. Guardar antes lo que deba persistir.
- NVS es compartida entre todas las apps: cada una usa su propio namespace.
- En standalone (`idf.py flash` desde el repo de la app) la app ocupa `factory` y las tres funciones no hacen nada.

Detalles, tabla y script de grabacion (`flash_all.sh`) en el README del launcher.

## Persistencia

Usar NVS para preferencias pequenas y SPIFFS/SD para datos medianos o assets.

Datos candidatos para NVS:

- Brillo.
- Tema/UI actual.
- Ultima app/pantalla.
- Calibracion IMU.
- Config simple de reloj/alarma.

Datos candidatos para SPIFFS/SD:

- Recursos grandes, fuentes, imagenes y audio.
- Logs largos o datos exportables.
- Video/AVI si se porta el ejemplo `06_videoplayer`.

## Configuracion Durable

Todo cambio de Kconfig debe ir a `sdkconfig.defaults`. Toda decision de particiones debe ir a `partitions.csv`.

No depender de `sdkconfig` para estado del proyecto.

Las apps usan la tabla comun del launcher (ver "Modo Launcher"): un cambio de offsets o slots se hace en `ESP32Watch-Launcher` y se copia a cada app. Si se introducen coredumps o assets grandes en flash, redisenar esa tabla antes de escribir codigo que dependa de offsets/tamanos.

## DESIGN.md

No hay `DESIGN.md` aun porque todavia no hay un producto final cerrado. Si el proyecto se fija como Poketch, reloj, launcher, Doom watch u otra direccion, crear un `DESIGN.md` de producto con UX, apps, navegacion y alcance.
