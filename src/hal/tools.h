#pragma once

#include <regex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char *get_extension(const char *path);
const char *get_mimetype(const char *path);

int compile_regex(regex_t *r, const char *regex_text);

int base64_encode_length(int len);
int base64_encode(char *encoded, const char *string, int len);

bool get_uint64(char *str, char *pattern, uint64_t *value);
bool get_uint32(char *str, char *pattern, uint32_t *value);
bool get_uint16(char *str, char *pattern, uint16_t *value);
bool get_uint8(char *str, char *pattern, uint8_t *value);