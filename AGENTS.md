# AGENTS.md

## Project Shape
- ESP-IDF component library + hardware docs shared by the ESP32Watch firmwares (Launcher, Maze, Doom, Fluid, Recorder, template).
- Target hardware is Waveshare `ESP32-S3-Touch-AMOLED-2.06`; full details in `docs/HARDWARE.md`.
- Baseline stack is `ESP-IDF 5.5.4` + `waveshare/esp32_s3_touch_amoled_2_06` BSP. Do not migrate to ESP-IDF 6.x or ESP-Brookesia unless explicitly requested.
- `components/watch_board` is consumed by apps via ESP Component Manager (`git` + `path` + `version` tag). Its public API is a contract: breaking changes need a new tag and a note in README.
- Public headers: `imu_service.h`, `watch_buttons.h` (raw BOOT, BOOT debouncer, PWR short press), `watch_rtc.h`, `watch_nvs.h`, `watch_display.h`, `watch_power.h`, `watch_battery.h`, `watch_launcher.h`. AXP2101 access is shared through the private `watch_pmu_priv.h`; do not add a second device handle for 0x34. Keep README's API table in sync.
- Only add code here when it is hardware-level or used by two or more apps, and after validating it on real hardware (see `docs/BRINGUP.md`).

## Alineacion De Repos
Core is the source of truth, the template is the minimal app pattern, and the apps inherit from both. Nothing learned or fixed may stay in a single repo:
- A behavior change or fix in `watch_board` ships as a new tag. In the same batch, bump the pin in the template and in every app (`version:` in `main/idf_component.yml`, then `idf.py update-dependencies` and `idf.py build`), and update the "Proyectos que lo usan" table in README.
- A lesson found in an app or a scratch test firmware goes to `docs/` here (GOTCHAS, BRINGUP, ARCHITECTURE) and, if it changes how an app should be written, to the template too.
- Fix stale docs (versions, app lists, header lists, hardware facts) in every repo that repeats them, not only here.
- Run `tools/check_apps.sh` (read-only, looks at the sibling `../ESP32Watch-*` repos) and leave it without warnings.

## Commands
- Source ESP-IDF: `source "$HOME/.espressif/v5.5.4/esp-idf/export.sh"`.
- Verification: `cd examples/basic && idf.py set-target esp32s3 && idf.py build`.
- No test, lint or format targets are configured; do not invent them.

## Documentation Map
- `docs/HARDWARE.md`: board parts, pins, buses, sensors, APIs and addresses.
- `docs/SETUP.md`: ESP-IDF 5.5.4 setup, build/flash, config files and dependencies.
- `docs/GOTCHAS.md`: known pitfalls and implementation cautions.
- `docs/PMU_SAFETY.md`: AXP2101 rules (which registers never to write without explicit user approval), reference dump, diagnosis and recovery. Read it before any PMU write.
- `docs/ARCHITECTURE.md`: LVGL+BSP decision, repo organization, Brookesia criteria, the app-wide button convention (BOOT = accept, PWR = back/menu) and launcher mode (`watch_launcher.h`, shared partition table owned by ESP32Watch-Launcher), app structure and shared NVS (`watch_nvs.h`, one namespace per app, listed there).
- `docs/BRINGUP.md`: hardware validation checklist.
- `docs/SOURCES.md`: official links, datasheets and examples.
