# AGENTS.md (ESP32Watch workspace)

Instructions for agents started in the workspace folder that holds all the ESP32Watch repos side by side. This folder is not a git repo: each `ESP32Watch-*` subfolder is its own repo with its own `AGENTS.md`. The source of this file is `ESP32Watch-core/workspace/AGENTS.md`; the workspace root has a symlink to it (`ln -s ESP32Watch-core/workspace/AGENTS.md AGENTS.md`).

## Repos
| Folder | Role |
| --- | --- |
| `ESP32Watch-core` | Source of truth: `watch_board` component (shared board services) and all hardware docs (`docs/`). |
| `ESP32Watch-template` | Minimal app pattern; its README has the checklist for creating a new app. |
| `ESP32Watch-Launcher` | Watch "OS" in `factory`; owns the shared `partitions.csv`, `apps.conf` and `flash_all.sh`. |
| `ESP32Watch-Maze`, `-Doom`, `-Fluid`, `-Recorder` | Apps, each in its own OTA slot, booted from the Launcher. |

## Working Across Repos
- Before changing a repo, read its `AGENTS.md`; its rules (including commit rules) apply inside it. Run git and `idf.py` from inside that repo.
- Cross-repo changes follow "Alineacion De Repos" in `ESP32Watch-core/AGENTS.md`: core fixes ship as a new tag and every pin is bumped in the same batch; lessons go to core `docs/` and, when they change how apps are written, to the template.
- Partition layout changes start in the Launcher and are copied to every app (Launcher `AGENTS.md`).
- After any cross-repo change, run `ESP32Watch-core/tools/check_apps.sh` (read-only) and leave it without warnings.
- Hardware facts (pins, PMU safety, gotchas) live only in `ESP32Watch-core/docs/`; read `PMU_SAFETY.md` before any AXP2101 access.
