#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

/* ---------------------------------------------------------
 * Global configuration instance
 * --------------------------------------------------------- */
GhostXConfig gx_config;

/* ---------------------------------------------------------
 * Trim leading whitespace
 * --------------------------------------------------------- */
static char *ltrim(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}

/* ---------------------------------------------------------
 * Parse a single key=value line
 * --------------------------------------------------------- */
static void parse_config_line(char *line)
{
    /* Ignore comments and blank lines */
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\0')
        return;

    char *eq = strchr(line, '=');
    if (!eq)
        return;

    *eq = '\0';
    char *key = ltrim(line);
    char *value = ltrim(eq + 1);

    /* Strip trailing newline */
    value[strcspn(value, "\r\n")] = '\0';

    if (strcmp(key, "backup_dir") == 0) {
        strncpy(gx_config.backup_dir, value, sizeof(gx_config.backup_dir) - 1);
        gx_config.backup_dir[sizeof(gx_config.backup_dir) - 1] = '\0';
    }

    if (strcmp(key, "compression") == 0) {
        strncpy(gx_config.compression, value, sizeof(gx_config.compression) - 1);
        gx_config.compression[sizeof(gx_config.compression) - 1] = '\0';
    }

}

/* ---------------------------------------------------------
 * Resolve config directory using XDG spec
 * Result: out = ".../ghostx"
 * --------------------------------------------------------- */
static void get_config_dir(char *out, size_t out_len)
{
    const char *xdg  = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");

    if (xdg && xdg[0] != '\0') {
        /* xdg + "/ghostx" */
        size_t xlen = strlen(xdg);
        const char suffix[] = "/ghostx";
        size_t slen = sizeof(suffix) - 1;

        if (xlen + slen + 1 > out_len) {
            /* Fallback if somehow too long */
            strncpy(out, "/tmp/ghostx", out_len - 1);
            out[out_len - 1] = '\0';
            return;
        }

        memcpy(out, xdg, xlen);
        memcpy(out + xlen, suffix, slen + 1); /* includes '\0' */
        return;
    }

    if (home && home[0] != '\0') {
        /* home + "/.config/ghostx" */
        size_t hlen = strlen(home);
        const char suffix[] = "/.config/ghostx";
        size_t slen = sizeof(suffix) - 1;

        if (hlen + slen + 1 > out_len) {
            strncpy(out, "/tmp/ghostx", out_len - 1);
            out[out_len - 1] = '\0';
            return;
        }

        memcpy(out, home, hlen);
        memcpy(out + hlen, suffix, slen + 1);
        return;
    }

    /* Last‑ditch fallback */
    strncpy(out, "/tmp/ghostx", out_len - 1);
    out[out_len - 1] = '\0';
}

/* ---------------------------------------------------------
 * Build full path: <dir> + "/config"
 * --------------------------------------------------------- */
static int build_config_path(char *out, size_t out_len, const char *dir)
{
    const char suffix[] = "/config";
    size_t dlen = strlen(dir);
    size_t slen = sizeof(suffix) - 1;

    if (dlen + slen + 1 > out_len) {
        return -1; /* too long */
    }

    memcpy(out, dir, dlen);
    memcpy(out + dlen, suffix, slen + 1); /* includes '\0' */
    return 0;
}

/* ---------------------------------------------------------
 * Load XDG config: <config_dir>/config
 * --------------------------------------------------------- */
void ghostx_config_load(void)
{
    memset(&gx_config, 0, sizeof(gx_config));

    /* Default compression */
    strncpy(gx_config.compression, "lz4", sizeof(gx_config.compression) - 1);

    char dir[2048];
    get_config_dir(dir, sizeof(dir));

    char path[2048];
    if (build_config_path(path, sizeof(path), dir) != 0) {
        return;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return; /* No config yet, or read‑only FS */
    }

    char line[2048];
    while (fgets(line, sizeof(line), fp)) {
        parse_config_line(line);
    }

    fclose(fp);
}

/* ---------------------------------------------------------
 * Save XDG config: <config_dir>/config
 * --------------------------------------------------------- */
void ghostx_config_save(void)
{
    char dir[2048];
    get_config_dir(dir, sizeof(dir));

    /* Try to create directory — ignore errors except EROFS */
    if (mkdir(dir, 0700) != 0 && errno == EROFS) {
        return;  /* ISO or immutable system — silently skip saving */
    }

    char path[2048];
    if (build_config_path(path, sizeof(path), dir) != 0) {
        return;
    }

    FILE *fp = fopen(path, "w");
    if (!fp) {
        return; /* read‑only FS or permission issue — safe to ignore */
    }

    fprintf(fp,
            "# ------------------------------------------------------------\n"
            "# GhostX User Preferences\n"
            "# ------------------------------------------------------------\n"
            "# These settings control how GhostX behaves during backup.\n"
            "# You may edit them manually. Invalid values fall back to safe defaults.\n"
            "#\n"
            "# compression=\n"
            "#   lz4   - extremely fast, moderate compression (recommended for speed)\n"
            "#   zstd  - fast, strong compression (recommended balance)\n"
            "#   gzip  - slow, legacy compatibility only\n"
            "#\n"
            "# Additional options may be added in future versions.\n"
            "# ------------------------------------------------------------\n\n"
    );

    if (gx_config.backup_dir[0] != '\0')
        fprintf(fp, "backup_dir=%s\n", gx_config.backup_dir);

    if (gx_config.compression[0] != '\0')
        fprintf(fp, "compression=%s\n", gx_config.compression);


    fclose(fp);
}
