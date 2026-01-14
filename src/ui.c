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
        fprintf(stderr, YELLOW "ERROR: %s\n" RESET, message);
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
    FILE *fp = popen("lsblk -rno NAME,LABEL,FSTYPE,SIZE,MOUNTPOINT", "r");
    if (!fp) {
        ui_error("Failed to run lsblk.");
        return NULL;
    }

    // Track mount status for each device
    struct entry {
        char device[128];
        int mounted;
    } entries[256];
    int entry_count = 0;

    // Start building the zenity command
    char cmd[16384];
    strcpy(cmd,
           "zenity --list "
           "--title='Select partition to back up' "
           "--text='Choose a partition:' "
           "--width=900 --height=500 "
           "--column='Device' "
           "--column='Label' "
           "--column='Filesystem' "
           "--column='Size' "
           "--column='Status' "
           "--print-column=1 2>/dev/null ");

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char name[128]={0}, label[128]={0}, fstype[64]={0}, size[64]={0}, mount[256]={0};

        int fields = sscanf(line, "%127s %127s %63s %63s %255s",
                            name, label, fstype, size, mount);

        if (fields < 4 || strlen(fstype)==0)
            continue;

        size_t len = strlen(name);
        if (!(strchr(name,'p') || (len>0 && isdigit(name[len-1]))))
            continue;

        char status[256];
        int is_mounted = (fields==5 && strlen(mount)>0);

        if (is_mounted)
            snprintf(status,sizeof(status),"MOUNTED at %s", mount);
        else
            snprintf(status,sizeof(status)," ");

        // Record mount status for later lookup
        strncpy(entries[entry_count].device, name, sizeof(entries[entry_count].device));
        entries[entry_count].mounted = is_mounted;
        entry_count++;

        // Append row as arguments
        char row[1024];
        snprintf(row, sizeof(row),
                 " '/dev/%s' '%s' '%s' '%s' '%s'",
                 name,
                 strlen(label)?label:"(no label)",
                 fstype,
                 size,
                 status);

        strcat(cmd, row);
    }

    pclose(fp);

    // Run Zenity and capture output
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

    // Check mount status using our stored table
    for (int i = 0; i < entry_count; i++) {
        char fullpath[256];
        snprintf(fullpath, sizeof(fullpath),
                 "/dev/%.*s",
                 (int)sizeof(entries[i].device) - 1,
                 entries[i].device);

        if (strcmp(fullpath, result) == 0) {
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
    FILE *fp = popen("lsblk -rno NAME,LABEL,FSTYPE,SIZE,MOUNTPOINT", "r");
    if (!fp) {
        ui_error("Failed to run lsblk.");
        return NULL;
    }

    struct entry {
        char device[128];
        int mounted;
    } entries[256];
    int entry_count = 0;

    char cmd[16384];
    snprintf(cmd, sizeof(cmd),
             "zenity --list "
             "--title='%s' "
             "--text='%s' "
             "--width=900 --height=500 "
             "--column='Device' "
             "--column='Label' "
             "--column='Filesystem' "
             "--column='Size' "
             "--column='Status' "
             "--print-column=1 2>/dev/null ",
             title, text);

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char name[128]={0}, label[128]={0}, fstype[64]={0}, size[64]={0}, mount[256]={0};

        int fields = sscanf(line, "%127s %127s %63s %63s %255s",
                            name, label, fstype, size, mount);

        if (fields < 4 || strlen(fstype)==0)
            continue;

        size_t len = strlen(name);
        if (!(strchr(name,'p') || (len>0 && isdigit(name[len-1]))))
            continue;

        char status[256];
        int is_mounted = (fields==5 && strlen(mount)>0);

        if (is_mounted)
            snprintf(status,sizeof(status),"MOUNTED at %s", mount);
        else
            snprintf(status,sizeof(status)," ");

        strncpy(entries[entry_count].device, name, sizeof(entries[entry_count].device));
        entries[entry_count].mounted = is_mounted;
        entry_count++;

        char row[1024];
        snprintf(row, sizeof(row),
                 " '/dev/%s' '%s' '%s' '%s' '%s'",
                 name,
                 strlen(label)?label:"(no label)",
                 fstype,
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

    for (int i = 0; i < entry_count; i++) {
        char fullpath[256];
        snprintf(fullpath, sizeof(fullpath),
                 "/dev/%.*s",
                 (int)sizeof(entries[i].device) - 1,
                 entries[i].device);

        if (strcmp(fullpath, result) == 0) {
            if (entries[i].mounted) {
                ui_error("The selected partition is mounted and cannot be used.");
                return NULL;
            }
            break;
        }
    }

    return strdup(result);
}
