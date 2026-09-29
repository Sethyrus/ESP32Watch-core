# AGENTS.md

## Project Shape
- ESP-IDF component library + hardware docs shared by the ESP32Watch firmwares (Maze, Doom, Fluid, Launcher, template).
- Target hardware is Waveshare `ESP32-S3-Touch-AMOLED-2.06`; full details in `docs/HARDWARE.md`.
- Baseline stack is `ESP-IDF 5.5.4` + `waveshare/esp32_s3_touch_amoled_2_06` BSP. Do not migrate to ESP-IDF 6.x or ESP-Brookesia unless explicitly requested.
- `components/watch_board` is consumed by apps via ESP Component Manager (`git` + `path` + `version` tag). Its public API is a contract: breaking changes need a new tag and a note in README.
- Only add code here when it is hardware-level or used by two or more apps, and after validating it on real hardware (see `docs/BRINGUP.md`).

## Commands
- Source ESP-IDF: `source "$HOME/.espressif/v5.5.4/esp-idf/export.sh"`.
- Verification: `cd examples/basic && idf.py set-target esp32s3 && idf.py build`.
- No test, lint or format targets are configured; do not invent them.

## Documentation Map
- `docs/HARDWARE.md`: board parts, pins, buses, sensors, APIs and addresses.
- `docs/SETUP.md`: ESP-IDF 5.5.4 setup, build/flash, config files and dependencies.
- `docs/GOTCHAS.md`: known pitfalls and implementation cautions.
- `docs/ARCHITECTURE.md`: LVGL+BSP decision, repo organization, Brookesia criteria, the app-wide button convention (BOOT = accept, PWR = back/menu) and launcher mode (`watch_launcher.h`, shared partition table owned by ESP32Watch-Launcher).
- `docs/BRINGUP.md`: hardware validation checklist.
- `docs/SOURCES.md`: official links, datasheets and examples.
