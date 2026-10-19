#include "config.h"
#include "launch_options.h"
#include <stdio.h>
int main(int argc, char **argv) {
    seedless_launch_options_t o; char msg[256];
    if (!seedless_launch_parse(argc, argv, &o, msg, sizeof msg)) { fprintf(stderr, "%s\n", msg); return 2; }
    if (o.help) { seedless_launch_usage(stdout); return 0; }
    seedless_config_t c = {0}; seedless_config_error_t e = {0};
    if (!seedless_config_load_or_create(&c, o.config_path, &e)) {
        fprintf(stderr, "%s: %s\n", e.key[0] ? e.key : c.selected_path, e.message);
        seedless_config_free(&c); return 1;
    }
    printf("config=%s\n", c.selected_path); seedless_config_free(&c); return 0;
}
