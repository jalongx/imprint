#include "ui.h"
#include "utils.h"
#include "config.h"
#include "colors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

void ui_error(const char *message)
{
    if (gx_no_gui) {
        fprintf(stderr, RED "ERROR: %s\n" RESET, message);
        return;
    }

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "zenity --error --title='Error' --text='%s' 2>/dev/null",
             message);
    system(cmd);
}

void ui_info(const char *message)
{
    if (gx_no_gui) {
        fprintf(stderr, "%s\n", message);
        return;
    }

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "zenity --info --title='Info' --text='%s' 2>/dev/null",
             message);
    system(cmd);
}


/* Directory chooser */
char *ui_choose_directory(void)
{
    char cmd[2048];

    if (gx_config.backup_dir[0] != '\0') {
        snprintf(cmd, sizeof(cmd),
                 "zenity --file-selection "
                 "--directory "
                 "--filename='%s/' "
                 "--title='Choose backup directory' 2>/dev/null",
                 gx_config.backup_dir);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "zenity --file-selection "
                 "--directory "
                 "--title='Choose backup directory' 2>/dev/null");
    }

    FILE *fp = popen(cmd, "r");
    if (!fp)
        return NULL;

    char buf[1024] = {0};
    if (!fgets(buf, sizeof(buf), fp)) {
        pclose(fp);
        return NULL;
    }

    pclose(fp);
    buf[strcspn(buf, "\r\n")] = '\0';

    return strdup(buf);
}



/*
 * Partition chooser:
 *  - show real partitions
 *  - show filesystem type
 *  - show label
 *  - show size
 *  - show mountpoint (if mounted)
 *
 * Uses lsblk -rno NAME,LABEL,FSTYPE,SIZE,MOUNTPOINT
 */

char *ui_choose_partition(void)
{
    // PATH first so it's always present and easy to parse
    FILE *fp = popen("lsblk -rpno PATH,TYPE,FSTYPE,SIZE,LABEL,MOUNTPOINT", "r");
    if (!fp) {
        ui_error("Failed to run lsblk.");
        return NULL;
    }

    struct entry {
        char device[256];   // full PATH
        int mounted;
    } entries[256];

    int entry_count = 0;

    char cmd[16384];
    strcpy(cmd,
           "zenity --list "
           "--title='Select partition to back up' "
           "--text='Choose a partition:' "
           "--width=1200 --height=1000 "
           "--column='Path' "
           "--column='Type' "
           "--column='Filesystem' "
           "--column='Size' "
           "--column='Label' "
           "--column='Status' "
           "--print-column=1 2>/dev/null ");

    char line[512];

    while (fgets(line, sizeof(line), fp)) {

        char path[256]={0}, type[32]={0}, fstype[64]={0},
        size[64]={0}, label[128]={0}, mount[256]={0};

        // PATH TYPE FSTYPE SIZE LABEL MOUNTPOINT
        int fields = sscanf(line, "%255s %31s %63s %63s %127s %255s",
                            path, type, fstype, size, label, mount);

        // Require at least PATH + TYPE
        if (fields < 2)
            continue;

        // Skip devices with no valid PATH
        if (strlen(path) == 0)
            continue;

        // Accept partitions, LUKS, and LVM
        if (strcmp(type, "part") != 0 &&
            strcmp(type, "crypt") != 0 &&
            strcmp(type, "lvm")  != 0)
            continue;

        // Normalize label (optional field)
        if (fields < 5 || strlen(label) == 0) {
            strcpy(label, "(no label)");
        } else {
            int empty = 1;
            for (int i = 0; label[i]; i++) {
                if (!isspace((unsigned char)label[i])) {
                    empty = 0;
                    break;
                }
            }
            if (empty)
                strcpy(label, "(no label)");
        }

        // Normalize size (optional)
        if (fields < 4 || strlen(size) == 0)
            strcpy(size, "(unknown)");

        // Mountpoint (optional)
        int is_mounted = (fields >= 6 && strlen(mount) > 0);

        char status[256];
        if (is_mounted)
            snprintf(status, sizeof(status), "MOUNTED at %s", mount);
        else
            snprintf(status, sizeof(status), "not mounted");

        // Store PATH
        snprintf(entries[entry_count].device,
                 sizeof(entries[entry_count].device),
                 "%s",
                 path);

        entries[entry_count].mounted = is_mounted;
        entry_count++;

        // Build Zenity row
        char row[1024];
        snprintf(row, sizeof(row),
                 " '%s' '%s' '%s' '%s' '%s' '%s'",
                 path,
                 type,
                 strlen(fstype) ? fstype : "(none)",
                 size,
                 label,
                 status);

        strcat(cmd, row);
    }

    pclose(fp);

    FILE *zen = popen(cmd, "r");
    if (!zen) {
        ui_error("Failed to launch Zenity.");
        return NULL;
    }

    char result[512]={0};
    if (!fgets(result, sizeof(result), zen)) {
        pclose(zen);
        return NULL;
    }

    pclose(zen);

    result[strcspn(result, "\r\n")] = '\0';

    for (int i = 0; i < entry_count; i++) {
        if (strcmp(entries[i].device, result) == 0) {
            if (entries[i].mounted) {
                ui_error("The selected partition is mounted and cannot be backed up.");
                return NULL;
            }
            break;
        }
    }

    return strdup(result);
}



/* Filename entry dialog */
char *ui_enter_filename(const char *default_name)
{
    char cmd[1024];

    // Build a safe, fully quoted Zenity command
    snprintf(cmd, sizeof(cmd),
             "zenity --entry "
             "--title='Backup filename' "
             "--text='Enter filename for backup (e.g., %s):' "
             "--entry-text='%s' 2>/dev/null",
             default_name, default_name);

    FILE *fp = popen(cmd, "r");
    if (!fp)
        return NULL;

    char buf[512] = {0};

    // Read the filename returned by Zenity
    if (!fgets(buf, sizeof(buf), fp)) {
        pclose(fp);
        return NULL;
    }

    pclose(fp);

    // Strip newline
    buf[strcspn(buf, "\r\n")] = '\0';

    // Debug output so we can see exactly what Zenity returned
    // fprintf(stderr, "DEBUG: filename entered: '%s'\n", buf);

    // If user pressed Cancel or left it empty
    if (strlen(buf) == 0)
        return NULL;

    // Return a heap-allocated copy
    return strdup(buf);
}

char *ui_choose_image_file(void)
{
    char cmd[2048];

    /*
     * We want to show:
     *   - normal single-file images
     *   - ONLY the first chunk of a chunked series (*.000)
     *
     * Zenity cannot express complex logic, but we can simply
     * include *.000 in the filter and exclude *.001+.  Those
     * will not match any filter and will be hidden.
     */

    const char *filter =
    "--file-filter='Image files | "
    "*.img.lz4 *.img.gz *.img.zst "
    "*.lz4 *.gz *.zst "
    "*.000'";

    if (gx_config.backup_dir[0] != '\0') {
        snprintf(cmd, sizeof(cmd),
                 "zenity --file-selection "
                 "--filename='%s/' "
                 "--title='Choose backup image file' "
                 "%s "
                 "2>/dev/null",
                 gx_config.backup_dir,
                 filter);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "zenity --file-selection "
                 "--title='Choose backup image file' "
                 "%s "
                 "2>/dev/null",
                 filter);
    }

    FILE *fp = popen(cmd, "r");
    if (!fp)
        return NULL;

    char buf[1024] = {0};
    if (!fgets(buf, sizeof(buf), fp)) {
        pclose(fp);
        return NULL;
    }

    pclose(fp);

    buf[strcspn(buf, "\r\n")] = '\0';

    if (buf[0] == '\0')
        return NULL;

    return strdup(buf);
}


char *ui_choose_partition_with_title(const char *title, const char *text)
{
    // PATH first so it's always present and easy to parse
    FILE *fp = popen("lsblk -rpno PATH,TYPE,FSTYPE,SIZE,LABEL,MOUNTPOINT", "r");
    if (!fp) {
        ui_error("Failed to run lsblk.");
        return NULL;
    }

    struct entry {
        char device[256];   // full PATH
        int mounted;
    } entries[256];

    int entry_count = 0;

    char cmd[16384];
    snprintf(cmd, sizeof(cmd),
             "zenity --list "
             "--title='%s' "
             "--text='%s' "
             "--width=900 --height=500 "
             "--column='Path' "
             "--column='Label' "
             "--column='Filesystem' "
             "--column='Size' "
             "--column='Status' "
             "--print-column=1 2>/dev/null ",
             title, text);

    char line[512];

    while (fgets(line, sizeof(line), fp)) {

        char path[256]={0}, type[32]={0}, fstype[64]={0},
        size[64]={0}, label[128]={0}, mount[256]={0};

        // PATH TYPE FSTYPE SIZE LABEL MOUNTPOINT
        int fields = sscanf(line, "%255s %31s %63s %63s %127s %255s",
                            path, type, fstype, size, label, mount);

        // Require at least PATH + TYPE
        if (fields < 2)
            continue;

        // Skip devices with no valid PATH
        if (strlen(path) == 0)
            continue;

        // Accept partitions, LUKS, and LVM
        if (strcmp(type, "part") != 0 &&
            strcmp(type, "crypt") != 0 &&
            strcmp(type, "lvm")  != 0)
            continue;

        // Normalize label
        if (fields < 5 || strlen(label) == 0)
            strcpy(label, "(no label)");

        // Normalize size
        if (fields < 4 || strlen(size) == 0)
            strcpy(size, "(unknown)");

        int is_mounted = (fields >= 6 && strlen(mount) > 0);

        char status[256];
        if (is_mounted)
            snprintf(status, sizeof(status), "MOUNTED at %s", mount);
        else
            snprintf(status, sizeof(status), "not mounted");

        // Store PATH
        snprintf(entries[entry_count].device,
                 sizeof(entries[entry_count].device),
                 "%s",
                 path);

        entries[entry_count].mounted = is_mounted;
        entry_count++;

        // Build Zenity row
        char row[1024];
        snprintf(row, sizeof(row),
                 " '%s' '%s' '%s' '%s' '%s'",
                 path,
                 label,
                 strlen(fstype) ? fstype : "(none)",
                 size,
                 status);

        strcat(cmd, row);
    }

    pclose(fp);

    FILE *zen = popen(cmd, "r");
    if (!zen) {
        ui_error("Failed to launch Zenity.");
        return NULL;
    }

    char result[512]={0};
    if (!fgets(result, sizeof(result), zen)) {
        pclose(zen);
        return NULL;
    }

    pclose(zen);

    result[strcspn(result, "\r\n")] = '\0';

    // Validate mount status
    for (int i = 0; i < entry_count; i++) {
        if (strcmp(entries[i].device, result) == 0) {
            if (entries[i].mounted) {
                ui_error("The selected partition is mounted and cannot be used.");
                return NULL;
            }
            break;
        }
    }

    return strdup(result);
}

