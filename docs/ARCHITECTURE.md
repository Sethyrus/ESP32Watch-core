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

El multi-app ya esta resuelto con un launcher propio que cambia de app reiniciando (ver "Modo Launcher"), sin lifecycle compartido.

Brookesia se considerara si el producto necesita:

- Varias apps vivas a la vez en un mismo firmware, con lifecycle formal.
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

## Estructura De Una App

Es el patron que siguen todas las apps (partir de `ESP32Watch-template`):

```text
main/
├── main.c               # solo arranque: watch_launcher_boot_once() y <app>_start()
└── idf_component.yml    # watch_board fijado por tag
components/
├── <app>_app/           # la app: init de placa, tareas, UI, ajustes (Kconfig propio si hace falta)
└── <app>_engine/        # opcional: logica pura sin hardware (fisica, juego...), como fluid_engine
docs/<APP>_DESIGN.md     # diseno y decisiones de la app
partitions.csv           # copia de la tabla comun del launcher
```

Separar motor y app cuando la logica es grande: el motor no incluye BSP ni LVGL y se puede razonar (o probar) aparte. `ESP-IDF` encuentra `components/*` solo, sin `EXTRA_COMPONENT_DIRS`.

Dos formas de usar el display, segun la app:

- UI con LVGL: `bsp_display_start()` (Maze, Launcher, template).
- Framebuffer propio a pantalla completa: `bsp_display_new()` y dibujar sobre el panel (Doom, Fluid). LVGL puede seguir usandose solo para menus.

## Reglas LVGL

- Crear y modificar objetos LVGL solo con `bsp_display_lock()` tomado.
- No hacer trabajo pesado dentro de callbacks de UI.
- Si una tarea FreeRTOS procesa sensores, que publique estado y haga update UI bajo lock.
- Evitar buffers grandes en RAM interna; usar PSRAM cuando aplique.

## Hardware Services

La app no deberia hablar directamente con todos los registros de hardware una vez que crezca. Encapsular:

- IMU: init QMI8658, calibracion, ejes de pantalla, filtros.
- RTC: hora/fecha PCF85063, fallback SNTP si aparece Wi-Fi.
- Energia: AXP2101, voltajes, bateria, PWR key, sleep/dimming.
- Almacenamiento: NVS, SPIFFS y SD.
- Audio: speaker/mic sobre BSP codec APIs.

Estado actual en `components/watch_board` de este repo:

- `imu_service`: implementado (QMI8658, calibracion, ejes de pantalla, suavizado). Validado en Maze y Fluid.
- `watch_buttons`: `BOOT` (GPIO0) raw y con debounce (`watch_boot_debouncer_*`, pulsacion corta y larga) y pulsacion corta de `PWR` (IRQ del AXP2101). Validado en Maze, Doom, Fluid y Launcher (el debouncer, desde v0.3.0, aun no lo usan las apps: lo adoptan cuando se toquen).
- `watch_rtc`: PCF85063, hora local sin zona horaria copiada al reloj del sistema. Validado en Fluid, de donde viene.
- `watch_nvs`: init unica de la NVS compartida (ver "Persistencia").
- `watch_launcher`: modo launcher (ver "Modo Launcher").
- Energia (bateria, sleep, dimming), SD y audio: pendientes. Hoy cada app usa el BSP directamente (Doom: SD y audio).

Antes de crear servicios permanentes, completar o actualizar `docs/BRINGUP.md` con resultados reales de hardware. No convertir suposiciones de wiki en APIs definitivas sin validacion si afectan energia, botones, bateria o pinout externo.

## Entrada Y Energia

Reglas iniciales:

- `BOOT` puede ser input directo por `GPIO0`, activo bajo.
- `PWR` debe tratarse como evento de PMU: el esquematico lo lleva a `PWRON` del AXP2101 y la pulsacion corta se lee por su IRQ (`watch_pwr_key_take_short_press()`). La wiki habla de `EXIO6`; no usar `SYS_OUT/GPIO10` por arrastre de experimentos previos.
- El long press de `PWR` cercano a 6 s apaga la placa, asi que la UX no debe depender de mantenerlo pulsado demasiado tiempo.
- Toda politica de sleep, dimming o wake debe vivir en un servicio de energia en `watch_board` (pendiente), no dispersa en pantallas/apps.

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

La NVS es una sola para todo el reloj: en modo launcher la comparten todas las apps. Por eso:

- Inicializarla siempre con `watch_nvs_init()`. Si esta llena o tiene formato viejo la borra entera, lo que afecta a todas las apps, y lo deja en el log.
- Cada app usa solo su namespace (maximo 15 caracteres) y nunca borra la particion completa:

| App | Namespace |
| --- | --- |
| Launcher | `launcher` (ultima app abierta) |
| Fluid | `fluid` (ajustes) |
| Maze, Doom | Sin NVS por ahora |

Una app nueva que use NVS anade aqui su namespace.

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

## Documentos De Diseno

Cada app documenta su diseno en su repo (`docs/MAZE_DESIGN.md`, `docs/FLUID_DESIGN.md`, `docs/DOOM_PORT.md`). Lo que afecta a todas las apps (botones, launcher, persistencia, particiones) vive en este documento.
