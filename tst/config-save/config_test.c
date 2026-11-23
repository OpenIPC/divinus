#include "check.h"
#include "app_config.h"

hal_platform plat;
osd osds[MAX_OSD];
char timefmt[64] = "%Y/%m/%d %H:%M:%S";
void region_defaults(osd *o, int id) { memset(o, 0, sizeof(*o)); }

static int read_file(const char *path, char *text, size_t size) {
    FILE *file = fopen(path, "r");
    if (!file) return 0;
    size_t len = fread(text, 1, size - 1, file);
    fclose(file);
    text[len] = '\0';
    return 1;
}

static void write_file(const char *path, const char *text) {
    FILE *file = fopen(path, "w");
    CHECK(file != NULL);
    if (!file) return;
    fputs(text, file);
    fclose(file);
}

int main(void) {
    char text[8192];

    /* The configuration file is looked up next to the executable first */
    write_file("divinus.yaml", "marker: original\n");
    app_config.web_port = 8081;

    CHECK(app_config_save() == EXIT_SUCCESS);

    /* The new configuration is in place, nothing is left half-written */
    CHECK(read_file("divinus.yaml", text, sizeof(text)));
    CHECK(strstr(text, "web_port: 8081") != NULL);
    CHECK(strstr(text, "marker: original") == NULL);
    CHECK(access("divinus.yaml.new", F_OK) != 0);

    /* The file before the save is kept as .bak */
    CHECK(read_file("divinus.yaml.bak", text, sizeof(text)));
    CHECK(strstr(text, "marker: original") != NULL);

    /* A second save keeps the file before that one */
    app_config.web_port = 8082;
    CHECK(app_config_save() == EXIT_SUCCESS);
    CHECK(read_file("divinus.yaml", text, sizeof(text)));
    CHECK(strstr(text, "web_port: 8082") != NULL);
    CHECK(read_file("divinus.yaml.bak", text, sizeof(text)));
    CHECK(strstr(text, "web_port: 8081") != NULL);

    /* Restoring puts the previous file back, as at a normal exit */
    app_config_restore();
    CHECK(read_file("divinus.yaml", text, sizeof(text)));
    CHECK(strstr(text, "web_port: 8081") != NULL);
    CHECK(access("divinus.yaml.bak", F_OK) != 0);

    /* A save that cannot be written leaves the configuration alone */
    write_file("divinus.yaml.new", "stale\n");
    CHECK(chmod("divinus.yaml.new", 0400) == 0);
    if (geteuid() != 0) {
        CHECK(app_config_save() == EXIT_FAILURE);
        CHECK(read_file("divinus.yaml", text, sizeof(text)));
        CHECK(strstr(text, "web_port: 8081") != NULL);
    }
    remove("divinus.yaml.new");

    remove("divinus.yaml");
    remove("divinus.yaml.bak");
    CHECK_DONE();
}
