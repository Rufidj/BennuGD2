#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

static const char *app_path(const char *path, char *buf, size_t size) {
    if (!path || path[0] == '/') return path;
    /* "." (the current-directory case our own glob() resolves relative
       components against, e.g. opendir(".") while walking down a
       "[gG][fF][xX]/*.[pP][nN][gG]" pattern) must map to "/app0" itself -
       this payload SDK's filesystem rejects "/app0/." with EPERM, not the
       ENOENT a missing path would give, confirmed on real hardware. */
    if (path[0] == '.' && path[1] == '\0') {
        snprintf(buf, size, "/app0");
        return buf;
    }
    snprintf(buf, size, "/app0/%s", path);
    return buf;
}

FILE *__wrap_fopen(const char *path, const char *mode) {
    char buf[512];
    const char *p = app_path(path, buf, sizeof buf);
    FILE *f = fopen(p, mode);
    if (!f && path && path[0] != '/') {
        FILE *g = fopen("/app0/data/paths.log", "ab");
        if (g) { fprintf(g, "fopen FAIL rel=%s mapped=%s errno=%d\n", path, p, errno); fclose(g); }
    }
    return f;
}

DIR *__wrap_opendir(const char *name) {
    char buf[512];
    const char *p = app_path(name, buf, sizeof buf);
    DIR *d = opendir(p);
    if (!d) {
        FILE *g = fopen("/app0/data/paths.log", "ab");
        if (g) { fprintf(g, "opendir FAIL %s errno=%d\n", p, errno); fclose(g); }
    }
    return d;
}

/* ---- tracing wrappers (log BEFORE the real call so a crash leaves a trace) */
#include <stdarg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <zlib.h>

static void ptrace_log(const char *what, const char *path) {
    FILE *g = fopen("/app0/data/paths.log", "ab");
    if (g) { fprintf(g, "%s %s\n", what, path ? path : "(null)"); fclose(g); }
}

gzFile __wrap_gzopen(const char *path, const char *mode) {
    char buf[512];
    const char *p = app_path(path, buf, sizeof buf);
    gzFile r = gzopen(p, mode);
    if (!r) ptrace_log("gzopen -> NULL", p);
    return r;
}

int __wrap_open(const char *path, int flags, ...) {
    int mode = 0;
    if (flags & O_CREAT) { va_list ap; va_start(ap, flags); mode = va_arg(ap, int); va_end(ap); }
    char buf[512];
    return open(app_path(path, buf, sizeof buf), flags, mode);   /* relative paths live under /app0 */
}

/* stat()/lstat() raise SIGSYS in this sandbox; open()+fstat() works. */
static int sbx_stat(const char *path, struct stat *sb) {
    char buf[512];
    const char *p = app_path(path, buf, sizeof buf);
    int fd = open(p, O_RDONLY);
    if (fd < 0) { ptrace_log("stat -> ENOENT", p); return -1; }
    int r = fstat(fd, sb);
    close(fd);
    if (r != 0) ptrace_log("stat -> fstat failed", p);
    return r;
}
int __wrap_stat(const char *path, struct stat *sb) { return sbx_stat(path, sb); }
int __wrap_lstat(const char *path, struct stat *sb) { return sbx_stat(path, sb); }

/* getcwd()/realpath() are refused by the sandbox; the working directory of a
   title is always /app0. */
#include <stdlib.h>
char *__wrap_getcwd(char *buf, size_t size) {
    static const char cwd[] = "/app0";
    if (!buf) {
        buf = (char *)malloc(size > sizeof cwd ? size : sizeof cwd);
        if (!buf) return NULL;
    } else if (size < sizeof cwd) {
        errno = ERANGE;
        return NULL;
    }
    memcpy(buf, cwd, sizeof cwd);
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
