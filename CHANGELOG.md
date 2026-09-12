# Changelog

History of changes for HDDHealth Monitor. Free-form, nothing fancy.

---

## v1.3 — Big Update

The biggest release so far. Three main things: way more hardware support, a bunch of new tools, and a built-in drive database.

### New Stuff

**Drive Info button** (next to Save Report on the main window):
- Looks up the selected drive in a built-in database that's baked right into the .exe (no external files)
- Shows a dialog with general specs: trademark, model, capacity, form-factor, type, interface, SATA version, controller, firmware, firmware upgrade, ATA version, advanced format, rotation rate
- The database (`drives.json`) is embedded as an `RT_RCDATA` resource — everything ships in one .exe
- Maintained by the developer, end users see it read-only
- Main window fields (Vendor, Capacity, Type) now pull from this database too, falling back to auto-detected values if the drive isn't in the database yet

**New Tools menu:**
- `File → Save JSON Report (Ctrl+J)` — exports reports as JSON, handy for scripts or dashboards
- `Tools → Benchmark Drive (Ctrl+B)` — 5-second sequential read test with a live progress bar (0-100%), runs in a background thread, shows result in MB/s with one decimal place
- `Tools → Settings` — tweak alert thresholds (temperature warning/critical, health warning/critical) and refresh interval (1-60 seconds). These actually work now — the alert system reads from these settings, and the refresh timer resets when you change the interval. Saved to `%APPDATA%\HDDH\settings.ini`
- `Tools → System Information` — shows OS version, CPU brand string, total/available RAM, drive count

**New fields on the main window:**
- **Vendor** — from the database if available, otherwise auto-detected (Samsung, WDC, Seagate, Phison, etc.)
- **Type** — from the database if available (HDD, SSD, etc.)
- The old "Sec. Speed" field was replaced with **Power-On Hours** — way more useful for day-to-day monitoring. Format: `4321 hrs (0.5 yrs)`. Read speed is now in Tools → Benchmark.
- Main window layout went from 7 rows to 9

**Way more hardware support:**
- 22 new drive vendors: Phison, Silicon Motion, Innogrit, Realtek SSD, YMTC, Maxio, HiKsemi, Leven, Patriot, Gigabyte, ASRock, Seagate Exos, Fujitsu, Quantum, Maxtor, Fusion-io, Micron Enterprise, Solidigm, KLEVV, Netac, TeamGroup T-Force, AORUS (42 total now)
- 20+ new USB bridge VID/PID pairs (ASMedia ASM235CM, Realtek RTL9210B/9220/9230, VIA VL716/717, JMicron JMS586, LaCie, Lenovo, OCZ, Corsair, Genesys Logic, ENE, Satechi, etc.)
- 50+ new SMART attribute IDs for vendor-specific SSD diagnostics (Phison E12/E13/E19/E25, Innogrit IG5236, SMI SM2262/2263/2270, YMTC PC411, Maxio MAP1202, Solidigm P41/P44, Samsung PM9A3, eMMC life-time, etc.)

### What Changed

- Main window now does a database lookup on every drive update — Vendor, Capacity, and Type all pull from the embedded JSON, falling back to auto-detected values
- TXT and JSON reports also pull from the database, so the report data matches what you see on the main window
- Settings dialog redesigned: group boxes for Temperature/Health/Refresh, clear labels, working OK/Cancel/Close buttons
- Benchmark dialog has a live progress bar and the Close button only shows up after the test finishes
- Alert system is now wired to the Settings values — temperature and health thresholds actually get used when checking for alerts
- Refresh timer gets reset after Settings closes, and alert states get cleared so they re-evaluate with the new thresholds
- HDDs show RPM in the Controller field of the Drive Info dialog (pulled from ATA IDENTIFY word 217)
- JSON parser handles escape sequences (`\"`, `\\`, `\/`) so values like `2.5"` work correctly

### What Got Removed

- **SMART Self-Test launcher** — gone from the menu. The underlying log-reading functions in smart.cpp are still there, just no UI to launch new tests.
- **Multi-language system** — completely ripped out. Had a nasty bug where the string table indexing was off, causing labels and button text to be wrong. Now everything is hardcoded English — simpler and actually works.
- **History Graph** — was already removed in v1.2, stays removed.
- The "Controller" field on the main window was replaced with "Type" (which pulls from the database). Controller info still shows up in the Drive Info dialog and in the reports.

### Build Stuff

- Visual Studio 2010 (v100 toolset). Works with VS2012-2022, just change the PlatformToolset
- `cfgmgr32.lib` is loaded dynamically via LoadLibrary (no static link needed)
- Added `#include <stdlib.h>` to smart.cpp (MSVC v100 needs it explicitly for strtol)
- Manifest is handled through app.rc, not the linker's embed step (avoids CVT1100 duplicate manifest)
- `GenerateManifest=false` and `EmbedManifest=false` in the project settings
- `drives.json` is compiled into the .exe as an `RT_RCDATA` resource — no post-build copy needed

### New Files

- `src/tools.h` + `src/tools.cpp` — JSON export, benchmark, settings, system info
- `src/drive_db.h` + `src/drive_db.cpp` — drive database lookup (loads from embedded resource) + display dialog
- `src/drives.json` — the database file, embedded into the .exe via `app.rc`

### How to Verify

After building, check:
- Window title says `HDDHealth Monitor 1.3`
- Menu bar has File, Tools, Help
- Main window has 9 info rows, with Vendor and Type pulling from the database
- The "Drive Info" button (next to Save Report) opens a dialog with 13 fields of general info
- Pick a mechanical HDD → Drive Info should show rotation rate if it's in the database
- `Tools → Settings` → dialog has group boxes, clear labels, OK/Cancel/Close all work
- Change alert thresholds in Settings → tray notifications use the new values
- Change refresh interval in Settings → timer picks up the new interval immediately
- `Tools → Benchmark` → progress bar runs for 5 seconds, then shows MB/s
- `File → Save Report` and `File → Save JSON Report` → data matches the main window
- `File → Save JSON Report` → `.json` file shows up in `Documents\HDDH_Reports\`

---

## v1.2 — Save Report + VS2010 Solution

Replaced the History Graph (which was experimental) with a more practical Save Report feature. Added a native VS2010 solution so it builds cleanly in the IDE.

### What's New

- **Save Report** — exports a report to `.txt` in `Documents\HDDH_Reports\`. Covers every drive: model, serial, firmware, capacity, SMART status, NVMe Health Log fields, or ATA attribute table.
- **Native VS2010 solution** — `.sln`, `.vcxproj`, `.vcxproj.filters`, `.vcxproj.user`
- **README_VS2010.txt** — build/run/troubleshoot guide
- **Makefile.original** — original MinGW Makefile kept for reference

### What Changed

- Build system: dual MinGW + VS2010 support
- `smart.cpp` got `#include <stdlib.h>` (MSVC v100 needs it for strtol)
- `smart.cpp` loads `cfgmgr32.dll` dynamically (avoids LNK1104 on VS2010 Express)
- VS2010 project: `GenerateManifest=false`, `EmbedManifest=false`, dropped `cfgmgr32.lib` from dependencies
- Version bumped 1.1 → 1.2

### What Got Removed

- **History Graph subsystem** (smart_history.h + smart_history.cpp) — the window, button, menu entry, all of it. The `history.dat` file in `%APPDATA%\HDDH\` is no longer read, safe to delete.

---

## v1.1 — Initial Release

First release. MinGW-only, no VS2010 project files.

### Features

- Health % and performance metric per drive
- Full S.M.A.R.T. attribute table (ID, value, worst, raw, status)
- Tray notifications for temperature/health/failure alerts
- Hot-plug aware (USB drives detected on plug-in)
- Per-drive history graph (experimental, removed in v1.2)
- Save screenshot as PNG via GDI+
- Multi-drive tray icons
- ATA/SATA via IOCTL_ATA_PASS_THROUGH_DIRECT
- USB via IOCTL_SCSI_PASS_THROUGH_DIRECT (SAT)
- NVMe via IOCTL_STORAGE_QUERY_PROPERTY (Health Log 0x02)
