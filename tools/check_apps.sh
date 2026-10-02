#!/usr/bin/env bash
# Read-only alignment check of the sibling app repos (../ESP32Watch-*) against this core.
# Prints one line per check and exits 1 if anything is out of line.
# Usage: tools/check_apps.sh [workspace_dir]   (default: the parent of this repo)
set -u

CORE="$(cd "$(dirname "$0")/.." && pwd)"
WS="${1:-$(dirname "$CORE")}"
LAUNCHER="$WS/ESP32Watch-Launcher"
TEMPLATE="$WS/ESP32Watch-template"

CORE_VER="v$(sed -n 's/^version: *"\(.*\)"/\1/p' "$CORE/components/watch_board/idf_component.yml")"
LAST_TAG="$(git -C "$CORE" describe --tags --abbrev=0 2>/dev/null || echo none)"
FAIL=0

warn() { echo "  WARN  $*"; FAIL=1; }
ok() { echo "  ok    $*"; }
md5_of() { md5 -q "$1" 2>/dev/null || md5sum "$1" | cut -d' ' -f1; }

echo "core: component $CORE_VER, last tag $LAST_TAG"
[ "$CORE_VER" = "$LAST_TAG" ] || warn "core component version $CORE_VER is not tagged (last tag $LAST_TAG)"

PART_REF="$(md5_of "$LAUNCHER/partitions.csv")"
CI_REF="$(md5_of "$TEMPLATE/.github/workflows/build.yml")"

for repo in "$WS"/ESP32Watch-*; do
    name="$(basename "$repo")"
    [ "$name" = "ESP32Watch-core" ] && continue
    [ -f "$repo/main/idf_component.yml" ] || continue
    echo "$name"

    pin="$(grep -A4 'watch_board:' "$repo/main/idf_component.yml" | sed -n 's/^ *version: *//p' | tr -d '"')"
    if grep -A4 'watch_board:' "$repo/main/idf_component.yml" | grep -q override_path; then
        warn "watch_board uses override_path (local core), not a tag"
    elif [ "$pin" = "$CORE_VER" ]; then
        ok "watch_board $pin"
    else
        warn "watch_board $pin, core is $CORE_VER"
    fi

    lock="$(grep -A8 '^  watch_board:' "$repo/dependencies.lock" 2>/dev/null | sed -n 's/^ *version: *//p' | head -1)"
    [ -n "$lock" ] || warn "dependencies.lock has no watch_board entry"

    if [ "$(md5_of "$repo/partitions.csv")" = "$PART_REF" ]; then
        ok "partitions.csv = Launcher"
    else
        warn "partitions.csv differs from the Launcher's"
    fi

    if grep -rqs --include='*.c' 'bsp_display_start(' "$repo/main" "$repo/components"; then
        warn "uses bsp_display_start() (registers the panel as RGB, see GOTCHAS)"
    fi

    if [ "$name" != "ESP32Watch-Launcher" ]; then
        first="$(sed -n '/app_main/,/}/p' "$repo/main/main.c" | grep -m1 -E '^\s+[a-z_]+\(' | tr -d ' ')"
        case "$first" in
            watch_launcher_boot_once*) ok "watch_launcher_boot_once() first in app_main" ;;
            *) warn "app_main does not start with watch_launcher_boot_once() (first call: $first)" ;;
        esac
    fi

    if [ "$(md5_of "$repo/.github/workflows/build.yml" 2>/dev/null)" = "$CI_REF" ]; then
        ok "CI workflow = template"
    else
        warn "CI workflow missing or differs from the template's"
    fi
done

[ $FAIL -eq 0 ] && echo "all aligned" || echo "misaligned: see WARN lines"
exit $FAIL
