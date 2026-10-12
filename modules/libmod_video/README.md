# libmod_video

FFmpeg video playback for BennuGD2, as an independent module (BennuGD2's own sources are not modified).

```
import "libmod_video";

PROCESS main()
PRIVATE int v, g;
BEGIN
    set_mode(1280, 720);
    v = video_open("intro.mp4");        // mp4/h264, webm/vp9, mkv, avi, ogv(theora)... whatever FFmpeg decodes
    g = video_graph(v);                 // a graph that always shows the current frame
    video_play(v, 0);                   // 0 = once, 1 = loop
    graph = g; x = 640; y = 360;
    WHILE (!video_is_finished(v)) FRAME; END
    video_close(v);
END
```

Functions: VIDEO_OPEN, VIDEO_GRAPH, VIDEO_PLAY(h,loop), VIDEO_PAUSE(h,paused), VIDEO_STOP, VIDEO_SEEK(h,ms),
VIDEO_CLOSE, VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_DURATION, VIDEO_POSITION (ms), VIDEO_IS_PLAYING,
VIDEO_IS_FINISHED, VIDEO_VOLUME(h,0..128).

Audio goes through SDL_mixer's music hook (Mix_HookMusic), so it shares the device BennuGD already opened;
a video that owns the audio replaces MUSIC_PLAY while it plays. Decoding is in software; for 8-bit 4:2:0 video
(H.264/VP9/Theora... `yuv420p`) the colour conversion runs on the GPU, see below.

Licensing: FFmpeg is LGPL/GPL depending on how it was built; check your FFmpeg build before shipping.

## GPU colour conversion
`yuv420p` / `yuvj420p` frames are not converted to RGB on the CPU: the decoder thread only copies the three planes,
which are uploaded as packed RGBA8 textures (four samples per texel, works on GL 3 / GLES2 alike) and a small
shader writes RGB into the video's graph (BT.601/709, limited/full range read from the stream). Other pixel formats,
or a renderer that cannot do it, fall back to the software converter automatically.
`VIDEO_USE_GPU(0)` forces software for videos opened afterwards; `VIDEO_GPU(handle)` tells which one is in use.
Tests: `test/video_gpu_pc.prg` (identical output either way), `test/video_gpu_compare.prg` (fps GPU vs software).

## Hardware decoding (PS5): experimental, off in practice
The module has the whole path for the console's decoder block (`libSceVideodec2`, `libmod_video_hwdec.c`):
H.264 / HEVC access units are converted to Annex B, decoded to NV12 / 16-bit NV12 and turned into RGB by the same GPU
shader (BT.709 / BT.2020, basic PQ tone map). It falls back to libavcodec on any failure; `VIDEO_USE_HW(0)` disables it
and `VIDEO_HW(handle)` tells which decoder a video uses. The shader paths are verified on PC with
`LIBMOD_VIDEO_FAKE_HW=1` (swscale produces what the decoder would give).

**Status on PS5: the decoder itself does not run yet.** What was established on hardware (native-app-boilerplate title):
- `libSceVideodec2` loads: `sceKernelLoadStartModule("/<sandbox word>/common/lib/libSceVideodec2.sprx")`
  (`sceKernelGetFsSandboxRandomWord()`), also `sceKernelLoadStartModuleForSysmodule`.
- `sceKernelDlsym` finds nothing in this runtime (not even `malloc` of `libc.prx`), so function addresses cannot be looked up.
- Importing `libSceSysmodule` makes the title not start with this boilerplate's packaging.
- Static imports of `libSceVideodec2` (a link stub in the SDK) do get bound after loading the library, but the first call
  faults inside a system module (a libkernel function, `rip` in the `0x800000000` region). Most likely cause: the
  title's own `libc.prx` threads lack the TCB/TLS structures libkernel expects, which VLC's runtime (PS5_Vulkan) provides.
- The title's code segments are execute-only (prot 4): a crash handler must not read code at `rip`.
The experimental variant with static imports, big-stack threads and the crash report is kept as
`ps5/notes/libmod_video_hwdec_experimental.c` for whoever continues (try VLC's packaging chain, or the same call from a thread
created through libkernel's own `scePthread*`).

Reference for the API facts (structure layouts, call order, NV12/P010 output, memory rules): the `ps5vdec.c`
plugin of github.com/ZiZc3/VLC (GPL-2.0+), used as a reference only; no code was copied.

## Subtitles
Text tracks (SRT, ASS/SSA, WebVTT, mov_text) embedded in the container, or an external file loaded with
`VIDEO_SUBTITLE_LOAD`. The module does not draw them; the script shows `VIDEO_SUBTITLE_TEXT(handle)` with its own
fonts (so any charset/style BennuGD can print works). Image-based tracks (PGS, DVB) are ignored.
```
n = video_subtitle_tracks(v);            // embedded + external
say(video_subtitle_lang(v, 0));          // "spa - Castellano"
video_subtitle_select(v, 0);             // -1 = off (default)
video_subtitle_load(v, "movie.es.srt");  // extra track, becomes selected
write(font, x, y, 4, video_subtitle_text(v));   // "" when nothing is due
```
Test: `test/video_subtitles.prg` (+ `test_subs.mkv`, `test_subs.srt`).

## Building

### PC (Linux)
```
# FFmpeg dev headers: apt install libavcodec-dev libavformat-dev libswscale-dev libswresample-dev libavutil-dev
# (or, without root, extract the .deb files into third_party/ffmpeg_pc/sysroot as done here)
gcc -shared -fPIC -O2 -DUSE_SDL2_GPU=1 $(sdl2-config --cflags) \
    -I$BENNU/core/include -I$BENNU/core/bgdrtm -I$BENNU/modules/libbggfx -I$BENNU/modules/libsdlhandler \
    -I$BENNU/vendor/sdl-gpu/include -I. $(pkg-config --cflags libavformat libavcodec libswscale libswresample libavutil) \
    libmod_video.c -o libmod_video.so -L$BENNU/build/linux-gnu/bin -lbggfx -lbgdrtm -lSDL2_mixer \
    $(pkg-config --libs libavformat libavcodec libswscale libswresample libavutil)
```
Put `libmod_video.so` next to the other module libraries, then `import "libmod_video";`.
Or symlink this directory into `BennuGD2/modules/` and let BennuGD2's CMake build it
(`-DFFMPEG_INCLUDE_DIRS=... -DFFMPEG_LIBRARIES=...` if pkg-config cannot find FFmpeg).

### PS5 (static, native-app-boilerplate)
1. Symlink into `BennuGD2/modules/libmod_video` and regenerate `core/include/fake_dl.h`
   (`core/make-fakedl.sh`, needs `find -L` to follow the symlink).
2. Configure `build/x86_64-sie-ps5` with `-DFFMPEG_INCLUDE_DIRS` / `-DFFMPEG_LIBRARIES` pointing at the
   pacbrew FFmpeg (`ps5-payload-sdk-pacbrew/target/user/homebrew`), `make mod_video`.
3. Link `libmod_video.a`, then `libavformat libavcodec libswscale libswresample libavutil libssl libcrypto
   libiconv liblzma libbz2` (copy them into the app's `libs/`; the packager only accepts in-repo `.a` paths),
   and keep `libps5_runtime_gaps.a` LAST (it supplies `gmtime_r`, `localtime_r`, `nl_langinfo`, `_setjmp`,
   `sendmmsg`, `dladdr`... that OpenSSL/iconv pull in).
4. The packaging layer must map relative paths to `/app0` for `open()` (FFmpeg opens files with it).

## Notes / limits
- Software decoding only, with GPU colour conversion (the PS5 has no hardware decoder exposed here); 640x360 H.264 and 320x240 VP9/Theora
  run in real time on the console. Heavier streams depend on the CPU.
- A closed video's graph is freed ~30 frames later: freeing and recreating a texture back to back left the new
  video black on the PS5 (the GPU executes draw calls after the CPU queued them).
- Audio: one video at a time owns the SDL_mixer music hook. Looping restarts the audio ring.
- Test material: `test/` (regenerate with the ffmpeg lavfi commands in the history; `video_test.prg` plays one,
  `video_test_seq.prg` plays three in a row).
