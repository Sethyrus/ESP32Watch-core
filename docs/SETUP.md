# Setup And Build

Entorno recomendado para todos los proyectos ESP32Watch: `ESP-IDF 5.5.4` con target `esp32s3`.

## Instalacion

Instalar con ESP-IDF Installation Manager (EIM) o con la extension ESP-IDF de VS Code, eligiendo la version `v5.5.4`. La ruta por defecto de EIM es:

```sh
$HOME/.espressif/v5.5.4/esp-idf
```

EIM requiere Python `3.10`-`3.13` con pip, venv y SSL. En macOS, EIM usa su propio `PATH` (`/opt/homebrew/bin:/usr/local/bin:/usr/bin:...`), asi que un Python de Homebrew versionado (`python@3.12`) debe tener `python3` enlazado en `/opt/homebrew/bin` o EIM detectara el Python 3.9 del sistema.

Activacion del entorno:

```sh
source "$HOME/.espressif/v5.5.4/esp-idf/export.sh"
```

Comprobar version:

```sh
idf.py --version
```

## Build

```sh
source "$HOME/.espressif/v5.5.4/esp-idf/export.sh"
idf.py set-target esp32s3
idf.py build
```

## Flash Y Monitor

Puerto en macOS: `/dev/tty.usbmodem*`; el numero cambia segun el puerto USB (p. ej. `/dev/tty.usbmodem1101`). Listarlo con `ls /dev/tty.usbmodem*`; sin `-p`, `idf.py` lo autodetecta.

Comando:

```sh
idf.py -p <PORT> flash monitor
```

Salir de monitor: `Ctrl+]`.

## Archivos De Configuracion

| Archivo | Tipo | Regla |
| --- | --- | --- |
| `sdkconfig.defaults` | Fuente durable | Editar aqui cambios de Kconfig que deben sobrevivir. |
| `sdkconfig` | Generado/local | No editar para cambios duraderos; esta git-ignored. |
| `partitions.csv` | Fuente durable | Tabla de particiones del proyecto. |
| `dependencies.lock` | Fuente durable generada | Bloquea versiones resueltas por ESP Component Manager. Actualizar al cambiar manifests. |
| `managed_components/` | Generado | Lo crea ESP Component Manager; esta git-ignored. |
| `build/` | Generado | Lo crea `idf.py build`; esta git-ignored. |

## Config Base Actual

Decisiones del baseline:

- Target: `esp32s3`.
- ESP-IDF: `5.5.4`.
- Flash mode: `QIO`.
- Flash size configurado: `16MB`.
- PSRAM: habilitada, octal, 80 MHz.
- CPU: 240 MHz.
- FreeRTOS tick: 1000 Hz.
- LVGL: v9.3.0 por manifest, con malloc/string/sprintf de libc.
- BSP: `waveshare/esp32_s3_touch_amoled_2_06`.
- BSP I2C: port 1, 400 kHz.
- Mounts BSP base: SPIFFS `/spiffs` si existe una particion SPIFFS, SD `/sdcard`.

Nota sobre flash: el chip es de 32 MB (`GD25Q256EYIGR`, confirmado en placa real: el boot log avisa `Detected size(32768k) larger than the size in the binary image header(16384k)`). Los proyectos siguen configurados a 16 MB por compatibilidad con los ejemplos oficiales Waveshare; el aviso es inocuo. Para usar todo el flash, cambiar a `CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y` y rehacer `partitions.csv`, manteniendo la app por debajo de 16 MB.

## Particiones

Cada proyecto define su propio `partitions.csv`. La tabla base (template y ejemplos de este repo) es single-factory sin OTA:

| Particion | Tipo | Tamano | Uso |
| --- | --- | --- | --- |
| `nvs` | data/nvs | `0x6000` | Config pequena, calibraciones, preferencias. |
| `phy_init` | data/phy | `0x1000` | Datos PHY ESP-IDF. |
| `factory` | app/factory | `8M` | Firmware. |
| `storage` | data/spiffs | `7M` | SPIFFS para assets. |

Las apps (Maze, Doom, Fluid) usan en cambio la tabla comun de [ESP32Watch-Launcher](https://github.com/Sethyrus/ESP32Watch-Launcher): `nvs`, `otadata`, `phy_init`, `factory` 1,5 MB (launcher), tres slots OTA de 2 MB (una app cada uno) y `storage` FAT ~8,4 MB (WAD de Doom). Asi se pueden grabar todas a la vez y cambiar entre ellas desde el launcher (ver [ARCHITECTURE](ARCHITECTURE.md#modo-launcher)). No hay particion de coredump. Si se necesitan crash dumps persistentes o assets mas grandes, redisenar la tabla antes de escribir codigo que dependa de offsets/tamanos.

## VS Code

La extension ESP-IDF debe apuntar a:

```text
$HOME/.espressif/v5.5.4/esp-idf
```

`clangd` usa `build/compile_commands.json`. Si cambian fuentes, dependencias o config, regenerar con:

```sh
idf.py build
```

## Devcontainer

El devcontainer usa la imagen Docker `espressif/idf:v5.5.4` para mantener la misma version que el baseline local. Evitar tags flotantes tipo `release-v5.5` si se necesita reproducibilidad exacta.

## Dependencias

El componente `main` declara dependencias en `main/idf_component.yml`:

```yaml
dependencies:
  idf: ">=5.5,<5.6"
  waveshare/esp32_s3_touch_amoled_2_06: "^1.0.6"
  lvgl/lvgl:
    version: "9.3.0"
    public: true
```

Para IMU y botones, anadir el componente `watch_board` de este repo (ver `README.md`); ya trae `waveshare/qmi8658`.

Para RTC/PMU/audio: preferir componentes separados o driver propio minimo, idealmente en este repo cuando esten validados; no mezclar todo en `main/main.c`.

## Ejemplos Oficiales Waveshare

El repo oficial de Waveshare contiene ejemplos ESP-IDF bajo `examples/ESP-IDF-v5.4.2`. Se usan como referencia, pero este proyecto queda fijado en ESP-IDF `5.5.4`.

| Ejemplo | Valor para este repo |
| --- | --- |
| `01_AXP2101` | Referencia PMU/bateria/carga con XPowersLib. |
| `02_lvgl_demo_v9` | Referencia LVGL+BSP, particiones 8M app + 7M SPIFFS. |
| `03_esp-brookesia` | Referencia futura si se adopta framework de apps. |
| `04_Immersive_block` | Referencia IMU QMI8658 + fisicas LVGL. |
| `05_Spec_Analyzer` | Referencia microfonos/audio capture + visualizacion. |
| `06_videoplayer` | Referencia AVI desde TF con audio; requiere assets en SD. |

No copiar ejemplos completos a los repos salvo que se porten conscientemente. Preferir extraer drivers o patrones minimos.

## Problemas Frecuentes

`idf.py` no existe:

```sh
source "$HOME/.espressif/v5.5.4/esp-idf/export.sh"
```

Build usa ESP-IDF incorrecto:

```sh
idf.py --version
```

Si aparece una `6.x`, revisar VS Code y shell: EIM puede tener varias versiones instaladas.

Falla por componentes antiguos o cacheados:

```sh
idf.py reconfigure
```

Si sigue fallando por cache de build, borrar `build/` manualmente o desde el IDE. Si se sospecha resolucion vieja de componentes, borrar tambien `managed_components/` y regenerar. `dependencies.lock` solo debe actualizarse si se aceptan nuevas versiones resueltas; no borrarlo como limpieza rutinaria aunque algunos textos de Waveshare lo sugieran para demos aisladas. No borrar cambios fuente.
