v0.3.0, 19-Dec-2025 — First usable release.
- Past the prototype stage.
- Now usable for both backup and restore.
- Posted to GitHub.

v0.3.1, 21-Dec-2025 — Major performance improvement.
- Changed default compression from gzip to lz4.
- Backup throughput ~10× faster with compressed images.
- Slight reduction in compression ratio.

v0.3.2, 25-Dec-2025 — Metadata expansion + checksum UX.
- Added progress indicator for checksum creation and validation.
- Added checksum verification during restore with mismatch warnings.
- Expanded metadata to include disk and partition layout of source disk.

v0.3.3, 25-Dec-2025 - Fixed error handling.
- When partclone generates an error, such as a dirty NTFS parition, the program wasn't picking it up and was still generating a checksum. Fixed.
- imprint now picks up a partclone exit error and handles it more gracefully by deleting the empty file and not generating metadata.

v0.3.4, 26-Dec-2025 - Added functions to create and save user preferences.
- Procedure for handling user preferences is now in place.
- Only preference added at this time was to save user image backup and restore directories.
- Ensured that preference locations was put in .config across most linux distros.
- Ensured that imprint would function normally even in read only/iso environments where preferences can't be written.

v0.4.0, 27-Dec-2025 - Created rescue iso for backup and restoration from a live environment.
- based on Arch mkarchiso.
- uses xfce as a desktop environment and fish as a shell.

v0.50.0, 28-Dec-2025 - Unified the UI for command line and GUI.
- Unified the command line so that users launching imprint directly from a file manager received the same messages as when running from the command line.

v0.6.0, 31-Dec-2025 - Created full-feature KDE wayland rescue ISO for better compatibility with modern systems, particularly laptops with hybrid GPUs.

v0.7.0, 1-Jan-2026 - Added support for multiple compressors.
- Added support for ZSTD and GZIP. Can be switched in the user preferences.
- New config system.
- New metadata schema.
- New compression pipeline.
- New UI elements.
- New dependency model.
- New banner system.

v0.8.0, 2-Jan-2026 - Added tee support to backup pipeline for checksum computation.
- Checksum is now computed as image is created rather than as a separate process following image creation.
- Restore no longer computes thje checksum before restoring an image.
- Metadata files are left in the image file directory for the manual validation of the checksum.
- Backup and restore processes are now twice as fast.

v0.9.0, 2-Jan-2026 - Added ability to backup and restore image file in user-defined chunks.
- User can define the chunk size in the config file in ~/.config/imprint

v0.9.1, 3-Jan-2026 - Changed backup pipeline logic for filesystems that don't support FIFO.
- Imprint now supports saving backups on filesystems that don't suport FIFO.
- Now supports saving backup images on Fat32, NTFS, EXFAT, SMB.

v0.9.2, 4-Jan-2026 - User interface enhancements. Project made "Public" on github.com

v0.9.21, 8-Jan-2026 - Fixed checksum mismatch between generated checksums and the values in the .json and .sha256
- Moved checksum tee to after compressor to ensure checksum is computed over compressed stream
- Verified chunked reconstruction matches .sha256 and JSON metadata
- Added chunk verification script for manual integrity checks

v0.9.3, 10-Jan-2026 — Added backup CLI functionality.
- Introduced a dedicated command‑line interface for backup operations.
- Allows fully headless backups without launching the GUI.
- Unified messaging and progress output between CLI and GUI modes.
- Improved scripting and automation support for advanced users.
- Strengthened error reporting and exit‑code consistency for shell workflows.

v0.9.35, 14-Jan-2026 — Added streaming sniffer for image inspection.
- Implemented streaming partial decompression for zstd and lz4.
- Detects Partclone textual headers without full decompression.
- Extracts filesystem backend (BTRFS, EXT4, XFS, NTFS, FAT32, F2FS).
- Provides full fallback behavior when metadata is missing or corrupted.
- Establishes foundation for restore‑side validation and metadata‑free recovery.

v0.9.4, 15-Jan-2026 — Restore CLI + unified chunked‑restore architecture
- Implemented full restore‑side command‑line interface, enabling headless restores without launching the GUI.
- Normalized image‑path handling across CLI and GUI, including correct stripping of .000 suffixes for chunked images.
- Added authoritative metadata loading during restore, ensuring consistent backend, compression, and partition‑size validation.
- Introduced robust chunk‑set validation for both CLI and GUI restore paths, preventing restores when any chunk is missing or mismatched.
- Unified restore logic between interactive and CLI modes, eliminating divergent behaviors and ensuring deterministic restore flow.
- Simplified restore pipeline by deferring missing‑metadata fallback logic to a future branch.
- Removed reliance on the experimental sniffer for restore decisions; partclone now provides authoritative filesystem‑size validation.
- General cleanup and restructuring of restore.c for clarity, maintainability, and future feature expansion.

v0.9.45, 16‑Jan‑2026 - LUKS + LVM support and unified mapper‑aware device model
- Added full backup/restore support for LUKS‑mapped devices and LVM logical volumes.
- Rebuilt device/partitoin-picker with a PATH‑first, TYPE‑aware model for consistent handling of raw partitions, LUKS, and LVM.
- Improved lsblk parsing to correctly interpret mapper hierarchies, missing labels, and variable‑length device names.
- Ensured correct filesystem detection and backend selection across all mapper types.
- Unified GUI and CLI device‑selection logic.
- Improved metadata accuracy for mapper devices.
- General cleanup and internal consistency improvements across backup and restore paths.
- Validated backup and restore pipelines across LUKS, LVM, chunked images, and zstd compression.

v0.9.46, 17‑Jan‑2026 - Backup and Restore overwrite‑safety for both GUI and CLI, CLI force‑mode, and GUI/CLI consistency fixes
- Added "Are you sure??" prompts when a user is about to overwrite a partition with an image file.
- Added --force flag to allow unattended or scripted restores from the command line
- Added full overwrite‑protection to backup operations, mirroring restore‑side safety.
- Implemented a function to detect existing single‑file and chunked images, including .json metadata and .sha256 checksum files.
- Added "Are you sure??" overwrite confirmations to GUI and CLI backup modes.
- Introduced --force flag for CLI backups, enabling unattended and scripted backups that overwrite an existing image file / file series.
- Fixed a minor bug where imprintr --help checked for dependencies before printing the help message.

