#ifndef UI_H
#define UI_H

#include <stdbool.h>

/*
 * Zenity-based UI functions.
 * These provide dialogs for:
 *  - selecting a backup directory
 *  - selecting a partition (with labels, fs type, size, mount status)
 *  - entering a filename
 *  - showing info/error dialogs
 */

/* Show an error dialog with Zenity. */
void ui_error(const char *message);

/* Show an informational dialog with Zenity. */
void ui_info(const char *message);

/* Ask the user to choose a directory for backups.
 * Returns a newly allocated string (caller must free), or NULL on cancel.
 */
char *ui_choose_directory(void);

/* Ask the user to select a partition.
 * Returns a newly allocated string containing the device path (e.g., "/dev/nvme0n1p2"),
 * or NULL if the user cancels.
 *
 * Mounted partitions are shown but will be rejected after selection.
 */
char *ui_choose_partition(void);

/* Ask the user to enter a filename (default provided).
 * Returns a newly allocated string (caller must free), or NULL on cancel.
 */
char *ui_enter_filename(const char *default_name);

/* Ask the user to select an existing image file (e.g. *.img.gz).
 * Returns a newly allocated string (caller must free), or NULL on cancel.
 */
char *ui_choose_image_file(void);

char *ui_choose_partition_with_title(const char *title, const char *text);


#endif /* UI_H */
