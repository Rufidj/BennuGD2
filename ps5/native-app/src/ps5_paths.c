#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

FILE *__real_fopen(const char *path, const char *mode);
DIR *__real_opendir(const char *name);

/* Base de rutas del titulo. Tras un jailbreak on-demand (lapy_jb_daemon) la
   raiz del proceso pasa a ser la raiz REAL del host y "/app0" deja de existir:
   main() cambia esta base a "/mnt/sandbox/<TID>_<NNN>/app0" y todos los
   fopen/open/gzopen/stat envueltos siguen resolviendo. */
char ps5_app_base[256] = "/app0";

static const char *app_path(const char *path, char *buf, size_t size) {
    if (!path) return path;
    if (path[0] == '/') {
        /* "/app0" y "/app0/..." absolutos deben seguir la base cuando cambio */
        if (strcmp(ps5_app_base, "/app0") != 0 &&
            strncmp(path, "/app0", 5) == 0 && (path[5] == '\0' || path[5] == '/')) {
            snprintf(buf, size, "%s%s", ps5_app_base, path + 5);
            return buf;
        }
        return path;
    }
    /* "." (the current-directory case our own glob() resolves relative
       components against, e.g. __real_opendir(".") while walking down a
       "[gG][fF][xX]/*.[pP][nN][gG]" pattern) must map to the app base
       itself - this payload SDK's filesystem rejects "/app0/." with EPERM,
       not the ENOENT a missing path would give, confirmed on real hardware. */
    if (path[0] == '.' && path[1] == '\0') {
        snprintf(buf, size, "%s", ps5_app_base);
        return buf;
    }
    snprintf(buf, size, "%s/%s", ps5_app_base, path);
    return buf;
}

/* ---- tracer de rutas ----
   Dormante por defecto: cada escritura de log cuesta ~60 ms en esta consola y
   un glob que sondea ficheros inexistentes lo dispararia a cada paso (por eso
   apuntar a /app0/data, solo lectura, "funcionaba": nunca escribia). Se
   activa creando /data/paths.trace (por FTP) y escribe en /data/paths.log,
   escribible con y sin sandbox y sobreviviente al crash. */
#include <stdarg.h>
static void ptrace_log(const char *fmt, ...) {
    FILE *m = __real_fopen("/data/paths.trace", "rb");
    if (!m) return;
    fclose(m);
    FILE *g = __real_fopen("/data/paths.log", "ab");
    if (g) {
        va_list ap; va_start(ap, fmt); vfprintf(g, fmt, ap); va_end(ap);
        fclose(g);
    }
}

FILE *__wrap_fopen(const char *path, const char *mode) {
    char buf[512];
    const char *p = app_path(path, buf, sizeof buf);
    FILE *f = __real_fopen(p, mode);
    if (!f && path && path[0] != '/')
        ptrace_log("fopen FAIL rel=%s mapped=%s errno=%d\n", path, p, errno);
    return f;
}

DIR *__wrap_opendir(const char *name) {
    char buf[512];
    const char *p = app_path(name, buf, sizeof buf);
    DIR *d = __real_opendir(p);
    if (!d)
        ptrace_log("opendir FAIL %s errno=%d\n", p, errno);
    return d;
}

/* ---- wrappers con I/O nativo ---- */
#include <fcntl.h>
#include <sys/stat.h>
#include <zlib.h>

gzFile __real_gzopen(const char *path, const char *mode);
gzFile __wrap_gzopen(const char *path, const char *mode) {
    char buf[512];
    const char *p = app_path(path, buf, sizeof buf);
    gzFile r = __real_gzopen(p, mode);
    if (!r) ptrace_log("gzopen FAIL %s errno=%d\n", p, errno);
    return r;
}

int __real_open(const char *path, int flags, ...);
int __wrap_open(const char *path, int flags, ...) {
    int mode = 0;
    if (flags & O_CREAT) { va_list ap; va_start(ap, flags); mode = va_arg(ap, int); va_end(ap); }
    char buf[512];
    return __real_open(app_path(path, buf, sizeof buf), flags, mode);   /* relative paths live under /app0 */
}

int __real_stat(const char *path, struct stat *sb);
/* stat()/lstat() raise SIGSYS in this sandbox; open()+fstat() works. */
static int sbx_stat(const char *path, struct stat *sb) {
    char buf[512];
    const char *p = app_path(path, buf, sizeof buf);
    int fd = __real_open(p, O_RDONLY);
    if (fd < 0) { ptrace_log("stat ENOENT %s errno=%d\n", p, errno); return -1; }
    int r = fstat(fd, sb);
    close(fd);
    if (r != 0) ptrace_log("stat fstat FAIL %s errno=%d\n", p, errno);
    return r;
}
int __wrap_stat(const char *path, struct stat *sb) { return sbx_stat(path, sb); }
/* __wrap_lstat/__wrap_getcwd (unlike __wrap_stat) used to leak into the dynamic symbol table
   because libScePosixForWebKit.so bundled its own same-named __wrap_lstat/__wrap_getcwd
   stubs for WebKit's internal use, and lld must keep a symbol dynamically exported whenever
   a needed .so defines the same name (for correct interposition) - no renaming avoided it,
   plain "lstat"/"getcwd" collided too, against libkernel*.so/libSceLibcInternal.so. Fixed at
   the build-config level by dropping libScePosixForWebKit.so as a needed library (see the
   "remove libScePosixForWebKit.so" workflow step) instead of here. */
int __wrap_lstat(const char *path, struct stat *sb) { return sbx_stat(path, sb); }

/* getcwd()/realpath() are refused by the sandbox; the working directory of a
   title is always /app0. */
#include <stdlib.h>
char *__wrap_getcwd(char *buf, size_t size) {
    const char *cwd = ps5_app_base[0] ? ps5_app_base : "/app0";
    size_t n = strlen(cwd) + 1;
    if (!buf) {
        buf = (char *)malloc(size > n ? size : n);
        if (!buf) return NULL;
    } else if (size < n) {
        errno = ERANGE;
        return NULL;
    }
    memcpy(buf, cwd, n);
    return buf;
}

char *__wrap_realpath(const char *path, char *resolved) {
    char tmp[512];
    const char *p = app_path(path, tmp, sizeof tmp);
    if (!resolved) resolved = (char *)malloc(1024);
    if (!resolved) return NULL;
    snprintf(resolved, 1024, "%s", p);
    return resolved;
}
