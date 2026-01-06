#ifndef GHOSTX_CONFIG_H
#define GHOSTX_CONFIG_H

#include <stdbool.h>

#define GHOSTX_VERSION    "0.9.2"
#define GHOSTX_BUILD_DATE __DATE__

typedef struct {
    char backup_dir[1024];
    char compression[32];
    int  chunk_size_mb;   // 0 = disabled, >0 = chunk size in MB
} GhostXConfig;

extern GhostXConfig gx_config;

void ghostx_config_load(void);
void ghostx_config_save(void);
extern char gx_workdir_override[1024];

#endif
