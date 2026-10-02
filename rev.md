# Storm

- Grabadora
- Tamagotchi

## Pendiente tecnico (ronda de alineacion 2026-10-02)

- Guarda en `watch_pmu_write()`: comprobar `0x03 == 0x4A` y `0xFF == 0` antes de escribir y verificar despues (PMU_SAFETY "No usar escrituras a ciegas"). Toca la ruta de la PMU: solo con aprobacion explicita.
- Ajustes compartidos (brillo, timeout de pantalla) en core: hoy la Recorder y la template leen `bright`/`timeout` del namespace `launcher` con rangos copiados.
- Maquina de atenuacion y sleep duplicada en Launcher `os_ui.c` y Recorder `rec_ui.c`: candidata a `watch_power`.
- Doom, Maze y Fluid tienen su propio antirrebote de BOOT; sustituir por `watch_boot_debouncer_*`.
- Sleep por inactividad en Maze y Fluid (Fluid no usa esp_lvgl_port: tendria que replicar GOTCHAS "Tactil En Sleep").
- `sw_rotate = true` en `watch_display.c` reserva un tercer buffer de ~41 KB de RAM interna DMA que nadie usa (ninguna app rota la pantalla). Con `false`, esp_lvgl_port llama al arrancar a `swap_xy` (el SH8601 no lo soporta: error en el log) y reescribe MADCTL. Liberarlo cuando haga falta RAM (fase BLE), probando que la orientacion no cambia.
