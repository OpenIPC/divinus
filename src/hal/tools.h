#pragma once

#include <ctype.h>
#include <regex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>

int base64_encode_length(int len);

int base64_encode(char *encoded, const char *string, int len);

int compile_regex(regex_t *r, const char *regex_text);

const char *get_extension(const char *path);

bool get_uint64(char *str, char *pattern, uint64_t *value);

bool get_uint32(char *str, char *pattern, uint32_t *value);

bool get_uint16(char *str, char *pattern, uint16_t *value);

bool get_uint8(char *str, char *pattern, uint8_t *value);

char *memstr(char *haystack, char *needle, int size, char needlesize);

unsigned int millis();

void reverse(void *arr, size_t width);

char *split(char **input, char *sep);

void unescape_uri(char *uri);

void uuid_generate(char *uuid);