#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sysauth.h"

/*
 * System-account login: with the configured password set to "@system", a
 * "Basic" credential is checked against the device's own accounts in
 * /etc/shadow (the SSH login), so no password has to be kept in the config.
 */
int sysauth_check_basic(const char *b64) {
    static char okCred[160];
    static char *(*cryptFn)(const char*, const char*);
    char raw[128], cred[160], line[256];
    unsigned int n = 0, bits = 0, acc = 0, len = 0;

    while (b64[len] && b64[len] != '\r' && b64[len] != '\n' && b64[len] != ' ' && len < sizeof(cred) - 1)
        { cred[len] = b64[len]; len++; }
    cred[len] = 0;
    if (!len) return 0;
    if (*okCred && !strcmp(cred, okCred)) return 1; // already verified this session

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
    if (ok) strcpy(okCred, cred);
    else usleep(500000); // slow down guessing
    return ok;
}
