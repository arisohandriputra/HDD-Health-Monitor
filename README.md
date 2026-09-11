# HDDHealth Monitor

**100% Free and Open Source Software (FOSS)**

A low-level Windows utility that reads raw S.M.A.R.T. data directly from
physical drives via `DeviceIoControl` and presents it through a clean,
modern GUI. Built and tested with **Visual Studio 2010**, also compatible
with **MinGW / TDM-GCC**.

---

## Author

**Ari Sohandri Putra** — [GitHub Sponsors](https://github.com/sponsors/arisohandriputra/)

If you find this tool useful, please consider supporting the author.

## License

[MIT License](./LICENSE) — 100% Free and Open Source Software.

---

## Features

- **Multi-transport S.M.A.R.T. data acquisition**
  - ATA / SATA drives via `IOCTL_ATA_PASS_THROUGH_DIRECT`
  - USB bridge chips (JMicron, ASMedia, Realtek, Cypress, VLI, ...) via
    `IOCTL_SCSI_PASS_THROUGH_DIRECT` using SAT (SCSI-ATA-Translation)
  - NVMe drives via `IOCTL_STORAGE_QUERY_PROPERTY` on the native
    Microsoft NVMe driver (reads Health Info Log 0x02)

- **Per-drive health dashboard**
  - Health percentage and performance metric
  - Full S.M.A.R.T. attribute table (ID, value, worst, raw, status)
  - Temperature / health / failure critical alerts via tray notifications
  - Hot-plug aware (USB drives detected on arrival)
  - Multi-drive tray icons

- **Save Report** (new in v1.2)
  - One-click export of the full drive health status to a plain-text
    `.txt` file in `<Documents>\HDDH_Reports\`
  - Covers every detected drive with identity, capacity, SMART status,
    NVMe Health Log fields, or the ATA attribute table
  - Report filename is timestamped so reports never overwrite each other

- **Save Screenshot** (PNG via GDI+)

---

## Building with Visual Studio 2010

### Prerequisites

- **Microsoft Visual Studio 2010** (any edition, including the free
  *Visual C++ 2010 Express*).
- Windows SDK 7.0A / 7.1 (bundled with VS2010).
- Target OS at runtime: Windows Vista / 7 / 8 / 8.1 / 10 / 11.

### Open and build

1. Unzip the release to a path **without spaces**
   (e.g. `C:\Projects\HDDHealth-VS2010`).
2. Double-click `HDDHealth-VS2010.sln` (or open from inside VS2010).
3. Pick `Release` and `Win32` in the toolbar.
4. Press **F7** (Build Solution).
5. Output: `HDDHealth-VS2010\bin\HDDHealth.exe`.

### Command-line build

Open the *Visual Studio 2010 Command Prompt* and run:

```cmd
cd C:\Projects\HDDHealth-VS2010
msbuild HDDHealth-VS2010.sln /p:Configuration=Release /p:Platform=Win32
```

### Using a newer Visual Studio

If you only have VS2012 / 2013 / 2015 / 2017 / 2019 / 2022 installed,
open `HDDHealth/HDDHealth.vcxproj` in a text editor and replace every
`<PlatformToolset>v100</PlatformToolset>` with the toolset identifier
matching your Visual Studio:

| Visual Studio | PlatformToolset |
| ------------- | ---------------- |
| 2012          | `v110`           |
| 2013          | `v120`           |
| 2015          | `v140`           |
| 2017          | `v141`           |
| 2019          | `v142`           |
| 2022          | `v143`           |

All other project settings are forward-compatible with newer toolsets.

---

## Building with MinGW / TDM-GCC (legacy)

The original Makefile is preserved as `Makefile.original` for reference.
To build with MinGW, rename it to `Makefile` and run:

```bash
# MinGW / MSYS2 shell
make

# TDM-GCC
mingw32-make
```

The output binary lands at `bin/HDDHealth.exe`, identical to the VS2010
build in functionality.

---

## Project structure

```
HDDHealth-VS2010/
├── CHANGELOG.md                  Full version history
├── HDDHealth-VS2010.sln          Visual Studio 2010 solution
├── LICENSE                       MIT License
├── README.md                     This file
├── Makefile.original             Original MinGW Makefile (reference)
└── HDDHealth/
    ├── HDDHealth.vcxproj         MSBuild project (v100, ToolsVersion 4.0)
    ├── HDDHealth.vcxproj.filters Solution Explorer grouping
    ├── HDDHealth.vcxproj.user    Local debugger settings
    ├── README_VS2010.txt         Build / run / troubleshooting guide
    ├── bin/                      Output folder (created on build)
    ├── obj/                      Intermediate folder (created on build)
    └── src/
        ├── main.cpp              WinMain + single-instance mutex
        ├── mainwnd.cpp           Main window + Save Report feature
        ├── smart.cpp             Low-level S.M.A.R.T. (ATA / USB / NVMe)
        ├── donate.cpp            Donate dialog (GitHub Sponsors)
        ├── mainwnd.h             Main window declarations
        ├── smart.h               S.M.A.R.T. public interface
        ├── donate.h              Donate module header
        ├── mingw_compat.h        MinGW header shims (harmless under MSVC)
        ├── resource.h            Resource IDs
        ├── app.rc                Resource script (icon + manifest + version)
        ├── app.manifest          Common-Controls v6 + requireAdministrator
        └── app.ico               Application icon
```

---

## Save Report — sample output

Clicking the **Save Report** button (or `File → Save Report...`, `Ctrl+R`)
writes a timestamped `.txt` file to `<Documents>\HDDH_Reports\` and opens
Explorer to show it:

```
================================================================
  HDDHealth Monitor - Drive Health Report
  Generated: 2026-09-11 14:23:08
  Author : Ari Sohandri Putra (ARImetic Inc.)
  Sponsor: https://github.com/sponsors/arisohandriputra/
  License: MIT (100% Free Open Source Software)
================================================================

Drives detected: 1

----------------------------------------------------------------
DRIVE 1 of 1
----------------------------------------------------------------
Model           : Samsung SSD 980 PRO 2TB
Serial Number   : S5GXNX0R123456W
Firmware        : 5B2QGXA7
Capacity        : 1.9 TB
Type            : NVMe
Vendor          : Samsung
Health Status   : Good
SMART Supported : Yes
SMART Enabled   : Yes
Access Method   : NVMe Protocol Query
Temperature     : 42 C
Health %        : 98
Performance %   : 100
Power-On Hours  : 4321
Power Cycles    : 187
Read Speed      : 6800 MB/s
Predict Failure : No

---- SMART Attributes / Health Log ----

  01h  Critical Warning             : 0x00
  02h  Composite Temperature        : 42 C (315 K)
  03h  Available Spare              : 100 %
  04h  Available Spare Threshold    : 10 %
  05h  Percentage Used (Endurance)  : 2 %
  06h  Data Units Read              : 12345678 (6028.1 GB)
  07h  Data Units Written           : 9876543 (4822.5 GB)
  09h  Power On Hours               : 4321
  0Ch  Power Cycles                 : 187
  10h  Unsafe Shutdowns             : 3
  11h  Media & Data Integrity Errors: 0
  12h  Error Log Entries            : 0
  13h  Warning Comp Temp Time       : 0 min
  14h  Critical Comp Temp Time      : 0 min

================================================================
  End of Report
================================================================
```

For ATA / SATA drives the attribute section becomes a full SMART
attribute table:

```
  ID    Name                              Value  Worst  Threshold  Raw          Status
  ----  --------------------------------  -----  -----  ---------  -----------  -------
  0x05  Reallocated Sectors Count          100    100         10            0  OK
  0x09  Power-On Hours Count               100    100          0         4321  OK
  0x0C  Power Cycle Count                  100    100          0          187  OK
  0xC2  Temperature                        100    100          0           42  OK
  0xC5  Current Pending Sector Count       100    100          0            0  OK
  0xC6  Uncorrectable Sector Count          100    100          0            0  OK
  ...
```

---

## Version history

See **[CHANGELOG.md](./CHANGELOG.md)** for the full version history.

### Highlights

- **1.2** (current) — Replaced the experimental History Graph feature
  with a more practical Save Report button that exports a full drive
  health report to `.txt`. Native Visual Studio 2010 solution added.
  Code-level compatibility fixes for MSVC v100 (`#include <stdlib.h>`
  in `smart.cpp`, dynamic `cfgmgr32.dll` loader).

- **1.1** (original release) — MinGW-only build. Per-drive history
  graph window, S.M.A.R.T. acquisition across ATA / USB-SAT / NVMe,
  tray notifications, PNG screenshots.

---

## Runtime notes

- The `.exe` requests administrator privileges via its manifest
  (`requireAdministrator`) because raw-disk IOCTLs require admin
  rights. Windows will prompt UAC on launch.
- Silent startup is supported via the `/minimized` command-line switch
  (e.g. drop a shortcut in the Windows Startup folder that runs
  `HDDHealth.exe /minimized`).
- The `.exe` is DPI-aware (no virtualization on high-DPI displays).
- Common-Controls v6 are required for the modern widget look
  (bundled with every Windows version since XP SP2).

---

## Acknowledgments

Inspired by the long tradition of low-level disk tools including
CrystalDiskInfo, smartmontools, and HDDScan. The S.M.A.R.T. attribute
classification tables and USB bridge detection logic borrow ideas from
those projects.

---

Enjoy — and if it saves your data, consider sponsoring!

> https://github.com/sponsors/arisohandriputra/
