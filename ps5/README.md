# BennuGD2 on PS5 (Prospero) — branch `ps5`

Work in progress port of BennuGD2 to the PlayStation 5, running as a real title (not a payload) through the
SDK-free "native-app-boilerplate" packager + the PS5 OpenGL (Mesa) driver. Verified on a real console (FW 9.00):
Street of Rage Remake, Galaxian, Joselkiller, Pac-Man, a 3D scene on `libmod_3d`, and video playback.

## What this branch changes in BennuGD2
- `build.sh`: new `ps5` target (static modules, cross toolchain in `cmake/Toolchains/ps5*.{cmake,sh}`).
- `core/`: `__PROSPERO__` support in bgdi/bgdrtm; `fake_dl.h` generation fixes (`make-fakedl.sh`):
  duplicate `module_initialize` symbols no longer fall back to NULL (this left `libmod_3d` uninitialised and the 3D black),
  `EXCLUDE_FAKEDL_MODULES`, `find -L`.
- `modules/libbggfx`: letterbox-correct scissor for scrolls (`gr_screen_clip`), screen clear deferred for modules that draw a
  full-screen image (PS5 blocks ~13 ms on the first touch of the screen buffer), stable sort of scroll processes,
  `gfx.cfg` tuning switches, Theora stub for PS5 (`g_media_theora_stub.c`).
- `modules/libmod_gfx`, `libmod_ads`, `libmod_iap`: renames/exports needed when every module links into one binary.
- `vendor/ps5/`: small prebuilt `libz.a`, SDL2_image shim (stb based) and headers used by the PS5 build.

## Not in this branch (kept as patches in `ps5/patches/`)
Nested/submodule repos can't be carried by this repo, so their PS5 changes are saved as patches:
- `libmod_3d-ps5.patch` — against `Rufidj/libmod_3d@940cf53` (PS5 sampler/uniform limits, water culling by visible block, FFT/foam/shadows every 2nd frame, incremental IBL bake, perf instrumentation).
- `sdl-gpu-ps5.patch` — against `vendor/sdl-gpu@69532af` (PS5 context, rotating blit VBOs).
- `theoraplay-ps5.patch` — against `vendor/theoraplay@c93be85`.
- `optional-sysprof-interpreter.patch` — optional native-call profiler for `core/bgdrtm/interpreter.c` (enabled by a `sysprof.on` file).

The FFmpeg video module is a separate project (`libmod_video`), not part of BennuGD2.

## Things learned the hard way (PS5 sandbox)
- A title loads `/app0/data/game.dcb`; relative game paths map to `/app0/<path>` (wrap `fopen/open/gzopen/stat`).
- `stat`, `getcwd`, `realpath`, `opendir` are refused (SIGSYS/EPERM): use open+fstat, `/app0`, and `.index` files for directory listings.
- Never `exit()` / return from main: the process-exit syscall is refused (SIGSYS). Ask the system: `sceSystemServiceLoadExec("exit", NULL)`.
- Each write to a log file costs ~60 ms: keep stdout/stderr fully buffered.
- `.dcb` files must be recompiled with the current `bgdc` (an old `z` as int vs the current `DOUBLE z` breaks draw order).
- PS5 GL limits: 16 samplers per fragment shader, 1024 uniform components per vertex shader.

## Tools
`ps5/tools/build-title.sh` is a copy of the script that builds the eboot for one title in the native-app-boilerplate
packager (link order matters: engine -> FFmpeg and deps -> libz -> GL driver -> libc gaps LAST). It lives in
`native-app-boilerplate/apps/bennugd2/` next to the app's `main.c`.
