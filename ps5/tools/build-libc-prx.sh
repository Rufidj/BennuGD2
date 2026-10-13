#!/bin/sh
# Builds the title's sce_module/libc.prx ourselves with the clean-room emitter of native-app-boilerplate
# (tooling/native/libc_builder.cpp, GPL-3.0-or-later), checks it is byte-identical to the digest the boilerplate
# records, and writes the attribution notice that has to travel with a release.
#
#   ps5/tools/build-libc-prx.sh /path/to/native-app-boilerplate [out_dir]
#
# The module is NOT Sony's: it is generated from two text manifests (api-surface.txt, imports.txt) and the emitter
# reads no reference binary (see docs/RUNTIME_SHIM.md in the boilerplate).
set -e
BP="${1:?path to native-app-boilerplate}"; OUT="${2:-$BP/dist-libc}"
( cd "$BP" && USE_CCACHE=0 make libc )
( cd "$BP/runtime" && sha256sum --check --strict libc.prx.sha256 )
mkdir -p "$OUT"
cp "$BP/runtime/libc.prx" "$OUT/libc.prx"
COMMIT="$(git -C "$BP" rev-parse HEAD 2>/dev/null || echo unknown)"
SUM="$(cut -d' ' -f1 "$BP/runtime/libc.prx.sha256")"
cat > "$OUT/libc.prx.NOTICE.txt" <<NOTICE
sce_module/libc.prx
===================
Generated with the clean-room runtime emitter of ps5-native-app-boilerplate
(https://github.com/blackbearreloaded/ps5-native-app-boilerplate, commit $COMMIT),
Copyright (C) 2026 BlackBearReloaded, SPDX-License-Identifier: GPL-3.0-or-later.

This file is a loader companion module, not a C library and not a Sony file: the emitter
(tooling/native/libc_builder.cpp) builds it from tooling/native/runtime/api-surface.txt and imports.txt and reads
no reference binary.

SHA-256 (signed container): $SUM
Reproduce:  make libc   (in the boilerplate repository, see docs/RUNTIME_SHIM.md)

The corresponding source is the boilerplate repository at the commit above. The licence of this module does not
extend to the application code that is shipped next to it.
NOTICE
echo "libc.prx -> $OUT/libc.prx  (sha256 $SUM)"
