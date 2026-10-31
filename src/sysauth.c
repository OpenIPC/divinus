#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "sysauth.h"

/*
 * System-account login: with the configured password set to "@system", a
 * "Basic" credential is checked against the device's own accounts in
 * /etc/shadow (the SSH login), so no password has to be kept in the config.
 *
 * Called from the HTTP and RTSP threads alike, so the cache and crypt() (not
 * reentrant) are kept under one lock. The last good credential is remembered
 * for a few minutes and only while /etc/shadow is unchanged, so a password
 * change takes effect. After a failure, new credentials are refused for a
 * second without checking, instead of sleeping in the caller's server loop.
 */
#define SYSAUTH_CACHE_S 300

static pthread_mutex_t authMtx = PTHREAD_MUTEX_INITIALIZER;
static char okCred[160];
static time_t okAt, failAt;
static struct stat okShadow;

static time_t sysauth_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec;
}

static int sysauth_check_locked(const char *b64) {
    static char *(*cryptFn)(const char*, const char*);
    char raw[128], cred[160], line[256];
    unsigned int n = 0, bits = 0, acc = 0, len = 0;
    struct stat st;
    time_t now = sysauth_now();

    while (b64[len] && b64[len] != '\r' && b64[len] != '\n' && b64[len] != ' ' && len < sizeof(cred) - 1)
        { cred[len] = b64[len]; len++; }
    cred[len] = 0;
    if (!len) return 0;
    if (stat("/etc/shadow", &st)) return 0;
    if (now - okAt >= SYSAUTH_CACHE_S ||
        st.st_ino != okShadow.st_ino || st.st_mtime != okShadow.st_mtime) *okCred = 0;
    if (*okCred && !strcmp(cred, okCred)) return 1;
    if (failAt && now - failAt < 1) return 0;

    for (unsigned int i = 0; i < len && cred[i] != '=' && n < sizeof(raw) - 1; i++) {
        char c = cred[i];
        int v = c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 :
            c >= '0' && c <= '9' ? c - '0' + 52 : c == '+' ? 62 : c == '/' ? 63 : -1;
        if (v < 0) return 0;
        acc = acc << 6 | v; bits += 6;
        if (bits >= 8) { bits -= 8; raw[n++] = acc >> bits & 0xFF; }
    }
    raw[n] = 0;
    char *pass = strchr(raw, ':');
    if (!pass || pass == raw) return 0;
    *pass++ = 0;

    if (!cryptFn) {
        void *lib = dlopen("libcrypt.so.1", RTLD_NOW);
        cryptFn = dlsym(lib ? lib : RTLD_DEFAULT, "crypt");
        if (!cryptFn) return 0;
    }
    int ok = 0;
    FILE *sh = fopen("/etc/shadow", "r");
    if (!sh) return 0;
    while (fgets(line, sizeof(line), sh)) {
        char *hash = strchr(line, ':');
        if (!hash) continue;
        *hash++ = 0;
        if (strcmp(line, raw)) continue;
        char *end = strchr(hash, ':');
        if (end) *end = 0;
        if (strlen(hash) < 4 || hash[0] == '!' || hash[0] == '*') break; // no usable password
        char *calc = cryptFn(pass, hash);
        ok = calc && !strcmp(calc, hash);
        break;
    }
    fclose(sh);
    memset(raw, 0, sizeof(raw));
    if (ok) {
        strcpy(okCred, cred);
        okAt = now;
        okShadow = st;
    } else failAt = now;
    return ok;
}

int sysauth_check_basic(const char *b64) {
    pthread_mutex_lock(&authMtx);
    int ok = sysauth_check_locked(b64);
    pthread_mutex_unlock(&authMtx);
    return ok;
}
