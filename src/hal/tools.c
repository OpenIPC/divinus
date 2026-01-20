#include "tools.h"

static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int base64_decode(char *decoded, const char *string, int maxLen) {
    char buf[3];
    int buflen = 0, i = 0, v;

    while (*string && *string != '=' && i < maxLen) {
        const char *p = strchr(base64_table, *string++);
        if (!p) continue;
        
        v = p - base64_table;
        switch (buflen) {
            case 0:
                buf[buflen++] = v << 2;
                break;
            case 1:
                buf[buflen - 1] |= v >> 4;
                buf[buflen++] = (v & 0xF) << 4;
                break;
            case 2:
                buf[buflen - 1] |= v >> 2;
                buf[buflen++] = (v & 0x3) << 6;
                break;
            case 3:
                buf[buflen - 1] |= v;
                if (i + 3 <= maxLen) {
                    memcpy(decoded + i, buf, 3);
                    i += 3;
                }
                buflen = 0;
                break;
        }
    }

    if (buflen > 0 && i + buflen <= maxLen) {
        memcpy(decoded + i, buf, buflen);
        i += buflen;
    }

    return i;
}

int base64_encode_length(int len) { return ((len + 2) / 3 * 4) + 1; }

int base64_encode(char *encoded, const char *string, int len) {
    int i;
    char *p;

    p = encoded;
    for (i = 0; i < len - 2; i += 3) {
        *p++ = base64_table[(string[i] >> 2) & 0x3F];
        *p++ = base64_table
            [((string[i] & 0x3) << 4) | ((int)(string[i + 1] & 0xF0) >> 4)];
        *p++ = base64_table
            [((string[i + 1] & 0xF) << 2) | ((int)(string[i + 2] & 0xC0) >> 6)];
        *p++ = base64_table[string[i + 2] & 0x3F];
    }
    if (i < len) {
        *p++ = base64_table[(string[i] >> 2) & 0x3F];
        if (i == (len - 1)) {
            *p++ = base64_table[((string[i] & 0x3) << 4)];
            *p++ = '=';
        } else {
            *p++ = base64_table
                [((string[i] & 0x3) << 4) | ((int)(string[i + 1] & 0xF0) >> 4)];
            *p++ = base64_table[((string[i + 1] & 0xF) << 2)];
        }
        *p++ = '=';
    }

    *p++ = '\0';
    return p - encoded;
}

int color_convert555(int color) {
    unsigned char r = (color >> 16) & 0xF8;
    unsigned char g = (color >> 8) & 0xF8;
    unsigned char b = color & 0xF8;
    return (1 << 15) | (r << 7) | (g << 2) | (b >> 3);
}

int color_parse(const char *str) {
    char *remain = NULL;
    int len = strlen(str);
    int result = 0;
    
    // Ignore leading spaces
    while (*str == ' ' || *str == '\t') str++;
    
    // 4-bit hex format "#RGB"
    if (str[0] == '#' && len == 4) {
        int r = hex_to_int(str[1]);
        int g = hex_to_int(str[2]);
        int b = hex_to_int(str[3]);
        
        if (r >= 0 && g >= 0 && b >= 0)
            return (1 << 16) | (r << 11) | (g << 6) | (b << 1);
    }
    
    // 8-bit hex format "#RRGGBB"
    else if (str[0] == '#') {
        result = strtol(str + 1, &remain, 16);
        if (remain != str + 1 && *remain == '\0')
            return color_convert555(result);
    }
    
    // C standard format "0xRRGGBB"
    else if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        result = strtol(str, &remain, 0);
        if (remain != str && *remain == '\0')
            return color_convert555(result);
    }
    
    // No prefix format "RRGGBB"
    else if (len >= 6) {
        result = strtol(str, &remain, 16);
        if (remain != str && *remain == '\0')
            return color_convert555(result);
    }

    return 0xFFFF;
}


#define REG_ERR_LEN 0x1000

int compile_regex(regex_t *r, const char *regex_text) {
    int status = regcomp(r, regex_text, REG_EXTENDED | REG_NEWLINE | REG_ICASE);
    if (status != 0) {
        char error_message[REG_ERR_LEN];
        regerror(status, r, error_message, REG_ERR_LEN);
        printf("Error compiling regex '%s': %s\n", regex_text, error_message);
        fflush(stdout);
        return -1;
    }
    return 1;
}


int escape_url(char *dst, const char *src, size_t maxlen) {
    static const char hex[] = "0123456789ABCDEF";
    int len = 0;
    
    if (!dst || !src || !maxlen)
        return 0;
        
    while (*src && len < maxlen - 1) {
        unsigned char c = *src;

        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            *dst++ = c;
            len++;
        } else {
            if (len + 3 > maxlen - 1)
                break;
                
            *dst++ = '%';
            *dst++ = hex[c >> 4];
            *dst++ = hex[c & 0xF];
            len += 3;
        }
        
        src++;
    }
    
    *dst = '\0';
    return len;
}


void generate_nonce(char *nonce, size_t len) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    if (len < 2) return;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    unsigned int seed = ts.tv_nsec;

    for (size_t i = 0; i < len - 1; i++)
        nonce[i] = charset[rand_r(&seed) % (sizeof(charset) - 1)];

    nonce[len - 1] = '\0';
}


const char *get_extension(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot || dot == path)
        return "";
    return dot + 1;
}


bool get_uint64(char *str, char *pattern, uint64_t *value) {
    char reg_buf[128];
    ssize_t reg_buf_len = sprintf(reg_buf, "%s([0-9]+)", pattern);
    reg_buf[reg_buf_len] = 0;

    regex_t regex;
    if (compile_regex(&regex, reg_buf) < 0) {
        printf("get_uint64 compile_regex error\n");
        return false;
    };
    size_t n_matches = 2; // We have 1 capturing group + the whole match group
    regmatch_t m[n_matches];

    char value_str[16] = {0};
    int match = regexec(&regex, str, n_matches, m, 0);
    if (match != 0)
        return false;
    int len = sprintf(
        value_str, "%.*s", (int)(m[1].rm_eo - m[1].rm_so), str + m[1].rm_so);
    value_str[len] = 0;

    char *pEnd;
    *value = strtoll(value_str, &pEnd, 10);
    regfree(&regex);

    return true;
}

bool get_uint32(char *str, char *pattern, uint32_t *value) {
    uint64_t val64 = 0;
    if (!get_uint64(str, pattern, &val64))
        return false;
    *value = val64;
    return true;
}

bool get_uint16(char *str, char *pattern, uint16_t *value) {
    uint64_t val64 = 0;
    if (!get_uint64(str, pattern, &val64))
        return false;
    *value = val64;
    return true;
}

bool get_uint8(char *str, char *pattern, uint8_t *value) {
    uint64_t val64 = 0;
    if (!get_uint64(str, pattern, &val64))
        return false;
    *value = val64;
    return true;
}

bool hal_registry(unsigned int addr, unsigned int *data, hal_register_op op) {
    static int mem_fd;
    static char *loaded_area;
    static unsigned int loaded_offset;
    static unsigned int loaded_size;

    unsigned int offset = addr & 0xffff0000;
    unsigned int size = 0xffff;
    if (!addr || (loaded_area && offset != loaded_offset))
        if (munmap(loaded_area, loaded_size))
            fprintf(stderr, "hal_registry munmap error: %s (%d)\n",
                strerror(errno), errno);

    if (!addr) {
        close(mem_fd);
        return true;
    }

    if (!mem_fd && (mem_fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
        fprintf(stderr, "can't open /dev/mem\n");
        return false;
    }

    volatile char *mapped_area;
    if (offset != loaded_offset) {
        mapped_area = mmap64(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, offset);
        if (mapped_area == MAP_FAILED) {
            fprintf(stderr, "hal_registry mmap error: %s (%d)\n",
                    strerror(errno), errno);
            return false;
        }
        loaded_area = (char *)mapped_area;
        loaded_size = size;
        loaded_offset = offset;
    } else
        mapped_area = loaded_area;

    if (op & OP_READ)
        *data = *(volatile unsigned int *)(mapped_area + (addr - offset));
    if (op & OP_WRITE)
        *(volatile unsigned int *)(mapped_area + (addr - offset)) = *data;

    return true;
}

int hex_to_int(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

unsigned int ip_to_int(const char *ip) {
    struct in_addr addr;

    if (!inet_aton(ip, &addr)) 
        return 0;

    return ntohl(addr.s_addr);
}

char ip_in_cidr(const char *ip, const char *cidr) {
    char network[18];
    int prefix_len = 32;

    strncpy(network, cidr, sizeof(network) - 1);
    network[sizeof(network) - 1] = '\0';

    char *slash = strchr(network, '/');
    if (slash) {
        *slash = '\0';
        prefix_len = atoi(slash + 1);
        if (prefix_len < 0 || prefix_len > 32)
            prefix_len = 32;
    }

    unsigned int ip_int = ip_to_int(ip);
    unsigned int network_int = ip_to_int(network);
    unsigned int mask = prefix_len ? ~((1 << (32 - prefix_len)) - 1) : 0;

    return (ip_int & mask) == (network_int & mask);
}

char *memstr(char *haystack, char *needle, int size, char needlesize) {
    for (char *p = haystack; p <= (haystack - needlesize + size); p++)
        if (!memcmp(p, needle, needlesize)) return p;
    return NULL;
}

unsigned int millis() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1000 + (t.tv_usec + 500) / 1000;
}

void reverse(void *arr, size_t width) {
    char *val = (char*)arr;
    char tmp;
    size_t i;

    for (i = 0; i < width / 2; i++) {
        tmp = val[i];
        val[i] = val[width - i - 1];
        val[width - i - 1] = tmp;
    }
}


void sha1_init(sha1_context *context) {
    context->state[0] = 0x67452301;
    context->state[1] = 0xEFCDAB89;
    context->state[2] = 0x98BADCFE;
    context->state[3] = 0x10325476;
    context->state[4] = 0xC3D2E1F0;
    context->count[0] = 0;
    context->count[1] = 0;
}

static void sha1_transform(unsigned int state[5], const unsigned char buffer[64]) {
    unsigned int a, b, c, d, e;
    unsigned int w[80];
    int i;

    // Break chunk into 16 words (big-endian)
    for (i = 0; i < 16; i++) {
        w[i]  = ((unsigned int)buffer[i * 4 + 0] << 24);
        w[i] |= ((unsigned int)buffer[i * 4 + 1] << 16);
        w[i] |= ((unsigned int)buffer[i * 4 + 2] <<  8);
        w[i] |= ((unsigned int)buffer[i * 4 + 3]);
    }

    // Extend the 16 words into 80
    for (i = 16; i < 80; i++) {
        w[i] = (w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]);
        w[i] = (w[i] << 1) | (w[i] >> 31);
    }

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];

    for (i = 0; i < 20; i++) {
        unsigned int temp = ((a << 5) | (a >> 27)) + 
            ((b & c) | ((~b) & d)) + e + w[i] + 0x5A827999;
        e = d;
        d = c;
        c = ((b << 30) | (b >> 2));
        b = a;
        a = temp;
    }
    for (; i < 40; i++) {
        unsigned int temp = ((a << 5) | (a >> 27)) +
            (b ^ c ^ d) + e + w[i] + 0x6ED9EBA1;
        e = d;
        d = c;
        c = ((b << 30) | (b >> 2));
        b = a;
        a = temp;
    }
    for (; i < 60; i++) {
        unsigned int temp = ((a << 5) | (a >> 27)) +
            ((b & c) | (b & d) | (c & d)) + e + w[i] + 0x8F1BBCDC;
        e = d;
        d = c;
        c = ((b << 30) | (b >> 2));
        b = a;
        a = temp;
    }
    for (; i < 80; i++) {
        unsigned int temp = ((a << 5) | (a >> 27)) +
            (b ^ c ^ d) + e + w[i] + 0xCA62C1D6;
        e = d;
        d = c;
        c = ((b << 30) | (b >> 2));
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

void sha1_update(sha1_context *context, const unsigned char *data, unsigned int len) {
    unsigned int i, j;
    j = (context->count[0] >> 3) & 63;
    context->count[0] += (len << 3);
    if (context->count[0] < (len << 3)) context->count[1]++;
    context->count[1] += (len >> 29);

    if ((j + len) > 63) {
        memcpy(&context->buffer[j], data, (i = 64 - j));
        sha1_transform(context->state, context->buffer);
        for (; i + 63 < len; i += 64)
            sha1_transform(context->state, &data[i]);
        j = 0;
    } else i = 0;

    memcpy(&context->buffer[j], &data[i], (len - i));
}

void sha1_final(unsigned char digest[20], sha1_context *context) {
    unsigned char finalcount[8];
    unsigned int i;

    // Store bit count
    for (i = 0; i < 8; i++)
        finalcount[i] = (unsigned char)(
            (context->count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) & 0xFF);

    // Pad with a "1" bit, then zero bits
    sha1_update(context, (unsigned char *)"\x80", 1);
    // Pad until message length in bits ≡ 448 (mod 512)
    while ((context->count[0] & 504) != 448)
        sha1_update(context, (unsigned char *)"\0", 1);

    // Append length in bits
    sha1_update(context, finalcount, 8);

    // Output final hash
    for (i = 0; i < 20; i++)
        digest[i] = (unsigned char)(
            (context->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 0xFF);
}


char *split(char **input, char *sep) {
    char *curr = (char *)"";
    while (curr && !curr[0] && *input) curr = strsep(input, sep);
    return (curr);
}

uint32_t time_get_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

unsigned long long time_get_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

void unescape_uri(char *uri) {
    char *src = uri;
    char *dst = uri;

    while (*src && !alt_isspace((int)(*src)) && (*src != '%'))
        src++;

    dst = src;
    while (*src && !alt_isspace((int)(*src)))
    {
        *dst++ = (*src == '+') ? ' ' :
                 ((*src == '%') && src[1] && src[2]) ?
                 ((*++src & 0x0F) + 9 * (*src > '9')) * 16 + ((*++src & 0x0F) + 9 * (*src > '9')) :
                 *src;
        src++;
    }
    *dst = '\0';
}

void uuid_generate(char *uuid) {
    const char *chars = "0123456789abcdef";

    int i, j = 0;
    for (i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            uuid[i] = '-';
        } else {
            uuid[i] = chars[rand() % 16];
        }
    }
    uuid[36] = '\0';
}