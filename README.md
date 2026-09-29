# ESP32Watch-core

Servicios de placa y documentacion de hardware compartidos por los firmwares ESP32Watch para la Waveshare **ESP32-S3-Touch-AMOLED-2.06** (ESP32-S3R8, AMOLED 410x502, QMI8658, AXP2101...).

Stack: `ESP-IDF 5.5.4` + BSP `waveshare/esp32_s3_touch_amoled_2_06`.

## Proyectos que lo usan

| Repo | Que es |
| --- | --- |
| [ESP32Watch-template](https://github.com/Sethyrus/ESP32Watch-template) | Plantilla para crear un firmware nuevo. |
| [ESP32Watch-Maze](https://github.com/Sethyrus/ESP32Watch-Maze) | Juego de laberinto controlado por inclinacion (IMU). |
| [ESP32Watch-Doom](https://github.com/Sethyrus/ESP32Watch-Doom) | Port de Doom (doomgeneric). |
| [ESP32Watch-Fluid](https://github.com/Sethyrus/ESP32Watch-Fluid) | Simulacion de fluido controlada por la IMU. |
| [ESP32Watch-Launcher](https://github.com/Sethyrus/ESP32Watch-Launcher) | Launcher de arranque: todas las apps grabadas a la vez, se elige cual abrir. |

## Componente `watch_board`

| Header | API |
| --- | --- |
| `imu_service.h` | QMI8658: init, calibracion, lectura de aceleracion ya mapeada a ejes de pantalla y suavizada. |
| `watch_buttons.h` | `BOOT` (GPIO0) raw y pulsacion corta de `PWR` via IRQ del AXP2101. El debounce queda en la app. Convencion de uso (BOOT = aceptar, PWR = atras/menu) en [ARCHITECTURE](docs/ARCHITECTURE.md#convencion-de-botones). |
| `watch_launcher.h` | Modo launcher (desde v0.2.0): `watch_launcher_boot_once()` al principio de `app_main`, `watch_launcher_is_available()` para mostrar "Salir" y `watch_launcher_exit()` para volver. Sin launcher no hacen nada. Ver [ARCHITECTURE](docs/ARCHITECTURE.md#modo-launcher). |

Todo usa el bus I2C del BSP (`bsp_i2c_get_handle()`, que lo inicializa en el primer uso). `imu_service` no es thread-safe: llamarlo siempre desde el mismo task.

### Usarlo en un proyecto

En `main/idf_component.yml`:

```yaml
dependencies:
  idf: ">=5.5,<5.6"
  watch_board:
    git: https://github.com/Sethyrus/ESP32Watch-core.git
    path: components/watch_board
    version: v0.2.0
```

Y en el `CMakeLists.txt` del componente que lo use: `REQUIRES watch_board`.

El commit exacto queda fijado en `dependencies.lock`. Para actualizar, cambiar `version` al nuevo tag y compilar.

Para desarrollar `core` y una app a la vez, sustituir temporalmente la dependencia por una ruta local:

```yaml
  watch_board:
    override_path: ../../ESP32Watch-core/components/watch_board
```

## Ejemplo

`examples/basic` inicializa I2C, IMU y botones y los loguea por serie:

```sh
source "$HOME/.espressif/v5.5.4/esp-idf/export.sh"
cd examples/basic
idf.py set-target esp32s3
idf.py build flash monitor
```

## Documentacion

| Documento | Contenido |
| --- | --- |
| [docs/HARDWARE.md](docs/HARDWARE.md) | Piezas, pines, buses, sensores, direcciones I2C y APIs. |
| [docs/SETUP.md](docs/SETUP.md) | Instalacion de ESP-IDF 5.5.4, build/flash, config y dependencias. |
| [docs/GOTCHAS.md](docs/GOTCHAS.md) | Problemas conocidos y precauciones. |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Decision LVGL+BSP, organizacion en repos, criterios Brookesia. |
| [docs/BRINGUP.md](docs/BRINGUP.md) | Checklist de validacion de hardware. |
| [docs/SOURCES.md](docs/SOURCES.md) | Enlaces oficiales, datasheets y ejemplos. |

## Versionado

Tags semver (`vX.Y.Z`). Cambios incompatibles en la API de `watch_board` suben la version menor mientras sea `0.x`.

## Licencia

MIT. Ver [LICENSE](LICENSE).
