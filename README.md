# Imprint Disk Imager

Imprint provides a safe way to back up and restore disk partitions using partclone. It includes both a simple Zenity‑based UI and a full command‑line interface for headless or automated workflows.

- You can backup or restore unmounted **non-system** partitions from within your Linux operating system. You can backup and restore **system partitions** by booting with the Imprint Rescue ISO or booting to another Linux installation on the same computer.
- It only backs up the data on the partition -- no raw images that are the same size as the partition you're backing up.


## Features

- **Full GUI and CLI support**  
  Complete backup and restore functionality available interactively or headlessly, with unified progress output and consistent exit codes.

- **Mapper‑aware device handling**  
  Correctly detects and processes raw partitions, **LUKS‑encrypted devices**, and **LVM logical volumes**, including nested mapper stacks.

- **Streaming backup pipeline**  
  No temporary files or double I/O — data streams directly from partclone → compressor → destination.

- **Fast compression**  
  Supports lz4, zstd, and gzip for compatibility.

- **Automatic SHA‑256 checksums**  
  Every image includes a checksum for integrity verification.

- **Chunked image support**  
  Automatic handling of `.000/.001` chunk sets for FAT32, SMB, and portable storage, with robust validation to prevent incomplete restores.

- **Metadata‑rich JSON**  
  Each image includes structured metadata describing filesystem, backend, compression, chunking, and original partition size.  
  A formal schema will be documented for the 1.0 milestone.

- **Streaming image inspection (sniffer)**  
  Identifies compression type, filesystem backend, and Partclone headers **without full decompression**, enabling metadata‑free recovery paths.

- **Safety‑first restore pipeline**  
  Validates partition size, backend, chunk completeness, and metadata before restoring. Prevents restoring to the wrong device or mismatched filesystem.

- **Clean restore UI**  
  Only shows the correct entry for chunked images (e.g., `.000`), reducing user error.

- **Rescue ISO**  
  A lightweight Arch‑based environment for full offline backup/restore or when system partitions must remain unmounted.
  https://github.com/jalongx/imprint_iso_kde

---

## Limitations

THIS IS BETA SOFTWARE. It works fine on my rather complex system but there are bound to be limitations and errors on other system setups. Imprint is stable for everyday use on the tested filesystems and environments, but it has not yet been validated across the full range of Linux distributions, storage hardware, and edge cases.

- It has only been tested on the following filesystems: Ext2/3/4, BTRFS, NTFS, FAT16/32/exFAT, XFS. Other filesystems supported by partclone should work, but they have not yet been formally tested.
- It has only been tested on NFS and SMB network filesystems.
- It inherits partclone's limitations: you cannot restore an image to a partition smaller than the original.
- It cannot restore an image to a bare drive. You have to create a partition big enough for it. The original partition size can be found in the metadata file accompanying the image.
- It currently backs up only one partition at a time in GUI mode. If you want to back up 3 partitions, you'll have to run it 3 times. However, you can script CLI mode to backup multiple partitions at one.

---

## Background

I’m a long‑time Windows user and gamer who recently moved to desktop Linux. I relied on partition imaging for backup/restore for decades. None of the backup software I have previously used would work on ext4 or btrfs.
The Linux options I found required booting rescue ISOs that often lacked support for newer hardware (USB4/Thunderbolt enclosures, NVMe bridges, etc.). I also didn’t want to reboot into an ISO every time I needed to image a partition.

So I began writing a small wrapper around partclone… then added features… then added a rescue ISO… 

Imprint now lets me easily create or restore an image for any partition I can safely unmount. For core system partitions, I boot into a tiny maintenance/rescue Linux installation (a habit from the early 1990s) and run Imprint from there. Once the rescue ISO was working, I decided that releasing the tool might benefit both long-time Linux users and other windows refugees like myself.

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

A lightweight Arch and KDE‑based rescue environment is available for full offline backup and restore, or when system partitions must remain unmounted. The ISO uses a modern Arch Linux kernel with broad hardware support, UEFI/BIOS boot, and standard driver coverage. It is designed for modern hardware; older systems may work but are untested (I don't have any older or BIOS-based systems to test it on). It includes network drivers and supports LAN and WiFi. Tested with NFS and SMB. Other network backends may work if they behave like a normal mounted filesystem (e.g., they appear as a normal directory), but they are not officially tested. 

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

Encrypted volumes (e.g., LUKS) must be unlocked before use; Imprint backs up the underlying filesystem, not encrypted containers. LVM volumes work as long as the logical volume is active; Imprint backs up the filesystem inside the LV.

---

## Roadmap (toward 1.0 and beyond)

Imprint is currently **version 0.92** — stable and fully usable, but still evolving.  
Here’s what’s planned for the 1.0 milestone (and beyond):

- **Command‑line switches** for automation and headless use. ✔️ *Completed*
- **GUI multi‑partition backup/restore** 
- **Sniffer** integration in the restore process to validate essential metadata values when metadata values are missing or corrupted. ✔️ *Completed*
- **Verification‑only mode** (validate images without restoring)
- **Logging** for headless operation.
- **Improved documentation**  

Development is active, and the 1.0 milestone is focused on stability, polish, and core feature completeness rather than rapid expansion. The transition to 1.0 will not break existing images; backward compatibility is a priority.

---

## Screenshots

### Device Selection  
Shows raw partitions, LUKS devices, and LVM logical volumes, sorted for clarity.
<img width="1217" height="1014" alt="backup_dialog" src="https://github.com/user-attachments/assets/27439126-7539-4595-b65a-975ab24f95ca" />

### Image Selection  
Automatically highlights the correct entry for chunked images (e.g., .000).
<img width="1162" height="819" alt="restore_dialog" src="https://github.com/user-attachments/assets/7223ac9a-fe3d-4e9e-837b-7743dfd830b2" />

### Backup Output
Command line backup of a LVM logical volume
<img width="1987" height="1171" alt="backup_output" src="https://github.com/user-attachments/assets/fa962d91-3569-45e7-8f7a-56a79558732e" />

### Restore Output
Command line restore of a LVM logical volume
<img width="1257" height="859" alt="restore_output" src="https://github.com/user-attachments/assets/8013cd36-fcfd-4ca3-b0c9-acacee9ec714" />

### Metadata
<img width="973" height="1470" alt="metadata" src="https://github.com/user-attachments/assets/81047949-2728-403f-922a-23b27d5c8024" />

### Command Line Options
<img width="1028" height="846" alt="imprint_help" src="https://github.com/user-attachments/assets/a60e1260-1c58-41b3-b2fc-e47f49d1b3d0" />

### Imprint Rescue ISO  
<img width="1920" height="1080" alt="imprint_iso_1" src="https://github.com/user-attachments/assets/f6b8b0d2-5662-49a1-9214-3c6c02388025" />

---

## Disclaimer

Imprint includes multiple safety checks and integrity safeguards, but disk imaging always carries inherent risk. If you are uncertain about any step, please ask for help before proceeding.

Imprint is provided without any warranty. I take no responsibility for any
damage, data loss, or other consequences that may occur to your partitions,
filesystems, or devices.

