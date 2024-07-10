#pragma once

#define HAL_DANGER(mod, x, ...) \
    do { \
        fprintf(stderr, "[%s] \033[31m", (mod)); \
        fprintf(stderr, (x), ##__VA_ARGS__); \
        fprintf(stderr, "\033[0m"); \
    } while (0)

#define HAL_ERROR(mod, x, ...) \
    do { \
        fprintf(stderr, "[%s] \033[31m", (mod)); \
        fprintf(stderr, (x), ##__VA_ARGS__); \
        fprintf(stderr, "\033[0m"); \
        return EXIT_FAILURE; \
    } while (0)

#define HAL_INFO(mod, x, ...) \
    do { \
        fprintf(stderr, "[%s] ", (mod)); \
        fprintf(stderr, (x), ##__VA_ARGS__); \
    } while (0)

#define HAL_WARNING(mod, x, ...) \
    do { \
        fprintf(stderr, "[%s] \033[33m", (mod)); \
        fprintf(stderr, (x), ##__VA_ARGS__); \
        fprintf(stderr, "\033[0m"); \
    } while (0)
