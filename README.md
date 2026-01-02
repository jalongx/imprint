I am a refugee from windows who has long been used to backing up my system with partition images. The only linux solutions I found involved
booting with rescue ISOs that sometimes lacked support for newer hardware (like USB4/Thunderbolt enclosures). I also didn't like having to boot to an ISO
whenever I wanted to make an image of a partition. So I began writing a wrapper for partclone, began adding new features and here we are. I can now easily create
or restore an image for any partition that I can safely unmount. For core system partitions, I boot to a small maintenance/rescue partition (a habit from the early 1990's)
and run the backup/restore from there. By the time I had added a rescue ISO, I figured I might as well release this program.

---

# Imprint Disk Imaging

---


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

Backup Dialog
<img width="926" height="1362" alt="imprint_bkup_1" src="https://github.com/user-attachments/assets/632d0e30-f302-4150-b605-3ccf0f892a98" />

Restore Dialog
<img width="1165" height="1303" alt="imprint_restore_1" src="https://github.com/user-attachments/assets/19e9921a-3bf1-4c9f-b621-8d2313f78f6a" />
