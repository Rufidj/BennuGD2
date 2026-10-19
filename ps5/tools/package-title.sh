#!/bin/bash
# Turns the freshly built PS5 engine archives into a ready-to-install title folder.
# Called by `./build.sh ps5` when PS5_TITLE_ID is set; it can also be run by hand after a build.
#
#   PS5_TITLE_ID=PPSA00090 PS5_TITLE_NAME="My Game" PS5_SCE_SYS=/path/to/sce_sys \
#   PS5_DCB=/path/to/game.dcb PS5_ASSETS=/path/to/assets PS5_OUT=/path/to/out ps5/tools/package-title.sh
#
#   PS5_TITLE_ID    PPSAxxxxx (required)           PS5_TITLE_NAME  name for the packager (default "BennuGD2 Game")
#   PS5_SCE_SYS     dir with param.json + icon0.png for that title (required)
#   PS5_DCB         compiled game, becomes data/game.dcb (optional: without it the title has no game)
#   PS5_ASSETS      dir whose content is copied into the title root (gfx/, res/, videos/ ...); every directory
#                   in it gets a fresh ".index" (the sandbox refuses opendir(): the engine lists with it)
#   PS5_OUT         where the title folder is written (default build/ps5-title/<TITLE_ID>)
#   PS5_BOILERPLATE native-app-boilerplate checkout (default $PS5_OPENGL_ROOT/native-app-boilerplate)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PS5_OPENGL_ROOT="${PS5_OPENGL_ROOT:-/home/ruben/Escritorio/ps5-opengl}"
BP="${PS5_BOILERPLATE:-$PS5_OPENGL_ROOT/native-app-boilerplate}"
ID="${PS5_TITLE_ID:?set PS5_TITLE_ID (PPSAxxxxx)}"
NAME="${PS5_TITLE_NAME:-BennuGD2 Game}"
SCE="${PS5_SCE_SYS:?set PS5_SCE_SYS (dir with param.json and icon0.png)}"
OUT="${PS5_OUT:-$ROOT/build/ps5-title/$ID}"
LIBS="$ROOT/build/x86_64-sie-ps5/bin"
APP="$BP/apps/bennugd2"

[ -d "$BP" ]      || { echo "package-title: missing $BP (PS5_BOILERPLATE)" >&2; exit 2; }
[ -d "$SCE" ]     || { echo "package-title: PS5_SCE_SYS $SCE is not a directory" >&2; exit 2; }
ls "$LIBS"/*.a >/dev/null 2>&1 || { echo "package-title: no archives in $LIBS - build first (./build.sh ps5)" >&2; exit 2; }

echo "### PS5 package: $ID \"$NAME\" ###"

# 1. sources the boilerplate compiles into every title (the copy there goes stale otherwise)
sync_file() { if ! cmp -s "$1" "$2"; then cp "$1" "$2"; echo "  updated $(basename "$2")"; fi; }
for f in "$ROOT"/ps5/native-app/src/*; do sync_file "$f" "$APP/src/$(basename "$f")"; done
sync_file "$ROOT/ps5/tools/build-title.sh" "$APP/build-title.sh"; chmod +x "$APP/build-title.sh"

# 2. the boilerplate carries copies of the core headers/sources it builds against: they must match core/
for pair in bennugd2-core-include:core/include bennugd2-bgdrtm:core/bgdrtm bennugd2-bgdi:core/bgdi; do
    v="$BP/vendor/${pair%%:*}"; r="$ROOT/${pair##*:}"
    if [ -d "$v" ] && ! diff -rq "$v" "$r" 2>/dev/null | grep -qv '^Only in'; then :; else
        if [ -d "$v" ]; then echo "  syncing $(basename "$v") from ${pair##*:}"; cp -a "$r"/. "$v"/; fi
    fi
done

# 3. libc gaps (opendir/stat/getaddrinfo shims) when their sources are newer than the archive
GAPS="$APP/libs/libps5_runtime_gaps.a"
if [ ! -f "$GAPS" ] || [ "$ROOT/ps5/tools/ps5_runtime_gaps.c" -nt "$GAPS" ] || [ "$ROOT/ps5/tools/ps5_net_shim.c" -nt "$GAPS" ]; then
    echo "  rebuilding libps5_runtime_gaps.a"
    sh "$ROOT/ps5/tools/build-runtime-gaps.sh" "$BP"
fi

# 3b. FreeType (TTF fonts) and its libpng dependency come from the PacBrew sysroot; the packager only links archives inside its repo
PACBREW="${PS5_PACBREW_PREFIX:-/home/ruben/ps5-payload-sdk-pacbrew/target/user/homebrew}"
for lib in libfreetype.a libpng16.a; do
    if [ -f "$PACBREW/lib/$lib" ]; then sync_file "$PACBREW/lib/$lib" "$APP/libs/$lib"; else echo "  warning: $PACBREW/lib/$lib not found (TTF fonts will not link)"; fi
done

# 4. engine archives -> eboot
cp "$LIBS"/*.a "$APP/libs/bgdclean/"
# the packager only accepts input paths inside its own repo: stage the title's sce_sys there
rm -rf "$BP/apps/title_$ID"; mkdir -p "$BP/apps/title_$ID"; cp -r "$SCE" "$BP/apps/title_$ID/sce_sys"
rm -f "$BP/dist/$ID/eboot.bin"          # so a failed link can never be mistaken for the previous eboot
LOG="$(mktemp)"
( cd "$BP" && "$APP/build-title.sh" "$ID" "$NAME" "apps/title_$ID/sce_sys" ) > "$LOG" 2>&1 || true
if ! grep -q "Build complete" "$LOG" || [ ! -s "$BP/dist/$ID/eboot.bin" ]; then
    echo "package-title: the eboot build FAILED:" >&2; tail -30 "$LOG" >&2; rm -f "$LOG"; exit 1
fi
rm -f "$LOG"

# 5. title folder
# only ever replaces a previous output of this script (never an arbitrary non-empty directory)
if [ -d "$OUT" ] && [ -n "$(ls -A "$OUT")" ] && [ ! -f "$OUT/eboot.bin" ]; then
    echo "package-title: $OUT exists, is not empty and is not a previous title folder - refusing to overwrite" >&2; exit 2
fi
rm -rf "$OUT"; mkdir -p "$OUT/data"
cp -r "$BP/dist/$ID"/. "$OUT"/
if [ -n "$PS5_DCB" ]; then cp "$PS5_DCB" "$OUT/data/game.dcb"; else echo "  (no PS5_DCB: data/game.dcb not included)"; fi
if [ -n "$PS5_ASSETS" ]; then
    cp -r "$PS5_ASSETS"/. "$OUT"/
    find "$OUT" -mindepth 1 -type d ! -path "$OUT/sce_sys*" ! -path "$OUT/sce_module*" ! -path "$OUT/data*" | while read -r d; do
        ( cd "$d" && ls -1 > .index.tmp 2>/dev/null; grep -v '^\.index' .index.tmp > .index; rm -f .index.tmp )
    done
fi
echo "### Title ready: $OUT ($(du -sh "$OUT" | cut -f1)) - upload eboot.bin last ###"
