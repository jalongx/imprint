#ifndef GHOSTX_CONFIG_H
#define GHOSTX_CONFIG_H

#include <stdbool.h>

typedef struct {
    char backup_dir[1024];
} GhostXConfig;

extern GhostXConfig gx_config;

void ghostx_config_load(void);
void ghostx_config_save(void);

#endif
