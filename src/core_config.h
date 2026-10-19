#ifndef SEEDLESSDS_CORE_CONFIG_H
#define SEEDLESSDS_CORE_CONFIG_H
#include "config.h"
#include <stdint.h>
#include <seedlessds/nds.h>
int seedless_core_config_apply(const seedless_config_t *config,
                               const seedless_runtime_config_t *runtime,
                               char *error, size_t error_size);
void seedless_core_config_reset(void);
int seedless_core_config_save_error(void);
void seedless_core_config_set_core(nds_t *core);
uint64_t seedless_core_config_bits(void);
#endif
