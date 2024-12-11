#include "tools.h"

static const char basis_64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int base64_encode_length(int len) { return ((len + 2) / 3 * 4) + 1; }

int base64_encode(char *encoded, const char *string, int len) {
    int i;
    char *p;

    p = encoded;
    for (i = 0; i < len - 2; i += 3) {
        *p++ = basis_64[(string[i] >> 2) & 0x3F];
        *p++ = basis_64
            [((string[i] & 0x3) << 4) | ((int)(string[i + 1] & 0xF0) >> 4)];
        *p++ = basis_64
            [((string[i + 1] & 0xF) << 2) | ((int)(string[i + 2] & 0xC0) >> 6)];
        *p++ = basis_64[string[i + 2] & 0x3F];
    }
    if (i < len) {
        *p++ = basis_64[(string[i] >> 2) & 0x3F];
        if (i == (len - 1)) {
            *p++ = basis_64[((string[i] & 0x3) << 4)];
            *p++ = '=';
        } else {
            *p++ = basis_64
                [((string[i] & 0x3) << 4) | ((int)(string[i + 1] & 0xF0) >> 4)];
            *p++ = basis_64[((string[i + 1] & 0xF) << 2)];
        }
        *p++ = '=';
    }

    *p++ = '\0';
    return p - encoded;
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

char *split(char **input, char *sep) {
    char *curr = (char *)"";
    while (curr && !curr[0] && *input) curr = strsep(input, sep);
    return (curr);
}

void unescape_uri(char *uri) {
    char *src = uri;
    char *dst = uri;

    while (*src && !isspace((int)(*src)) && (*src != '%'))
        src++;

    dst = src;
    while (*src && !isspace((int)(*src)))
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