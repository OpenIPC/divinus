#include "config.h"

enum ConfigError find_sections(struct IniConfig *ini) {
    for (unsigned int i = 0; i < MAX_SECTIONS; ++i)
        ini->sections[i].pos = -1;

    regex_t regex;
    if (compile_regex(&regex, REG_SECTION) < 0) {
        HAL_DANGER("config", "Error compiling regex!\n");
        return CONFIG_REGEX_ERROR;
    };

    size_t n_matches = 4;
    regmatch_t m[n_matches];

    int section_pos = 0;
    int section_index = 0;
    while (1) {
        if (section_index >= MAX_SECTIONS)
            break;
        int match = regexec(&regex, ini->str + section_pos, n_matches, m, 0);
        if (match != 0)
            break;

        int i = 2;
        if (m[i].rm_eo - m[i].rm_so == 0)
            i++;
        int len = sprintf(ini->sections[section_index].name, "%.*s",
            (int)(m[i].rm_eo - m[i].rm_so), ini->str + section_pos + m[i].rm_so);
        ini->sections[section_index].name[len] = 0;
        section_pos = section_pos + (int)m[1].rm_eo;
        ini->sections[section_index].pos = section_pos;
        section_index++;
    }

    regfree(&regex);
    return CONFIG_OK;
}

enum ConfigError section_pos(
    struct IniConfig *ini, const char *section, int *start_pos, int *end_pos) {
    for (unsigned int i = 0; i < MAX_SECTIONS; ++i) {
        if (ini->sections[i].pos > 0 &&
            strcasecmp(ini->sections[i].name, section) == 0) {
            *start_pos = ini->sections[i].pos;
            if (i + 1 < MAX_SECTIONS && ini->sections[i + i].pos > 0)
                *end_pos = ini->sections[i + 1].pos;
            else {
                *end_pos = -1;
            }
            return CONFIG_OK;
        }
    }

    return CONFIG_SECTION_NOT_FOUND;
}

enum ConfigError parse_param_value(
    struct IniConfig *ini, const char *section, const char *param_name,
    char *param_value) {
    int start_pos = 0;
    int end_pos = 0;
    if (strlen(section) > 0) {
        enum ConfigError err = section_pos(ini, section, &start_pos, &end_pos);
        if (err != CONFIG_OK)
            return err;
    }

    regex_t regex;
    char reg_buf[128];
    ssize_t reg_buf_len = sprintf(reg_buf, REG_PARAM, param_name);
    reg_buf[reg_buf_len] = 0;
    if (compile_regex(&regex, reg_buf) < 0) {
        HAL_DANGER("config", "Error compiling regex!\n");
        return CONFIG_REGEX_ERROR;
    };

    size_t n_matches = 2;
    regmatch_t m[n_matches];
    int match = regexec(&regex, ini->str + start_pos, n_matches, m, 0);
    regfree(&regex);
    if (match > 0 || (end_pos >= 0 && end_pos - start_pos < m[1].rm_so))
        return CONFIG_PARAM_NOT_FOUND;

    int res = sprintf(param_value, "%.*s", (int)(m[1].rm_eo - m[1].rm_so),
        ini->str + start_pos + m[1].rm_so);
    param_value[res] = 0;
    return CONFIG_OK;
}

enum ConfigError parse_enum(
    struct IniConfig *ini, const char *section, const char *param_name,
    void *enum_value, const char *possible_values[],
    const int possible_values_count, const int possible_values_offset) {
    char param_value[64];
    enum ConfigError err =
        parse_param_value(ini, section, param_name, param_value);
    if (err != CONFIG_OK)
        return err;

    // try to parse as number
    char *end;
    long res = strtol(param_value, &end, 10);
    if (*end) {
        res = strtol(param_value, &end, 16);
    }

    if (!*end) {
        *(int *)enum_value = res;
        return CONFIG_OK;
    }

    // try to find value in possible values
    for (unsigned int i = 0; i < possible_values_count; ++i)
        if (strcasecmp(param_value, possible_values[i]) == 0) {
            *(int *)enum_value = possible_values_offset + i;
            return CONFIG_OK;
        }

    // print error
    HAL_DANGER("config",
        "Can't parse param '%s' value '%s'. Is not a number and is not in "
        "possible values: ",
        param_name, param_value);
    for (unsigned int i = 0; i < possible_values_count; ++i)
        printf("'%s', ", possible_values[i]);
    return CONFIG_ENUM_INCORRECT_STRING;
}

enum ConfigError parse_bool(
    struct IniConfig *ini, const char *section, const char *param_name,
    bool *bool_value) {
    const char *possible_values[] = {"0", "1", "false", "true",
                                     "n", "y", "no",    "yes"};
    const int count = sizeof(possible_values) / sizeof(const char *);

    int val;
    enum ConfigError err = parse_enum(
        ini, section, param_name, (void *)&val, possible_values, count, 0);
    if (err != CONFIG_OK)
        return err;
    if (val % 2) {
        *bool_value = true;
        return CONFIG_OK;
    } else {
        *bool_value = false;
        return CONFIG_OK;
    }
    *bool_value = false;
    return CONFIG_OK;
}

enum ConfigError parse_int(
    struct IniConfig *ini, const char *section, const char *param_name,
    const int min, const int max, int *int_value) {
    char param_value[64];
    enum ConfigError err =
        parse_param_value(ini, section, param_name, param_value);
    if (err != CONFIG_OK)
        return err;

    // try to parse as number
    char *end = NULL;
    long res = strtol(param_value, &end, 10);
    if (*end) {
        res = strtol(param_value, &end, 16);
    }
    if (!*end) {
        if (res < min || res > max) {
            HAL_DANGER("config",
                "Can't parse param '%s' value '%s'. Value '%ld' is not in a "
                "range [%d; %d].",
                param_name, param_value, res, min, max);
            return CONFIG_PARAM_ISNT_IN_RANGE;
        }
        *int_value = (int)res;
        return CONFIG_OK;
    }
    if (strcasecmp(param_value, "true") == 0) {
        *int_value = 1;
        return CONFIG_OK;
    }
    if (strcasecmp(param_value, "false") == 0) {
        *int_value = 0;
        return CONFIG_OK;
    }

    HAL_DANGER("config",
        "Can't parse param '%s' value '%s'. Is not a integer (dec or hex) "
        "number.",
        param_name, param_value);
    return CONFIG_PARAM_ISNT_NUMBER;
}

enum ConfigError parse_array(
    struct IniConfig *ini, const char *section, const char *param_name,
    int *array, const int array_size) {
    char param_value[256];
    enum ConfigError err =
        parse_param_value(ini, section, param_name, param_value);
    if (err != CONFIG_OK)
        return err;

    const char *token = strtok(param_value, "|");
    for (int i = 0; token && i < array_size; i++) {
        char *end = NULL;
        int64_t res = strtol(token, &end, 10);
        if (*end)
            res = strtol(token, &end, 16);
        if (*end) {
            HAL_DANGER("config",
                "Can't parse param '%s' value '%s'. Is not a integer (dec or "
                "hex) number.",
                param_name, token);
            return CONFIG_PARAM_ISNT_NUMBER;
        }
        array[i] = (int)res;
        token = strtok(NULL, "|");
    }

    return CONFIG_OK;
}

enum ConfigError parse_uint64(
    struct IniConfig *ini, const char *section, const char *param_name,
    const uint64_t min, const uint64_t max, uint64_t *int_value) {
    char param_value[64];
    enum ConfigError err = parse_param_value(
        ini, section, param_name, param_value);
    if (err != CONFIG_OK)
        return err;

    // try to parse as number
    char *end = NULL;
    uint64_t res = strtoull(param_value, &end, 10);
    if (*end) {
        res = strtoull(param_value, &end, 16);
    }
    if (!*end) {
        if (res < min || res > max) {
            HAL_DANGER("config",
                "Can't parse param '%s' value '%s'. Value '%lu' is not in a "
                "range [%lu; %lu].\n",
                param_name, param_value, res, min, max);
            return CONFIG_PARAM_ISNT_IN_RANGE;
        }
        *int_value = (uint64_t)res;
        return CONFIG_OK;
    }
    if (strcasecmp(param_value, "true") == 0) {
        *int_value = 1;
        return CONFIG_OK;
    }
    if (strcasecmp(param_value, "false") == 0) {
        *int_value = 0;
        return CONFIG_OK;
    }

    HAL_DANGER("config",
        "Can't parse param '%s' value '%s'. Is not a integer (dec or hex) "
        "number.",
        param_name, param_value);
    return CONFIG_PARAM_ISNT_NUMBER;
}


enum ConfigError parse_uint32(
    struct IniConfig *ini, const char *section, const char *param_name,
    const unsigned int min, const unsigned int max, unsigned int *value) {
    uint64_t val = 0;
    enum ConfigError err =
        parse_uint64(ini, section, param_name, min, max, &val);
    if (err != CONFIG_OK)
        return err;
    *value = val;
    return CONFIG_OK;
}

bool open_config(struct IniConfig *ini, FILE **file) {
    if (!*file)
        return false;

    fseek(*file, 0, SEEK_END);
    size_t length = ftell(*file);
    fseek(*file, 0, SEEK_SET);

    ini->str = malloc(length + 1);
    if (!ini->str) {
        HAL_DANGER("config", "Cannot allocate buffer to hold the config file!\n");
        fclose(*file);
        return false;
    }

    size_t n = fread(ini->str, 1, length, *file);
    if (n != length) {
        HAL_DANGER("config", "Cannot read the whole config file!\n");
        fclose(*file);
        free(ini->str);
        return false;
    }

    fclose(*file);
    ini->str[length] = 0;

    return true;
}