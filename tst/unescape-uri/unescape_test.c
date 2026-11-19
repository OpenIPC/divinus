#include "check.h"
#include "hal/tools.h"

static int unescapes_to(const char *in, const char *expect) {
    char buf[128];
    strncpy(buf, in, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    unescape_uri(buf);
    return !strcmp(buf, expect);
}

int main(void) {
    /* What the web UI sends */
    CHECK(unescapes_to("text=Front%20door", "text=Front door"));
    /* '+' is a space only after a '%', as before this decoder was rewritten */
    CHECK(unescapes_to("text=Front+door", "text=Front+door"));
    CHECK(unescapes_to("text=%41+b", "text=A b"));
    CHECK(unescapes_to("fmt=%25Y-%25m-%25d", "fmt=%Y-%m-%d"));
    CHECK(unescapes_to("dest=udp%3A%2F%2Fhost%3A5600", "dest=udp://host:5600"));
    CHECK(unescapes_to("plain", "plain"));
    CHECK(unescapes_to("", ""));

    /* A decoded control character is dropped: values are saved to the
       configuration a line each, so %0A was a line of the caller's choosing. */
    CHECK(unescapes_to("adc_device=x%0Astream:%0A%20%20enable:%20true",
                       "adc_device=xstream:  enable: true"));
    CHECK(unescapes_to("a%0D%0Ab", "ab"));
    CHECK(unescapes_to("a%00b", "ab"));
    CHECK(unescapes_to("a%7Fb", "ab"));
    CHECK(unescapes_to("a%09b", "ab"));

    /* A % without two hex digits is left as it is, and never read past */
    CHECK(unescapes_to("100%", "100%"));
    CHECK(unescapes_to("a%2", "a%2"));
    CHECK(unescapes_to("a%zzb", "a%zzb"));

    /* Bytes >= 0x80 decode as themselves (UTF-8 OSD text) */
    CHECK(unescapes_to("%D0%9A", "\xD0\x9A"));

    CHECK_DONE();
}
