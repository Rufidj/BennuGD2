FFmpeg 7.1 for the Switch with the https / crypto / data protocols (devkitPro's build leaves them out) and TLS through mbedTLS.
Source: https://ffmpeg.org/releases/ffmpeg-7.1.tar.xz + devkitPro's patches (pacman-packages/switch/ffmpeg: ffmpeg-7.1.patch, tls.patch), plus, in libavformat:
 - network.c (copy here): no SOCK_CLOEXEC on the Switch and a getnameinfo() stub (the reverse-DNS call is only used for log messages and
   blocks forever in emulators), both found testing in the Eden emulator;
 - tls.c (copy here): the "is the host a numeric IP" test uses inet_pton on the Switch (libnx's getaddrinfo ignores AI_NUMERICHOST, so no TLS SNI was
   sent and many servers answered 421 Misdirected Request);
 - Makefile (copy here): tls_libnx.o is left out so that --enable-mbedtls provides the "tls" protocol. The console's SSL service is not
   implemented by the emulators, and with mbedTLS the same code runs everywhere. Certificates are checked against the Mozilla bundle that
   build_switch.sh puts in the romfs (res/cacert.pem, from https://curl.se/ca/cacert.pem), passed to FFmpeg as ca_file by libmod_video (NET_TLS_OPTS).
Configure line: build_ff.sh (--enable-version3 is needed by mbedTLS; paths in it are those of the machine it was built on).
Only include/ and lib/ are used by build.sh (target switch); they are not kept in git.
