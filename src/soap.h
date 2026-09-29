#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

const char *soap_tag(const char *xml, const char *tag);
bool soap_tag_text(const char *xml, const char *tag, char *text, size_t size);
bool soap_tag_attr_has(const char *xml, const char *tag, const char *needle);
bool soap_action(const char *xml, char *action, size_t size);

int soap_duration(const char *text);
time_t soap_datetime(const char *text);
void soap_datetime_format(time_t when, char *text, size_t size);
int soap_term_seconds(const char *xml, const char *tag, time_t now, int fallback, int max);

bool soap_digest_valid(const char *nonce, const char *created, const char *pass, const char *digest);
