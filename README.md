# Imprint Disk Imager

Imprint is a modern, Linux‑native reimagining of the classic Norton Ghost workflow.  
It provides a clean, safe, and fast way to back up and restore partitions using partclone, with a simple Zenity‑based UI and strong integrity guarantees.

---

## Background

I’m a long‑time Windows user who is now a happy refugee with Linux. I relied on partition imaging for decades.  
The Linux options I found required booting heavy rescue ISOs that often lacked support for newer hardware (USB4/Thunderbolt enclosures, NVMe bridges, etc.). I also didn’t want to reboot into an ISO every time I needed to image a partition.

So I began writing a small wrapper around partclone… then added features… then added a rescue ISO… and eventually realized I had built something worth sharing.

Imprint now lets me easily create or restore an image for any partition I can safely unmount. For core system partitions, I boot into a tiny maintenance/rescue partition (a habit from the early 1990s) and run Imprint from there. Once the rescue ISO was working, releasing the tool publicly felt like the right next step.

---

## Features

- **Ghost‑style simplicity** — pick a partition, pick a filename, done  
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

This makes Imprint a safe, modern alternative to classic tools like Norton Ghost, Clonezilla, or Acronis — with a clean UI and strong integrity guarantees.

---

## Imprint Rescue ISO

A lightweight KDE‑based rescue environment is available for full offline backup and restore, or when system partitions must remain unmounted.

👉 https://github.com/jalongx/imprint_iso_kde

If you can’t afford to donate but need a prebuilt ISO, open an issue and I’ll make one available.

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

## Why Imprint?

Imprint focuses on **predictability**, **transparency**, and **safety**.

If you liked how Ghost “just worked,” Imprint brings that same reliability to modern Linux systems — with better integrity checks, better metadata, and a cleaner UI. It’s designed to be deterministic, human‑readable, and easy to trust.

---

## Screenshots

### Backup Dialog  
<img width="926" height="1362" alt="imprint_bkup_1" src="https://github.com/user-attachments/assets/632d0e30-f302-4150-b605-3ccf0f892a98" />

### Restore Dialog  
<img width="1165" height="1303" alt="imprint_restore_1" src="https://github.com/user-attachments/assets/19e9921a-3bf1-4c9f-b621-8d2313f78f6a" />

### Imprint Rescue ISO  
<img width="1920" height="1080" alt="imprint_iso_1" src="https://github.com/user-attachments/assets/f6b8b0d2-5662-49a1-9214-3c6c02388025" />

---

## Roadmap (toward 1.0)

Imprint is currently **version 0.90** — stable and fully usable, but still evolving.  
Here’s what’s planned for the 1.0 milestone:

- **Unified `imprint` binary** (merge backup + restore into one tool)  
- **Command‑line switches** for automation and headless use  
- **Multi‑partition backup/restore**  
- **Verification‑only mode** (validate images without restoring)  
- **Improved documentation**  
- **Optional supporter perks** (prebuilt ISOs, convenience features)

---

## Supporting the Project

Imprint is built and maintained by a retired senior on a fixed income living in uncertain times.  
If this tool helps you, or if you’d like to support the continued development toward 1.0, consider becoming a sponsor.

Your support helps fund:

- ongoing maintenance  
- hardware testing  
- rescue ISO improvements  
- future features from the roadmap  

Supporters may also receive access to **prebuilt rescue ISOs** and other convenience perks as a thank‑you.

---

## Disclaimer

Working with disk images is inherently risky. If you are uncertain about any step, please ask for help before proceeding.  
I take no responsibility for any damage or data loss that may occur to your partitions, filesystems, or devices.
