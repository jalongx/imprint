Imprint Disk Imaging

Imprint is a modern, Linux‑native reimagining of the classic Norton Ghost workflow.
It provides a clean, safe, and fast way to back up and restore partitions using partclone, with a simple Zenity‑based UI and strong integrity guarantees.
Features

    Ghost‑style simplicity — pick a partition, pick a filename, done

    Streaming backup pipeline (no temp files, no double I/O)

    Fast compression with lz4, zstd, or gzip

    Automatic SHA‑256 checksums for every image

    Chunked image support (e.g., 2 GB / 4 GB chunks for FAT32 or portability)

    Metadata‑rich JSON describing filesystem, backend, layout, checksum, and chunking

    Safety checks to prevent restoring to the wrong partition or filesystem

    Clean restore UI that only shows the correct image entry (e.g., .000 for chunked sets)

Dependencies

Imprint uses standard Linux tools:

    partclone

    zenity

    lz4, zstd, or gzip

    sha256sum

    pkexec (for privilege elevation)

Supported Filesystems

Anything supported by partclone, including ext4, xfs, btrfs, ntfs, fat/exfat, f2fs, and more.

Why Imprint?

Imprint focuses on predictability, transparency, and safety.
If you liked how Ghost “just worked,” Imprint brings that same reliability to modern Linux systems — with better integrity checks, better metadata, and a cleaner UI.

