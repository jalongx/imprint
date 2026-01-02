#ifndef GHOSTX_CONFIG_H
#define GHOSTX_CONFIG_H

#include <stdbool.h>

#define GHOSTX_VERSION   "0.70.00"
#define GHOSTX_BUILD_DATE __DATE__

typedef struct {
    char backup_dir[1024];
    char compression[32];
} GhostXConfig;

extern GhostXConfig gx_config;

void ghostx_config_load(void);
void ghostx_config_save(void);



#endif
