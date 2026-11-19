#include "check.h"
#include "app_config.h"
#include "hal/globals.h"
#include "region.h"
#include "text.h"

void region_fill_formatted(char *str);

/* What region.c links against, and nothing it does here needs. */
struct AppConfig app_config;
volatile char keepRunning = 0;
hal_platform plat;
float hal_temperature_read(void) { return 42.5f; }
hal_bitmap text_create_rendered(const char *font, double size, const char *text,
    int color, int outline, double thick) {
    hal_bitmap b = {0};
    (void)font; (void)size; (void)text; (void)color; (void)outline; (void)thick;
    return b;
}

static void expand(char *buf, size_t size, const char *text) {
    memset(buf, 0, size);
    strncpy(buf, text, size - 1);
    region_fill_formatted(buf);
}

int main(void) {
    char buf[80];

    strcpy(timefmt, DEF_TIMEFMT);

    /* Ordinary text and placeholders */
    expand(buf, sizeof(buf), "Front door");
    CHECK(!strcmp(buf, "Front door"));
    expand(buf, sizeof(buf), "cost $$5");
    CHECK(!strcmp(buf, "cost $5"));
    expand(buf, sizeof(buf), "$T C");
    CHECK(!strcmp(buf, "42.5 C"));
    expand(buf, sizeof(buf), "$t");
    CHECK(strlen(buf) == 19);

    /* The report: with the default 19-character format, five $t already
       wrote past the 80-byte buffer; a whole OSD text of them, a kilobyte.
       Now the text stops at 79 characters. */
    expand(buf, sizeof(buf), "$t$t$t$t$t");
    CHECK(strlen(buf) == 79);
    {
        char text[80];
        for (int i = 0; i < 39; i++) memcpy(text + 2 * i, "$t", 2);
        text[78] = '\0';
        strcpy(timefmt, "%Y-%m-%d %H:%M:%S %Y-%m-%d %H:%M");
        expand(buf, sizeof(buf), text);
        CHECK(strlen(buf) == 79);
        strcpy(timefmt, DEF_TIMEFMT);
    }

    /* A format strftime cannot fill expands to nothing, not to garbage */
    strcpy(timefmt, "%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y");
    expand(buf, sizeof(buf), "[$t]");
    CHECK(!strcmp(buf, "[]"));

    CHECK_DONE();
}
