# HDDHealth Monitor

**100% Free and Open Source Software (FOSS)**

<img width="379" alt="image" src="https://github.com/user-attachments/assets/974c3d26-8544-43a2-957b-915e312c5bc6" />
<img width="379" alt="Screenshot 2026-06-17 130619" src="https://github.com/user-attachments/assets/5e2d8ec2-0d8b-438c-aec3-5ef92378cc50" />

---

**Support :**
Windows 11/10/8.1/8/7/Vista

## Author

**Ari Sohandri Putra**

If you find this tool useful, please consider supporting the author via
GitHub Sponsors:

> https://github.com/sponsors/arisohandriputra/

## License

This project is released under the [MIT License](./LICENSE).

## What it does

HDDHealth Monitor is a low-level Windows utility that reads raw
S.M.A.R.T. data directly from physical drives via `DeviceIoControl` and
presents it through a clean, modern GUI.  It supports:

- **ATA / SATA** drives via `IOCTL_ATA_PASS_THROUGH_DIRECT`
- **USB** bridge chips (JMicron, ASMedia, Realtek, Cypress, ...) via
  `IOCTL_SCSI_PASS_THROUGH_DIRECT` using SAT (SCSI-ATA-Translation)
- **NVMe** drives via `IOCTL_STORAGE_QUERY_PROPERTY` on the native
  Microsoft NVMe driver (reads Health Info Log 0x02)

Features include:

- Per-drive health percentage and performance metric
- Full S.M.A.R.T. attribute table (ID, value, worst, raw, status)
- Temperature / health / failure critical alerts via tray notifications
- Hot-plug aware (USB drives detected on arrival)
- Per-drive history graph (health % and individual attribute over time)
- Save-screenshot feature (PNG via GDI+)
- Multi-drive tray icons

## Building

### Prerequisites

- **MinGW-w64** or **TDM-GCC** (any recent GCC with C++ support).

### Build

```bash
# Native Windows build (in a MinGW / MSYS2 shell)
make
```
```bash
# Native Windows build (in a TDM-GCC)
mingw32-make
```

The output binary is placed at `bin/HDDHealth.exe`.

### Clean

```bash
make clean
```

## Project structure

```
HDDHealth/
├── LICENSE              # MIT License
├── README.md            # This file
├── Makefile             # Build configuration
└── src/
    ├── main.cpp         # WinMain entry point, single-instance guard
    ├── mainwnd.h        # Main window declarations
    ├── mainwnd.cpp      # Main window implementation + About dialog
    ├── smart.h          # S.M.A.R.T. public interface
    ├── smart.cpp        # Low-level S.M.A.R.T. acquisition (ATA/USB/NVMe)
    ├── smart_history.h  # History & graph public interface
    ├── smart_history.cpp# History persistence + graph window
    ├── donate.h         # Donate module interface
    ├── donate.cpp       # Donate dialog + GitHub Sponsors link
    ├── resource.h       # Resource IDs
    ├── app.rc           # Win32 resource script (icon + manifest + version)
    ├── app.manifest     # Common-Controls v6 + requireAdministrator
    ├── app.ico          # Application icon
    └── mingw_compat.h   # MinGW SCSI/ATA header shims
```


Enjoy - and if it saves your data, consider sponsoring!
