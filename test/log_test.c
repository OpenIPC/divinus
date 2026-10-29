#include "check.h"
#include "hal/macros.h"
#include <unistd.h>

static const char *logPath = "log_test.out";

int main(void) {
    int terminalFd = dup(STDERR_FILENO);
    CHECK(freopen(logPath, "w", stderr) != NULL);
    HAL_INFO("test", "info line\n");
    HAL_WARNING("test", "warning line\n");
    HAL_DANGER("test", "danger line\n");
    fflush(stderr);
    dup2(terminalFd, STDERR_FILENO);
    close(terminalFd);

    FILE *logFile = fopen(logPath, "r");
    CHECK(logFile != NULL);
    char logText[256];
    size_t logLen = fread(logText, 1, sizeof(logText) - 1, logFile);
    fclose(logFile);
    remove(logPath);
    logText[logLen] = '\0';

    CHECK(strstr(logText, "[test] info line\n") != NULL);
    CHECK(strstr(logText, "[test] warning line\n") != NULL);
    CHECK(memchr(logText, '\033', logLen) == NULL);
    CHECK_DONE();
}
