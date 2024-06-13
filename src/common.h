#pragma once

#include "app_config.h"
#include "hal/support.h"

#ifndef CEILING
#define CEILING_POS(X) ((X-(int)(X)) > 0 ? (int)(X+1) : (int)(X))
#define CEILING_NEG(X) (int)(X)
#define CEILING(X) ( ((X) > 0) ? CEILING_POS(X) : CEILING_NEG(X) )
#endif

#define starts_with(a, b) !strncmp(a, b, strlen(b))
#define equals(a, b) !strcmp(a, b)
#define equals_case(a, b) !strcasecmp(a, b)
#define ends_with(a, b)      \
    size_t alen = strlen(a); \
    size_t blen = strlen(b); \
    return (alen > blen) && strcmp(a + alen - blen, b);
#define empty(x) (x[0] == '\0')