# Imprint Disk Imager


Imprint provides a safe and fast way to back up and restore partitions using partclone with a simple Zenity‑based UI.

- You can backup or restore unmounted **non-system** partitions from withing your linux operating system.
- You can backup and restore **system partitions** by booting with the Imprint Rescue ISO or booting to another linux installation on the same computer.
- It only backs up the data on the partition -- no raw images that are the same size as the partition you're backing up.
- Can be run from the command line or from an icon on your desktop.

THIS IS BETA SOFTWARE. It works fine on my rather complex system but there are bound to be limitations and errors on other system setups.

- It has only been tested on the follwing filesystems: Ext2/3/4, BTRFS, NTFS, FAT16/32/exFAT.
- It has only been tested on NFS and SMB network filesystems.
- It inherits partclone's limitations: you cannot restore an image to a partition smaller than the original.
- It cannot (yet) restore an image to a bare drive. You have to create a partition big enough for it. The original partition size can be found in the metadata file accompanying the image.
- It currently backs up only one partition at a time. If you want to back up 3 partitions, you'll have to run it 3 times.

---

## Background

I’m a long‑time Windows user and gamer who recently moved to Linux. I relied on partition imaging for backup/restore for decades. None of the backup software I have previously used would work on ext4 or btrfs.
The Linux options I found required booting rescue ISOs that often lacked support for newer hardware (USB4/Thunderbolt enclosures, NVMe bridges, etc.). I also didn’t want to reboot into an ISO every time I needed to image a partition.

So I began writing a small wrapper around partclone… then added features… then added a rescue ISO… 

Imprint now lets me easily create or restore an image for any partition I can safely unmount. For core system partitions, I boot into a tiny maintenance/rescue linux installation (a habit from the early 1990s) and run Imprint from there. Once the rescue ISO was working, I decided that releasing the tool might benefit other windows refugees like myself.

---

## Features

- **Simplicity** — pick a partition, pick a destination, pick a filename, done  
- **Streaming backup pipeline** (no temp files, no double I/O)  
- **Fast compression** with lz4, zstd, or gzip  
- **Automatic SHA‑256 checksums** for every image  
- **Chunked image support** (2 GB / 4 GB chunks for FAT32, SMB, portability)  
- **Metadata‑rich JSON** describing filesystem, backend, layout, checksum, and chunking  
- **Safety checks** to prevent restoring to the wrong partition or filesystem  
- **Clean restore UI** that only shows the correct entry (e.g., `.000` for chunked sets)  
- **Rescue ISO** for full offline backup/restore on any machine  

---

## Using Imprint on Windows Systems

Imprint works perfectly for Windows users when run from the **Imprint Rescue ISO**.  
You do not need Linux installed — simply boot the ISO from a USB stick and you can:

- back up Windows partitions  
- restore Windows partitions  
- image NVMe, SATA, USB, and RAID volumes  
- work offline without touching the installed OS  

This makes Imprint a safe, modern alternative to classic tools like Clonezilla or Acronis — with a clean UI and strong integrity guarantees.

---

## Imprint Rescue ISO

A lightweight KDE‑based rescue environment is available for full offline backup and restore, or when system partitions must remain unmounted.

👉 https://github.com/jalongx/imprint_iso_kde

---

## Dependencies

Imprint uses standard Linux tools:

- partclone  
- zenity  
- lz4, zstd, or gzip  
- sha256sum  
- pkexec (for privilege elevation)

---

## Supported Filesystems

Anything supported by partclone, including:

- ext4  
- xfs  
- btrfs  
- ntfs  
- fat/exfat  
- f2fs  
- and more  

---

## Screenshots

### Backup Dialog  
<img width="926" height="1362" alt="imprint_bkup_1" src="https://github.com/user-attachments/assets/632d0e30-f302-4150-b605-3ccf0f892a98" />

### Restore Dialog  
<img width="1165" height="1303" alt="imprint_restore_1" src="https://github.com/user-attachments/assets/19e9921a-3bf1-4c9f-b621-8d2313f78f6a" />

### Backup Output
<img width="2044" height="1056" alt="backup_output" src="https://github.com/user-attachments/assets/1d1c7408-0d13-44c1-b942-42adea3c44da" />

### Restore Output
<img width="1398" height="791" alt="restore_output" src="https://github.com/user-attachments/assets/16d06707-a6b8-4b49-ad52-228bb7aef39d" />

### Metadata
<img width="925" height="1397" alt="metadata" src="https://github.com/user-attachments/assets/a4fc762a-4665-4b26-acb6-a226cdcebd9f" />

### Imprint Rescue ISO  
<img width="1920" height="1080" alt="imprint_iso_1" src="https://github.com/user-attachments/assets/f6b8b0d2-5662-49a1-9214-3c6c02388025" />

---

## Roadmap (toward 1.0 and beyond)

Imprint is currently **version 0.92** — stable and fully usable, but still evolving.  
Here’s what’s planned for the 1.0 milestone:

- **Unified `imprint` binary** (merge backup + restore into one tool)  
- **Command‑line switches** for automation and headless use  
- **Multi‑partition backup/restore**  
- **Verification‑only mode** (validate images without restoring)  
- **Improved documentation**  
- **Optional supporter perks** (prebuilt ISOs, convenience features)  

---

## Disclaimer

Working with disk images is inherently risky. If you are uncertain about any
step, please ask for help before proceeding.

Imprint is provided without any warranty. I take no responsibility for any
damage, data loss, or other consequences that may occur to your partitions,
filesystems, or devices.

