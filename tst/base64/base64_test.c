#include "check.h"
#include "hal/tools.h"

static int decode_is(const char *text, const char *expect, int expectLen, int maxLen) {
    char out[64];
    memset(out, 0x55, sizeof(out));
    int len = base64_decode(out, text, maxLen);
    return len == expectLen && !memcmp(out, expect, expectLen);
}

int main(void) {
    /* Whole groups */
    CHECK(decode_is("TWFu", "Man", 3, 64));
    CHECK(decode_is("TWFuTWFu", "ManMan", 6, 64));

    /* Padded tails: two and one input bytes */
    CHECK(decode_is("TWE=", "Ma", 2, 64));
    CHECK(decode_is("TQ==", "M", 1, 64));
    CHECK(decode_is("TWFuTWE=", "ManMa", 5, 64));
    CHECK(decode_is("TWFuTQ==", "ManM", 4, 64));

    /* Tails without padding */
    CHECK(decode_is("TWE", "Ma", 2, 64));
    CHECK(decode_is("TQ", "M", 1, 64));

    /* A lone character carries no complete byte */
    CHECK(decode_is("TWFuT", "Man", 3, 64));
    CHECK(decode_is("", "", 0, 64));

    /* A 16-byte nonce as sent in a WS-UsernameToken */
    {
        char raw[16], text[32], out[32];
        int i;
        for (i = 0; i < 16; i++) raw[i] = (char)(i * 17 + 3);
        base64_encode(text, raw, sizeof(raw));
        CHECK(strlen(text) == 24);
        CHECK(base64_decode(out, text, sizeof(out)) == 16);
        CHECK(!memcmp(out, raw, 16));
    }

    /* Round trip for every length up to 20 bytes */
    {
        char raw[20], text[32], out[32];
        int len, i;
        for (len = 0; len <= 20; len++) {
            for (i = 0; i < len; i++) raw[i] = (char)(i * 29 + 7 + len);
            base64_encode(text, raw, len);
            CHECK(base64_decode(out, text, sizeof(out)) == len);
            CHECK(!memcmp(out, raw, len));
        }
    }

    /* The output limit is kept */
    CHECK(decode_is("TWFuTWFu", "ManMan", 6, 6));
    CHECK(decode_is("TWFuTWE=", "Man", 3, 3));

    CHECK_DONE();
}
