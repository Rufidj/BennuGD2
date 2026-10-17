/* Generic PS5 packaging-side gaps - unrelated to which BennuGD2 modules are
 * built. Every module fake_dl.h references now comes from BennuGD2's own
 * clean CMake build (build.sh ps5); this file only covers libc/toolchain
 * holes that exist no matter which modules are linked in. (The previous,
 * larger stub_fake_dl_missing.c also stood in for libbginput/libmod_debug/
 * libmod_net/the Theora decoder - those are now either real compiled
 * modules or, for libmod_net, cleanly excluded from fake_dl.h itself via
 * build.sh's EXCLUDE_FAKEDL_MODULES; see modules/libmod_net/CMakeLists.txt.
 * The Theora stand-in moved inside libbggfx itself, as
 * modules/libbggfx/g_media_theora_stub.c.) */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* __assert: BSD libc's assertion-failure reporter (noreturn), same role as
 * ps5-opengl's own Mesa-side stub - this static libc build has no real one. */
void __assert(const char *func, const char *file, int line, const char *failedexpr) {
    (void)func; (void)file; (void)line; (void)failedexpr;
    _Exit(134);
}

/* -------------------- linker-script placeholders (see ps5-opengl's own
 * src/ps5_opengl_stubs.c for the full rationale) -------------------- */

char __eh_frame_start;
extern char __eh_frame_end __attribute__((alias("__eh_frame_start")));
extern char __eh_frame_hdr_start __attribute__((alias("__eh_frame_start")));
extern char __eh_frame_hdr_end __attribute__((alias("__eh_frame_start")));

int mkstemps(char *name_template, int suffix_len) { (void)name_template; (void)suffix_len; return -1; }
void openlog(const char *ident, int option, int facility) { (void)ident; (void)option; (void)facility; }

/* popen/pclose: only ever used by ps5-opengl's optional ACO disassembly
 * debug-print path (aco_print_asm.cpp piping through objdump) - not
 * exercised by normal shader compilation, "unavailable" is correct here. */
#include <stdio.h>
FILE *popen(const char *command, const char *mode) { (void)command; (void)mode; return NULL; }
int pclose(FILE *stream) { (void)stream; return -1; }

/* lseek64/__errno_location: zlib's (Linux-flavored) gzlib.c/gzwrite.c/
 * gzread.c expect GNU libc names; this BSD-derived payload SDK already has
 * everything they need under its own names (lseek takes a 64-bit off_t
 * already; errno is the macro `(*__error())`) - these just forward. */
#include <unistd.h>
#include <errno.h>
off_t lseek64(int fd, off_t offset, int whence) { return lseek(fd, offset, whence); }
int *__errno_location(void) { return &errno; }

/* _ZTH23_mesa_glapi_tls_Context: see ps5-opengl's own src/ps5_opengl_stubs.c
 * for the full rationale (weak Itanium-ABI TLS-init wrapper with no strong
 * definition anywhere; this toolchain's post-link tool requires a resolvable
 * symbol even for a weak reference, unlike a real ld/lld). */
void *_ZTH23_mesa_glapi_tls_Context(void) { return (void *)0; }

/* glob/globfree: core/bgdrtm/dirs.c (BennuGD2's FILES()/DIR scripting
 * commands) builds a case-insensitive pattern - every letter in EVERY path
 * component becomes a [xX] bracket pair, e.g. "gfx" + wildcard "*.png" ->
 * "[gG][fF][xX]" + "*.[pP][nN][gG]" (confirmed via a trace build on real
 * hardware: a bare opendir() of the literal bracketed directory component
 * fails, errno=2 - the directory component itself needs case-insensitive
 * resolution too, the same as the filename) - and calls
 * glob(pattern, GLOB_ERR | GLOB_NOSORT, NULL, &globd). This payload SDK's
 * glob.h declares the standard BSD glob_t/flags but the libc has no actual
 * glob()/globfree() to link; this is a real, working implementation (not a
 * "just degrade" stand-in - a level/file browser calling FILES() needs
 * real matches): walks and resolves every '/'-separated component in turn,
 * matching '*', '?' and '[...]' (plain character sets, no ranges/negation -
 * all dirs.c ever generates) against real directory entries at each level,
 * then globs the final component. No GLOB_APPEND/GLOB_DOOFFS/GLOB_MARK/
 * GLOB_BRACE/GLOB_TILDE support - dirs.c never asks for them. */
#include <string.h>
#include <dirent.h>
#include <glob.h>

static int ps5_glob_match(const char *pat, const char *s) {
    while (*pat) {
        if (*pat == '*') {
            while (*pat == '*') pat++;
            if (!*pat) return 1;
            for (; *s; s++)
                if (ps5_glob_match(pat, s)) return 1;
            return 0;
        } else if (*pat == '?') {
            if (!*s) return 0;
            pat++; s++;
        } else if (*pat == '[') {
            if (!*s) return 0;
            const char *set = ++pat;
            int matched = 0;
            while (*pat && *pat != ']') {
                if (*pat == *s) matched = 1;
                pat++;
            }
            if (*pat == ']') pat++;
            if (set == pat - 1) { /* empty "[]": treat '[' as a literal */
                if (*s != '[') return 0;
            } else if (!matched) {
                return 0;
            }
            s++;
        } else {
            if (*pat != *s) return 0;
            pat++; s++;
        }
    }
    return *s == '\0';
}

/* The literal name a [xX]-style bracket pattern would have if every bracket
   picked its first option - that's always the lowercase one, since dirs.c
   always emits [lower][upper] (see this file's own comment above), and
   everything we ever upload to this console uses lowercase directory
   names. Used to avoid opendir()-and-scan entirely for the common case. */
static void ps5_literal_guess(const char *comp, char *out, size_t out_sz) {
    size_t oi = 0;
    const char *p = comp;
    while (*p && oi + 1 < out_sz) {
        if (*p == '[') {
            p++;
            if (*p && *p != ']') { out[oi++] = *p; p++; }
            while (*p && *p != ']') p++;
            if (*p == ']') p++;
        } else {
            out[oi++] = *p++;
        }
    }
    out[oi] = '\0';
}

/* Resolves one path component (which may itself be a [xX]-style pattern)
   against a real, already-resolved directory. "." means the current
   directory. Writes the real on-disk name into out (size out_sz) and
   returns 1, or returns 0 if resolving fails (can't open curdir, or no
   entry matches). Tries the lowercase literal guess directly first -
   needed because this payload SDK's sandbox refuses to opendir() this
   app's own root ("/app0", mapped from "."), errno=1 (EPERM, not a missing
   path): there's no way to *list* that one directory and find "gfx" by
   scanning, only to open it directly once its real name is known. Ordinary
   subdirectories (anything past the first component) don't have this
   restriction, but trying the cheap guess first before scanning is a
   harmless, if also hard-coding what the app actually uploads lowercase. */
/* Does this pattern have a real wildcard ('*' or '?') OUTSIDE any [...]
   bracket group? dirs.c's own case-insensitivity brackets never need real
   enumeration - only a pattern someone wrote with an actual '*'/'?' does. */
static int ps5_pattern_has_real_wildcard(const char *pat) {
    while (*pat) {
        if (*pat == '[') {
            pat++;
            while (*pat && *pat != ']') pat++;
            if (*pat == ']') pat++;
        } else if (*pat == '*' || *pat == '?') {
            return 1;
        } else {
            pat++;
        }
    }
    return 0;
}

static int ps5_resolve_one(const char *curdir, const char *comp, char *out, size_t out_sz) {
    char guess[256];
    ps5_literal_guess(comp, guess, sizeof guess);
    char guess_path[1024];
    if (!strcmp(curdir, "."))
        snprintf(guess_path, sizeof guess_path, "%s", guess);
    else
        snprintf(guess_path, sizeof guess_path, "%s/%s", curdir, guess);
    DIR *gd = opendir(guess_path);
    if (gd) {
        closedir(gd);
        snprintf(out, out_sz, "%s", guess);
        return 1;
    }

    /* opendir() is refused (EPERM) on EVERY directory under /app0 on this
       payload SDK's sandbox - confirmed on real hardware for both the app
       root and ordinary subdirectories (e.g. "/app0/gfx"), not just the
       root as first suspected. That means a failed opendir() here carries
       no information about whether the guessed name is right - it fails
       the same way whether the directory exists or not. Since dirs.c's
       brackets are case-insensitivity only (no real wildcard), and every
       asset this app ships is uploaded in lowercase, just trust the guess:
       the alternative (a directory scan via readdir()) is categorically
       unavailable here anyway. Only fall back to a real scan if opendir()
       ever fails for some OTHER reason (checked below) - a difference we
       can't currently reproduce, but if it did, scanning still works. */
    if (errno == EPERM) {
        snprintf(out, out_sz, "%s", guess);
        return 1;
    }

    DIR *d = opendir(curdir);
    if (!d) return 0;
    int found = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        if (ps5_glob_match(comp, de->d_name)) {
            snprintf(out, out_sz, "%s", de->d_name);
            found = 1;
            break;
        }
    }
    closedir(d);
    return found;
}


/* ---- Directory listing fallbacks (opendir() on /app0 is refused with EPERM
   by this sandbox, even for directories that exist). Tried in order, each
   attempt logged to /app0/data/paths.log so the working one is visible:
   1. open()+fdopendir() on the /app0 path
   2. the title's real directory /data/homebrew/<TITLEID>/<dir> (title id read
      from /app0/sce_sys/param.json)
   3. "<dir>/.index": plain text, one file name per line, written at upload
      time. */
#include <fcntl.h>
#include <stdarg.h>
static void ps5_plog(const char *fmt, ...) {
    FILE *g = fopen("/app0/data/paths.log", "ab");
    if (!g) return;
    va_list ap; va_start(ap, fmt); vfprintf(g, fmt, ap); va_end(ap);
    fclose(g);
}
static int ps5_title_id(char *out, size_t sz) {
    FILE *f = fopen("/app0/sce_sys/param.json", "rb");
    if (!f) return 0;
    char buf[2048]; size_t n = fread(buf, 1, sizeof buf - 1, f); fclose(f);
    buf[n] = 0;
    char *t = strstr(buf, "\"titleId\"");
    if (!t) return 0;
    t = strchr(t + 9, ':'); if (!t) return 0;
    t = strchr(t, '"'); if (!t) return 0;
    t++;
    size_t i = 0;
    while (*t && *t != '"' && i + 1 < sz) out[i++] = *t++;
    out[i] = 0;
    return i > 0;
}
/* Returns a malloc'd NULL-terminated name list (excluding . and ..), or NULL
   if every method failed. *count gets the number of names. */
static char **ps5_list_dir(const char *dir, size_t *count) {
    char path[1024];
    DIR *d = NULL;
    snprintf(path, sizeof path, "/app0/%s", strcmp(dir, ".") ? dir : "");
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        d = fdopendir(fd);
        ps5_plog("list: fdopendir %s -> %s\n", path, d ? "OK" : "FAIL");
        if (!d) close(fd);
    } else {
        ps5_plog("list: open(%s) FAIL errno=%d\n", path, errno);
    }
    if (!d) {
        char tid[32];
        if (ps5_title_id(tid, sizeof tid)) {
            snprintf(path, sizeof path, "/data/homebrew/%s/%s", tid, strcmp(dir, ".") ? dir : "");
            d = opendir(path);
            ps5_plog("list: opendir %s -> %s\n", path, d ? "OK" : "FAIL");
        } else {
            ps5_plog("list: no title id (%s)\n", dir);
        }
    }
    size_t cap = 16, n = 0;
    char **names = (char **)malloc(cap * sizeof(char *));
    if (!names) { if (d) closedir(d); return NULL; }
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
            if (n + 2 >= cap) { cap *= 2; names = (char **)realloc(names, cap * sizeof(char *)); if (!names) break; }
            names[n++] = strdup(de->d_name);
        }
        closedir(d);
        names[n] = NULL; *count = n;
        return names;
    }
    snprintf(path, sizeof path, "/app0/%s/.index", strcmp(dir, ".") ? dir : "");
    FILE *f = fopen(path, "rb");
    if (!f) { ps5_plog("list: %s missing\n", path); free(names); return NULL; }
    char line[512];
    while (fgets(line, sizeof line, f)) {
        size_t l = strlen(line);
        while (l && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = 0;
        if (!l) continue;
        if (n + 2 >= cap) { cap *= 2; names = (char **)realloc(names, cap * sizeof(char *)); }
        names[n++] = strdup(line);
    }
    fclose(f);
    ps5_plog("list: %s -> %d names\n", path, (int)n);
    names[n] = NULL; *count = n;
    return names;
}

/* Absolute patterns ("/data/", "/mnt/usb0/" + "*.png"): the real file system, no /app0 mapping and
   no case-insensitive component resolution. Used by file-browser programs once the sandbox allows it. */
int sceKernelGetdents(int fd, char *buf, int nbytes);
static void ps5_plog(const char *fmt, ...);

static int ps5_dir_exists(const char *path) {
    DIR *t = opendir(path);
    if (t) { closedir(t); return 1; }
    int fd = open(path, O_RDONLY);
    if (fd >= 0) { close(fd); return 1; }
    return 0;
}

/* Resolves one path component that may be a [xX]-style case-insensitivity pattern against the real
   directory 'curdir' (absolute). The lowercase literal first; otherwise list curdir and match. */
static int ps5_resolve_abs(const char *curdir, const char *comp, char *out, size_t out_sz) {
    if (!strchr(comp, '[')) { snprintf(out, out_sz, "%s", comp); return 1; }
    char guess[256], path[1024];
    ps5_literal_guess(comp, guess, sizeof guess);
    snprintf(path, sizeof path, "%s%s%s", curdir, (curdir[0] == '/' && curdir[1] == 0) ? "" : "/", guess);
    if (ps5_dir_exists(path)) { snprintf(out, out_sz, "%s", guess); return 1; }
    DIR *d = opendir(curdir);
    if (!d) { snprintf(out, out_sz, "%s", guess); return 1; }     /* cannot list: trust the guess */
    int found = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        if (ps5_glob_match(comp, de->d_name)) { snprintf(out, out_sz, "%s", de->d_name); found = 1; break; }
    }
    closedir(d);
    return found;
}

static int ps5_glob_abs(const char *pattern, int flags, glob_t *pglob) {
    char dirpat[1024], dirpart[1024];
    const char *last = strrchr(pattern, '/');
    size_t dl = (size_t)(last - pattern);
    if (dl >= sizeof dirpat) dl = sizeof dirpat - 1;
    memcpy(dirpat, pattern, dl); dirpat[dl] = 0;
    const char *base = last[1] ? last + 1 : "*";

    /* resolve the directory part component by component */
    snprintf(dirpart, sizeof dirpart, "/");
    const char *p = dirpat;
    pglob->gl_pathc = pglob->gl_matchc = 0;
    pglob->gl_pathv = NULL; pglob->gl_offs = 0; pglob->gl_flags = flags;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *e = strchr(p, '/');
        size_t cl = e ? (size_t)(e - p) : strlen(p);
        char comp[256], real[256];
        if (cl >= sizeof comp) cl = sizeof comp - 1;
        memcpy(comp, p, cl); comp[cl] = 0;
        if (!ps5_resolve_abs(dirpart, comp, real, sizeof real)) return (flags & GLOB_ERR) ? GLOB_ABORTED : GLOB_NOMATCH;
        size_t cur = strlen(dirpart);
        snprintf(dirpart + cur, sizeof dirpart - cur, "%s%s", (cur == 1) ? "" : "/", real);
        p += cl;
    }

    /* names of the directory: readdir() first, then raw getdents() on an open() descriptor (the sandbox
       refuses opendir() on some directories that open() still reaches). Every step is logged. */
    size_t ncap = 64, nn = 0;
    char **nm = (char **)malloc(ncap * sizeof(char *));
    if (!nm) return GLOB_NOSPACE;
    int ok = 0;
    DIR *d = opendir(dirpart);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
            if (nn + 1 >= ncap) { ncap *= 2; char **g = (char **)realloc(nm, ncap * sizeof(char *)); if (!g) break; nm = g; }
            nm[nn++] = strdup(de->d_name);
        }
        closedir(d);
        ok = 1;
    } else {
        int e1 = errno;
        int fd = open(dirpart, O_RDONLY);
        int e2 = errno;
        if (fd < 0) {
            ps5_plog("abs: %s opendir errno=%d open errno=%d\n", dirpart, e1, e2);
        } else {
            static char buf[32768];
            int total = 0, rc;
            long base = 0;
            while ((rc = sceKernelGetdents(fd, buf, sizeof buf)) > 0) {
                total += rc;
                for (int off = 0; off + 8 <= rc; ) {
                    /* struct dirent (FreeBSD 9 layout): d_fileno u32, d_reclen u16, d_type u8, d_namlen u8, d_name[] */
                    unsigned short reclen = *(unsigned short *)(buf + off + 4);
                    unsigned char namlen = *(unsigned char *)(buf + off + 7);
                    const char *name = buf + off + 8;
                    if (reclen < 8 || off + reclen > rc) break;
                    if (namlen && !(namlen == 1 && name[0] == '.') && !(namlen == 2 && name[0] == '.' && name[1] == '.')) {
                        if (nn + 1 >= ncap) { ncap *= 2; char **g = (char **)realloc(nm, ncap * sizeof(char *)); if (!g) break; nm = g; }
                        nm[nn++] = strndup(name, namlen);
                    }
                    off += reclen;
                }
            }
            ps5_plog("abs: %s opendir errno=%d, getdents total=%d last rc=%d names=%d\n", dirpart, e1, total, rc, (int)nn);
            close(fd);
            ok = total > 0 || rc == 0;
        }
        (void)base;
    }
    if (!ok) { free(nm); return (flags & GLOB_ERR) ? GLOB_ABORTED : GLOB_NOMATCH; }

    size_t cap = 32, n = 0;
    char **paths = (char **)malloc(cap * sizeof(char *));
    if (!paths) { for (size_t i = 0; i < nn; i++) free(nm[i]); free(nm); return GLOB_NOSPACE; }
    for (size_t k = 0; k < nn; k++) {
        if (ps5_glob_match(base, nm[k])) {
            if (n + 2 >= cap) { cap *= 2; char **g = (char **)realloc(paths, cap * sizeof(char *)); if (!g) break; paths = g; }
            size_t fl = strlen(dirpart) + 1 + strlen(nm[k]) + 1;
            char *full = (char *)malloc(fl);
            if (!full) break;
            snprintf(full, fl, "%s%s%s", dirpart, (dirpart[0] == '/' && dirpart[1] == 0) ? "" : "/", nm[k]);
            paths[n++] = full;
        }
        free(nm[k]);
    }
    free(nm);
    paths[n] = NULL;
    if (n == 0) { free(paths); return GLOB_NOMATCH; }
    pglob->gl_pathc = pglob->gl_matchc = n;
    pglob->gl_pathv = paths;
    return 0;
}

int glob(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *pglob) {
    (void)errfunc;
    if (pattern && pattern[0] == '/') return ps5_glob_abs(pattern, flags, pglob);
    char resolved[1024] = "";   /* real resolved directory so far; "" means "." */
    char comp[256];
    const char *p = pattern;
    const char *slash;
    int had_any_slash = (strchr(pattern, '/') != NULL);

    /* Resolve every component up to (not including) the final one. */
    while ((slash = strchr(p, '/')) != NULL) {
        size_t clen = (size_t)(slash - p);
        if (clen >= sizeof comp) clen = sizeof comp - 1;
        memcpy(comp, p, clen);
        comp[clen] = '\0';

        const char *curdir = resolved[0] ? resolved : ".";
        char found[256];
        if (!ps5_resolve_one(curdir, comp, found, sizeof found)) {
            pglob->gl_pathc = pglob->gl_matchc = 0;
            pglob->gl_pathv = NULL;
            return (flags & GLOB_ERR) ? GLOB_ABORTED : GLOB_NOMATCH;
        }
        char grown[1024];
        if (resolved[0]) snprintf(grown, sizeof grown, "%s/%s", resolved, found);
        else snprintf(grown, sizeof grown, "%s", found);
        memcpy(resolved, grown, sizeof resolved);

        p = slash + 1;
    }

    const char *base = *p ? p : "*"; /* trailing '/': list everything inside */
    const char *dirpart = resolved[0] ? resolved : ".";

    size_t nnames = 0;
    char **names = ps5_list_dir(dirpart, &nnames);
    if (!names) {
        /* Listing impossible. A pattern that is only case-insensitivity
           brackets needs no listing: report the lowercase guess. */
        if (!ps5_pattern_has_real_wildcard(base)) {
            char guess[256];
            ps5_literal_guess(base, guess, sizeof guess);
            char **paths = (char **)malloc(2 * sizeof(char *));
            if (!paths) return GLOB_NOSPACE;
            size_t full_len = strlen(dirpart) + 1 + strlen(guess) + 1;
            char *full = (char *)malloc(full_len);
            if (!full) { free(paths); return GLOB_NOSPACE; }
            if (!had_any_slash) snprintf(full, full_len, "%s", guess);
            else snprintf(full, full_len, "%s/%s", dirpart, guess);
            paths[0] = full; paths[1] = NULL;
            pglob->gl_pathc = pglob->gl_matchc = 1;
            pglob->gl_offs = 0; pglob->gl_flags = flags; pglob->gl_pathv = paths;
            return 0;
        }
        pglob->gl_pathc = pglob->gl_matchc = 0;
        pglob->gl_pathv = NULL;
        return (flags & GLOB_ERR) ? GLOB_ABORTED : GLOB_NOMATCH;
    }

    size_t cap = 16, n = 0;
    char **paths = (char **)malloc(cap * sizeof(char *));
    if (!paths) { for (size_t i = 0; i < nnames; i++) free(names[i]); free(names); return GLOB_NOSPACE; }

    int out_of_memory = 0;
    for (size_t k = 0; k < nnames && !out_of_memory; k++) {
        if (!ps5_glob_match(base, names[k])) continue;
        if (n + 1 >= cap) {
            cap *= 2;
            char **grown = (char **)realloc(paths, cap * sizeof(char *));
            if (!grown) { out_of_memory = 1; break; }
            paths = grown;
        }
        size_t full_len = strlen(dirpart) + 1 + strlen(names[k]) + 1;
        char *full = (char *)malloc(full_len);
        if (!full) { out_of_memory = 1; break; }
        if (!had_any_slash) snprintf(full, full_len, "%s", names[k]);
        else snprintf(full, full_len, "%s/%s", dirpart, names[k]);
        paths[n++] = full;
    }
    for (size_t i = 0; i < nnames; i++) free(names[i]);
    free(names);

    if (out_of_memory) {
        for (size_t i = 0; i < n; i++) free(paths[i]);
        free(paths);
        return GLOB_NOSPACE;
    }

    paths[n] = NULL;
    pglob->gl_pathc = pglob->gl_matchc = n;
    pglob->gl_offs = 0;
    pglob->gl_flags = flags;
    pglob->gl_pathv = paths;

    if (n == 0) {
        free(paths);
        pglob->gl_pathv = NULL;
        return GLOB_NOMATCH;
    }
    return 0;
}

void globfree(glob_t *pglob) {
    if (!pglob || !pglob->gl_pathv) return;
    for (size_t i = 0; i < pglob->gl_pathc; i++) free(pglob->gl_pathv[i]);
    free(pglob->gl_pathv);
    pglob->gl_pathv = NULL;
    pglob->gl_pathc = 0;
}

/* ---- libc gaps pulled in by FFmpeg's dependencies (OpenSSL, iconv, lzma) ----
 * Local video playback never reaches any of these; they exist so the static
 * link resolves. Where a real answer is cheap (time, locale) it is real. */
#include <time.h>
#include <errno.h>
#include <sys/types.h>

/* OpenSSL's async-job fibres (never used): fail loudly if something ever does. */
int _setjmp(void *env) { (void)env; return 0; }
void _longjmp(void *env, int val) { (void)env; (void)val; _Exit(134); }

struct mmsghdr;
ssize_t sendmmsg(int s, struct mmsghdr *msgvec, size_t vlen, int flags) { (void)s; (void)msgvec; (void)vlen; (void)flags; errno = ENOSYS; return -1; }
ssize_t recvmmsg(int s, struct mmsghdr *msgvec, size_t vlen, int flags, const struct timespec *timeout) { (void)s; (void)msgvec; (void)vlen; (void)flags; (void)timeout; errno = ENOSYS; return -1; }

char *nl_langinfo(int item) { (void)item; return (char *)"UTF-8"; }
int ___mb_cur_max(void) { return 1; }

struct tm *gmtime_r(const time_t *timep, struct tm *r) {
    long long t = (long long)*timep;
    long long days = t / 86400, rem = t % 86400;
    if (rem < 0) { rem += 86400; days--; }
    r->tm_hour = (int)(rem / 3600); r->tm_min = (int)(rem % 3600 / 60); r->tm_sec = (int)(rem % 60);
    r->tm_wday = (int)((days + 4) % 7); if (r->tm_wday < 0) r->tm_wday += 7;
    long long z = days + 719468;                       /* civil-from-days (H. Hinnant) */
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    long long doe = z - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y = yoe + era * 400;
    long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long long mp = (5 * doy + 2) / 153;
    int d = (int)(doy - (153 * mp + 2) / 5 + 1);
    int m = (int)(mp < 10 ? mp + 3 : mp - 9);
    if (m <= 2) y++;
    r->tm_year = (int)(y - 1900); r->tm_mon = m - 1; r->tm_mday = d;
    static const int cum[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    int leap = ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0);
    r->tm_yday = cum[m - 1] + d - 1 + (leap && m > 2 ? 1 : 0);
    r->tm_isdst = 0;
    return r;
}
struct tm *localtime_r(const time_t *timep, struct tm *r) { return gmtime_r(timep, r); }   /* no timezone database on the console */

int dladdr(const void *addr, void *info) { (void)addr; (void)info; return 0; }

/* ---- libScePosixForWebKit.so's own private stubs for the six names below are the sole
 * reason __wrap_getcwd/__wrap_lstat used to leak into the dynamic symbol table (ps5_paths.c's
 * __wrap_<name> collides with that .so's identically-named __wrap_<name> export, and lld must
 * keep a symbol dynamically visible whenever a needed .so defines the same name, breaking the
 * native converter's "no application exports" check - no renaming avoids a same-name clash).
 * Dropping libScePosixForWebKit.so as a needed library removes that clash, but FFmpeg's
 * dependencies (OpenSSL/avformat's network protocol handlers) still reference these six names
 * at link time even though local video playback - the only thing libmod_video exercises - never
 * calls into any of them. Same category as the OpenSSL/iconv/lzma gaps above: real where cheap,
 * a loud failure otherwise. */
int isatty(int fd) { (void)fd; return 0; }   /* never a terminal on a PS5 title */

#include <stdio.h>
int mkstemp(char *name_template) {
    size_t len = strlen(name_template);
    if (len < 6 || strcmp(name_template + len - 6, "XXXXXX") != 0) { errno = EINVAL; return -1; }
    static unsigned counter;
    for (int tries = 0; tries < 1000; tries++) {
        unsigned v = (unsigned)time(NULL) ^ (counter++ * 2654435761u);
        for (int i = 0; i < 6; i++) {
            static const char alnum[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
            name_template[len - 6 + i] = alnum[(v >> (i * 5)) % 62];
        }
        int fd = open(name_template, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) return fd;
        if (errno != EEXIST) return -1;
    }
    errno = EEXIST;
    return -1;
}

#include <ctype.h>
char *strcasestr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) { h++; n++; }
        if (!*n) return (char *)haystack;
    }
    return NULL;
}

/* arc4random_buf: only reached by OpenSSL's RNG seeding for network-protocol TLS, never by
 * local video decode - but seed it with real entropy anyway (cheap and harmless) rather than
 * a fixed stub, by mixing clock_gettime with an address (ASLR) and a free-running counter. */
void arc4random_buf(void *buf, size_t n) {
    static unsigned long long state;
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    if (!state) state = (unsigned long long)ts.tv_nsec ^ (unsigned long long)(uintptr_t)&state;
    unsigned char *p = (unsigned char *)buf;
    for (size_t i = 0; i < n; i++) {
        state ^= state << 13; state ^= state >> 7; state ^= state << 17;
        p[i] = (unsigned char)(state ^ (unsigned long long)ts.tv_nsec >> (i % 8));
    }
}

/* gethostbyname/getnameinfo: only FFmpeg's network protocol handlers (rtsp://, http://, ...)
 * would call these - local video/audio file playback (the only thing libmod_video exercises on
 * this console) never does. Fail loudly instead of silently misresolving a hostname. */
#include <sys/socket.h>
#include <netdb.h>
struct hostent *gethostbyname(const char *name) { (void)name; h_errno = HOST_NOT_FOUND; return NULL; }
int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, size_t hostlen,
                char *serv, size_t servlen, int flags) {
    (void)sa; (void)salen; (void)host; (void)hostlen; (void)serv; (void)servlen; (void)flags;
    return EAI_NONAME;
}
