#pragma once

#include "types.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <regex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define SHA1_BLOCK_SIZE 64
#define SHA1_DIGEST_SIZE 20

typedef struct {
    unsigned int state[5];
    unsigned int count[2];
    unsigned char buffer[64];
} sha1_context;

void *mmap64(void *start, size_t len, int prot, int flags, int fd, off_t off);

static inline int alt_isspace(int c) {
    return (c == ' ' || c == '\f' || c == '\n' || 
            c == '\r' || c == '\t' || c == '\v');
}

int base64_decode(char *decoded, const char *string, int maxLen);

int base64_encode_length(int len);

int base64_encode(char *encoded, const char *string, int len);

int color_convert555(int color);

int color_parse(const char *str);

int compile_regex(regex_t *r, const char *regex_text);

int escape_url(char *dst, const char *src, size_t maxlen);

void generate_nonce(char *nonce, size_t len);

const char *get_extension(const char *path);

bool get_uint64(char *str, char *pattern, uint64_t *value);

bool get_uint32(char *str, char *pattern, uint32_t *value);

bool get_uint16(char *str, char *pattern, uint16_t *value);

bool get_uint8(char *str, char *pattern, uint8_t *value);

bool hal_registry(unsigned int addr, unsigned int *data, hal_register_op op);

int hex_to_int(char value);

unsigned int ip_to_int(const char *ip);

char ip_in_cidr(const char *ip, const char *cidr);

char *memstr(char *haystack, char *needle, int size, char needlesize);

unsigned int millis();

void reverse(void *arr, size_t width);

void sha1_init(sha1_context *context);

void sha1_update(sha1_context *context, const unsigned char *data, unsigned int len);

void sha1_final(unsigned char digest[20], sha1_context *context);

char *split(char **input, char *sep);

void unescape_uri(char *uri);

void uuid_generate(char *uuid);