#include "soap.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal/tools.h"

static const char *soap_name_end(const char *name) {
    while (*name && !isspace((unsigned char)*name) && *name != '>' && *name != '/')
        name++;
    return name;
}

const char *soap_tag(const char *xml, const char *tag) {
    size_t len = strlen(tag);

    for (const char *pos = strchr(xml, '<'); pos; pos = strchr(pos + 1, '<')) {
        const char *name = pos + 1, *end;
        if (*name == '/' || *name == '?' || *name == '!') continue;
        end = soap_name_end(name);
        const char *colon = memchr(name, ':', end - name);
        if (colon) name = colon + 1;
        if (end - name == len && !strncmp(name, tag, len))
            return pos;
    }

    return NULL;
}

bool soap_tag_text(const char *xml, const char *tag, char *text, size_t size) {
    const char *start = soap_tag(xml, tag), *end;

    if (!start || !(start = strchr(start, '>'))) return false;
    if (start[-1] == '/') {
        *text = '\0';
        return true;
    }
    start++;
    if (!(end = strstr(start, "</")) || end - start >= size) return false;

    memcpy(text, start, end - start);
    text[end - start] = '\0';
    return true;
}

bool soap_tag_attr_has(const char *xml, const char *tag, const char *needle) {
    const char *start = soap_tag(xml, tag), *end, *hit;

    if (!start || !(end = strchr(start, '>'))) return false;
    hit = strstr(start, needle);
    return hit && hit < end;
}

bool soap_action(const char *xml, char *action, size_t size) {
    const char *pos = soap_tag(xml, "Body"), *end;

    if (!pos || !(pos = strchr(pos, '>'))) return false;
    while (*++pos && isspace((unsigned char)*pos));
    if (*pos != '<') return false;

    end = soap_name_end(++pos);
    const char *colon = memchr(pos, ':', end - pos);
    if (colon) pos = colon + 1;
    if (end == pos || end - pos >= size) return false;

    memcpy(action, pos, end - pos);
    action[end - pos] = '\0';
    return true;
}

int soap_duration(const char *text) {
    long total = 0, value;
    bool clock = false;
    char *end;

    if (*text++ != 'P') return -1;

    while (*text) {
        if (*text == 'T') {
            clock = true;
            text++;
            continue;
        }
        value = strtol(text, &end, 10);
        if (end == text || value < 0) return -1;
        if (*end == '.')
            while (isdigit((unsigned char)*++end));
        switch (*end) {
            case 'D': if (clock) return -1; total += value * 86400; break;
            case 'H': if (!clock) return -1; total += value * 3600; break;
            case 'M': if (!clock) return -1; total += value * 60; break;
            case 'S': if (!clock) return -1; total += value; break;
            default: return -1;
        }
        if (total > INT_MAX) return -1;
        text = end + 1;
    }

    return total;
}

time_t soap_datetime(const char *text) {
    struct tm tm = {0};

    if (sscanf(text, "%4d-%2d-%2dT%2d:%2d:%2d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
        &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) return -1;
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    return timegm(&tm);
}

void soap_datetime_format(time_t when, char *text, size_t size) {
    struct tm tm;

    gmtime_r(&when, &tm);
    strftime(text, size, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

int soap_term_seconds(const char *xml, const char *tag, time_t now, int fallback, int max) {
    char text[64];
    long seconds;

    if (!soap_tag_text(xml, tag, text, sizeof(text))) return fallback;

    if (*text == 'P')
        seconds = soap_duration(text);
    else {
        time_t when = soap_datetime(text);
        seconds = when < 0 ? -1 : when - now;
        if (when >= 0 && seconds < 1) seconds = 1;
    }

    if (seconds < 0) return fallback;
    if (seconds < 1) return 1;
    return seconds > max ? max : seconds;
}

bool soap_digest_valid(const char *nonce, const char *created, const char *pass, const char *digest) {
    char nonceDec[64], expected[32];
    unsigned char hash[SHA1_DIGEST_SIZE];
    sha1_context ctx;

    int nonceLen = base64_decode(nonceDec, nonce, sizeof(nonceDec));
    if (nonceLen <= 0) return false;

    sha1_init(&ctx);
    sha1_update(&ctx, (unsigned char *)nonceDec, nonceLen);
    sha1_update(&ctx, (unsigned char *)created, strlen(created));
    sha1_update(&ctx, (unsigned char *)pass, strlen(pass));
    sha1_final(hash, &ctx);

    base64_encode(expected, (char *)hash, SHA1_DIGEST_SIZE);
    return !strcmp(digest, expected);
}
