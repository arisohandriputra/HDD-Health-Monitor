# Changelog

All notable changes to **HDDHealth Monitor** are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.2] - 2026-09-11

### Summary

First public release shipping as a **native Visual Studio 2010 solution**.
Compared to the original 1.1 MinGW-only distribution, this version replaces
the experimental per-drive history graph feature with a more practical
**Save Report** function that exports the full drive health status to a
plain-text `.txt` file. The project file layout has also been reworked to
build cleanly inside the classic VS2010 IDE without any external SDK
configuration.

### Removed

- **Per-drive History Graph feature** (entire subsystem).
  - Removed files:
    - `src/smart_history.h`
    - `src/smart_history.cpp`
  - Removed UI elements:
    - "History Graph" button on the main window (`IDC_HISTORY_BTN_MAIN`)
    - "View → History Graph" menu entry (`IDM_HISTORY`)
  - Removed runtime calls:
    - `History_Init()`, `History_Load()`, `History_Save()`,
      `History_Record()`, `History_FindSlot()`, `History_Clear()`
    - `Graph_RegisterClass()`, `Graph_ShowWindow()`, `Graph_Repaint()`,
      `Graph_DestroyAll()`, `Graph_Paint()`
  - Removed persistent state:
    - The rolling 120-sample buffer kept in `%APPDATA%\HDDH\history.dat`
      is no longer written or read. Existing `history.dat` files left
      over from version 1.1 can be safely deleted; this version simply
      ignores them.

  The history graph was an experimental visualization that required
  continuous background sampling and a custom-drawn graph window. In
  practice it added complexity without delivering enough value to most
  users, so it has been replaced by the on-demand Save Report feature
  below.

### Added

- **Save Report feature** - exports a complete drive health snapshot to a
  plain-text file.

  - Output location: `<Documents>\HDDH_Reports\HDDH_Report_YYYYMMDD_HHMMSS.txt`
  - Naming uses the local date and time so reports never overwrite each
    other.
  - A success dialog offers to open the containing folder in Explorer
    with the new report pre-selected.
  - The report covers every detected drive and includes:

    - Drive identity (model, serial, firmware)
    - Capacity (formatted as TB / GB / MB as appropriate)
    - Drive type (HDD / SSD-SATA / NVMe / USB / etc.)
    - Detected vendor (Samsung, WDC, Seagate, ...)
    - Overall health status (Good / Caution / Bad)
    - SMART supported / enabled flags
    - Access method used (ATA Passthrough / SAT / Storage Query / ...)
    - Temperature in Celsius
    - Health % and Performance %
    - Power-on hours and power cycle count
    - Read speed (MB/s)
    - Predictive failure flag
    - USB bridge VID/PID and vendor/product strings (for USB drives)

  - The per-drive attribute table differs by transport:

    - **ATA / SATA**: full 30-attribute SMART table with
      ID, name, value, worst, threshold, raw value, and status
      (OK / Warning / FAILED).
    - **NVMe**: the NVMe Health Log (Log Page 02h) fields, including
      Critical Warning, Composite Temperature, Available Spare,
      Percentage Used, Data Units Read/Written, Power On Hours,
      Power Cycles, Unsafe Shutdowns, Media Errors, Error Log Entries,
      and the warning/critical composite temperature times.
    - **USB bridges without SAT**: a clear note explaining that SMART
      attributes are not exposed by the bridge chip.

- **New menu entry**: `File → Save Report...   Ctrl+R` triggers the
  report export.
- **New toolbar button**: "Save Report" replaces the old "History Graph"
  button at the same screen location.
- **Native Visual Studio 2010 solution files**:
  - `HDDHealth-VS2010.sln` (Format Version 11.00)
  - `HDDHealth/HDDHealth.vcxproj` (ToolsVersion 4.0, PlatformToolset v100)
  - `HDDHealth/HDDHealth.vcxproj.filters` (Solution Explorer grouping)
  - `HDDHealth/HDDHealth.vcxproj.user` (local debugger settings)
- **`README_VS2010.txt`** - comprehensive build / run / troubleshooting
  guide for the VS2010 project.
- **`Makefile.original`** - the original MinGW Makefile is preserved
  for reference (not used by the VS2010 build).
- **`CHANGELOG.md`** - this file.

### Changed

- **Build system switched from MinGW-only to dual MinGW + VS2010**.
  The VS2010 build produces a binary that is functionally identical to
  the MinGW build; the only differences are the compiler (`cl.exe` vs.
  `g++`) and the statically-linked C runtime (`/MT` vs. `-static-libgcc`).

- **VS2010 project settings** (faithful port of the original Makefile
  flags):

  | Setting              | Value                                            |
  | -------------------- | ------------------------------------------------ |
  | PlatformToolset      | `v100`   (MSVC 16.0, ships with VS2010)         |
  | CharacterSet         | Multi-Byte (matches MinGW default)             |
  | SubSystem            | `Windows`   (GUI app, no console)               |
  | RuntimeLibrary       | `MultiThreaded` (`/MT`, static CRT)             |
  | ExceptionHandling    | `false`   (matches MinGW `-fno-exceptions`)     |
  | RuntimeTypeInfo      | `false`   (no RTTI needed)                      |
  | Optimization         | `/O2` (MaxSpeed) on Release                      |
  | Linker Manifest       | disabled (manifest embedded via `app.rc`)        |
  | UAC Execution Level  | `RequireAdministrator`   (per `app.manifest`)    |
  | WINVER / _WIN32_WINNT| `0x0600`  (Vista - needed for NVMe IOCTLs)       |
  | GenerateManifest     | `false`   (avoids CVT1100 duplicate manifest)   |
  | EmbedManifest        | `false`   (avoids `mt.exe` step)                |

- **`smart.cpp`**: added `#include <stdlib.h>` (MSVC v100 needs this
  explicitly for `strtol`, while MinGW pulls it in transitively via
  `<ctype.h>`).

- **`smart.cpp`**: `CM_Get_Parent` / `CM_Get_Device_IDA` /
  `CM_Get_Device_ID_Size` are now resolved at runtime via
  `LoadLibraryA("cfgmgr32.dll")` + `GetProcAddress` instead of being
  linked statically against `cfgmgr32.lib`. The DLL ships with every
  Windows version since 2000, so this is always safe at runtime and
  removes the link-time dependency on `cfgmgr32.lib` (which is not on
  the linker search path in some VS2010 Express installs).

- **Version bumped** from `1.1.0.0` to `1.2.0.0` in `src/app.rc`,
  `src/main.cpp` (window title), and `src/mainwnd.cpp`
  (`UpdateWindowTitle()`).

### Project layout

```
HDDHealth-VS2010/
├── CHANGELOG.md                      <-- NEW in 1.2
├── HDDHealth-VS2010.sln              <-- NEW in 1.2
├── LICENSE                           (unchanged)
├── README.md                         (updated for GitHub)
├── Makefile.original                 <-- NEW in 1.2 (renamed from "Makefile")
└── HDDHealth/
    ├── HDDHealth.vcxproj             <-- NEW in 1.2
    ├── HDDHealth.vcxproj.filters     <-- NEW in 1.2
    ├── HDDHealth.vcxproj.user        <-- NEW in 1.2
    ├── README_VS2010.txt             <-- NEW in 1.2
    ├── bin/                          <-- output folder (filled at build)
    ├── obj/                          <-- intermediate folder (filled at build)
    └── src/
        ├── main.cpp                  (version string bumped to 1.2)
        ├── mainwnd.cpp               (Save Report replaces History Graph)
        ├── smart.cpp                 (stdlib.h + dynamic cfgmgr32 loader)
        ├── donate.cpp                (unchanged)
        ├── smart.h                   (unchanged)
        ├── mainwnd.h                 (IDC_SAVE_REPORT_BTN replaces
        │                              IDC_HISTORY_BTN_MAIN;
        │                              IDM_SAVE_REPORT replaces IDM_HISTORY)
        ├── donate.h                  (unchanged)
        ├── mingw_compat.h            (unchanged)
        ├── resource.h                (unchanged)
        ├── app.rc                    (version-info block bumped to 1.2)
        ├── app.manifest              (unchanged)
        └── app.ico                   (unchanged)
```

### Removed files (compared to 1.1)

- `src/smart_history.h`   - entire history graph subsystem removed
- `src/smart_history.cpp` - entire history graph subsystem removed

### Build requirements

- Microsoft Visual Studio 2010 (any edition, including the free
  "Visual C++ 2010 Express" edition).
- Windows SDK 7.0A / 7.1 (bundled with VS2010).
- Target OS at runtime: Windows Vista / 7 / 8 / 8.1 / 10 / 11.

### How to build

1. Unzip to a path **without spaces**
   (e.g. `C:\Projects\HDDHealth-VS2010`).
2. Double-click `HDDHealth-VS2010.sln` (or open it from inside VS2010).
3. In the toolbar, pick `Release` and `Win32`.
4. Press `F7` (Build Solution).
5. Output: `HDDHealth-VS2010\bin\HDDHealth.exe`.

Command-line alternative (VS2010 Command Prompt):

```cmd
cd C:\Projects\HDDHealth-VS2010
msbuild HDDHealth-VS2010.sln /p:Configuration=Release /p:Platform=Win32
```

### Migrating to a newer Visual Studio

If you do not have VS2010 installed but have VS2012 / 2013 / 2015 / 2017 /
2019 / 2022, open `HDDHealth.vcxproj` in a text editor and replace every
occurrence of `<PlatformToolset>v100</PlatformToolset>` with the matching
toolset identifier for your VS version:

| Visual Studio | PlatformToolset |
| ------------- | ---------------- |
| 2012          | `v110`           |
| 2013          | `v120`           |
| 2015          | `v140`           |
| 2017          | `v141`           |
| 2019          | `v142`           |
| 2022          | `v143`           |

All other project settings are forward-compatible with newer toolsets.

### Sample report output

```
================================================================
  HDDHealth Monitor - Drive Health Report
  Generated: 2026-09-11 14:23:08
  Author : Ari Sohandri Putra (ARImetic Inc.)
  Sponsor: https://github.com/sponsors/arisohandriputra/
  License: MIT (100% Free Open Source Software)
================================================================

Drives detected: 2

----------------------------------------------------------------
DRIVE 1 of 2
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

  01h  Critical Warning            : 0x00
  02h  Composite Temperature       : 42 C (315 K)
  03h  Available Spare             : 100 %
  04h  Available Spare Threshold    : 10 %
  05h  Percentage Used (Endurance) : 2 %
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

---

## [1.1] - 2026 (original release)

### Summary

Initial public release of HDDHealth Monitor. Source distributed with a
MinGW / TDM-GCC Makefile (no Visual Studio project files).

### Features

- Per-drive health percentage and performance metric.
- Full S.M.A.R.T. attribute table (ID, value, worst, raw, status).
- Temperature / health / failure critical alerts via tray notifications.
- Hot-plug aware (USB drives detected on arrival).
- Per-drive history graph (health % and individual attribute over time).
- Save-screenshot feature (PNG via GDI+).
- Multi-drive tray icons.
- ATA / SATA drives via `IOCTL_ATA_PASS_THROUGH_DIRECT`.
- USB bridge chips (JMicron, ASMedia, Realtek, Cypress, ...) via
  `IOCTL_SCSI_PASS_THROUGH_DIRECT` using SAT (SCSI-ATA-Translation).
- NVMe drives via `IOCTL_STORAGE_QUERY_PROPERTY` on the native Microsoft
  NVMe driver (reads Health Info Log 0x02).

### Build

Original release was MinGW-only:

```bash
# Native Windows build (in a MinGW / MSYS2 shell)
make

# Native Windows build (in TDM-GCC)
mingw32-make
```

---

## Versioning roadmap

- **1.x** - Win32 native C++ GUI, single-instance, GDI+ screenshots, hot-plug
  awareness, multi-transport S.M.A.R.T. (ATA / USB-SAT / NVMe), and the
  new Save Report feature introduced in 1.2.
- **Future** - Possible additions being considered: NVMe self-test log
  decoding, RAID controller support (Intel RST / AMD RAIDXpert via
  `SRB_IO_CONTROL`), localized UI strings, optional command-line /
  PowerShell exit codes for scripted monitoring, scheduled automatic
  report generation.
