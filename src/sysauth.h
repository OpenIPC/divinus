#pragma once

// 1 when a Basic credential matches an account in /etc/shadow
int sysauth_check_basic(const char *b64);
