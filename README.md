# HDDHealth Monitor

**Free & Open Source** — v1.3

Checks HDD/SSD/NVMe health by reading raw S.M.A.R.T. data straight from the drives. Built with Visual Studio 2010, compiles anywhere without messing with SDK paths. No runtime dependencies — everything's statically linked into one .exe.

---

## Author

**Ari Sohandri Putra** — [GitHub Sponsors](https://github.com/sponsors/arisohandriputra/)

License: [MIT](./LICENSE) — 100% free, no strings attached.

---

## What It Does

- Reads S.M.A.R.T. data from ATA/SATA, USB (via SAT bridge), and NVMe drives
- Shows health %, temperature, power-on hours, RPM (for mechanical HDDs)
- Detects 42+ drive vendors (Samsung, WDC, Seagate, Phison, Silicon Motion, Innogrit, YMTC, etc.)
- Tray notifications when a drive starts going bad (thresholds are configurable)
- Hot-plug aware — USB drives show up the moment you plug them in
- Export reports to TXT or JSON for documentation
- 5-second sequential read benchmark with live progress bar
- Drive Info button — looks up the selected drive in a built-in database (baked into the .exe) and shows full specs
- System info dialog (OS, CPU, RAM, drive count)
- Save screenshots as PNG (via GDI+)

---

## Building with VS2010

1. Extract the zip to a path without spaces (e.g. `C:\Projects\HDDHealth`)
2. Double-click `HDDHealth-VS2010.sln`
3. Pick `Release` + `Win32`
4. Hit **F7**
5. Output lands in `bin\HDDHealth.exe`

Or from the command line:
```cmd
msbuild HDDHealth-VS2010.sln /p:Configuration=Release /p:Platform=Win32
```

### Other VS versions (2012-2022)

Open `HDDHealth/HDDHealth.vcxproj` in a text editor and swap `v100` for:
- VS2012: `v110` | VS2013: `v120` | VS2015: `v140`
- VS2017: `v141` | VS2019: `v142` | VS2022: `v143`

---

## Menu Layout

```
File
├── Save Screenshot       Ctrl+S
├── Save Report...        Ctrl+R     (TXT)
├── Save JSON Report...   Ctrl+J     (JSON)
└── Exit

Tools
├── Benchmark Drive...   Ctrl+B     (5-sec test, has a progress bar)
├── Settings...                     (alert thresholds + refresh interval)
└── System Information               (OS / CPU / RAM / drives)

Help
├── Donate...
└── About HDDHealth Monitor
```

---

## Main Window

There's also a **Drive Info** button next to **Save Report** that pops up a dialog with full specs from the built-in database.

The main window shows 9 info fields per drive:

| Field | What it shows |
|-------|---------------|
| Model | Drive model string |
| Vendor | From the built-in database if available, otherwise auto-detected |
| Serial No. | Serial number |
| Firmware | Firmware version |
| Capacity | From the database (e.g. `500.1 GB (465.7 GiB)`) if available, otherwise auto-detected |
| Temperature | Current temp (°C) |
| S.M.A.R.T. | SMART status + access method |
| Type | From the database if available (HDD, SSD, etc.) |
| Power-On Hours | Total hours on + years conversion (e.g. `4321 hrs (0.5 yrs)`) |
| Health % | Color-coded bar (green/yellow/red) |
| Performance % | Color-coded bar |

---

## Drive Info Dialog

Clicking the **Drive Info** button opens a dialog with general specs pulled from the built-in database (embedded in the .exe as a resource — no external files needed):

- Trademark
- Model
- Capacity
- Form-factor
- Type
- Interface
- SATA version
- Controller
- Firmware
- Firmware upgrade
- ATA version
- Advanced Format
- Rotation rate

The database is maintained by the developer and baked into the .exe via `drives.json` (compiled as an `RT_RCDATA` resource). To add or update entries, just edit `src/drives.json` and rebuild — the resource compiler embeds it automatically.

---

## Settings

`Tools → Settings` lets you tweak:

- **Temperature Alerts**: Warning threshold (°C), Critical threshold (°C)
- **Health Alerts**: Warning threshold (%), Critical threshold (%)
- **Refresh**: Interval in seconds (1-60)

Settings are saved to `%APPDATA%\HDDH\settings.ini` and take effect immediately. The alert system uses these thresholds for tray notifications, and the refresh timer gets reset when you change the interval.

---

## Sample Report Output

**TXT format:**
```
Model           : Samsung SSD 980 PRO 2TB
Vendor          : Samsung
Serial Number   : S5GXNX0R123456W
Firmware        : 5B2QGXA7
Capacity        : 1.9 TB
Type            : NVMe
Controller      : NVMe PCIe (NVMe Protocol Query)
Rotation Rate   : 5400 rpm
Temperature     : 42 C
Health %        : 98
Power-On Hours  : 4321
```

**JSON format:**
```json
{
  "model": "Samsung SSD 980 PRO 2TB",
  "vendor": "Samsung",
  "capacity": "1.9 TB",
  "type": "NVMe",
  "controller": "NVMe PCIe (NVMe Protocol Query)",
  "rotation_rate": "5400 rpm",
  "temperature_c": 42,
  "health_percent": 98
}
```

---

## Runtime Notes

- Needs admin (UAC prompt) — raw disk access requires it
- Silent startup with `/minimized` switch
- DPI-aware, won't blur on HiDPI screens
- Settings live in `%APPDATA%\HDDH\settings.ini`
- Reports land in `<Documents>\HDDH_Reports\`
- The drive database is embedded in the .exe — nothing extra to distribute

---

## Version History

- **1.3** (current) — Big update: 22 new vendors, 50+ SMART attributes, Drive Info button with embedded database, benchmark with progress bar, settings that actually work, RPM for HDDs, Vendor/Capacity/Type from JSON
- **1.2** — Swapped History Graph for Save Report, added VS2010 solution
- **1.1** — Initial release, MinGW-only

See [CHANGELOG.md](./CHANGELOG.md) for the full story.

---

## Credits

Inspired by CrystalDiskInfo, smartmontools, and HDDScan. Shoutout to those projects.

---

If this tool saves your data, consider sponsoring!

> https://github.com/sponsors/arisohandriputra/
