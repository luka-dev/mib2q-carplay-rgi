#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "framework/logging.h"

int main(void)
{
    char path[] = "/tmp/mhi2-state-trace-logger-XXXXXX";
    char line[256];
    log_config_t config = LOG_CONFIG_DEFAULT;
    FILE *stream;
    int fd = mkstemp(path);
    int found = 0;

    assert(fd >= 0);
    close(fd);
    unlink(path);
    config.log_path = path;
    config.max_size = 0;
    config.max_files = 0;
    assert(log_init(&config) == HOOK_OK);
    log_write(LOG_LEVEL_WARN, "STATE",
              "ST ev=1 mono_ms=1 pid=1 gen=1 name=TERMINAL_RECORD");
    log_shutdown();

    stream = fopen(path, "r");
    assert(stream != NULL);
    while (fgets(line, sizeof(line), stream)) {
        if (strstr(line, "name=TERMINAL_RECORD")) found = 1;
    }
    fclose(stream);
    unlink(path);
    assert(found);
    return 0;
}
