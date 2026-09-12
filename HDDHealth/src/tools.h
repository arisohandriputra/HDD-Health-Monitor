/* ============================================================================
 *  HDDHealth Monitor 1.3 - Tools module interface
 *
 *  Author  : Ari Sohandri Putra
 *  Sponsor : https://github.com/sponsors/arisohandriputra/
 *  License : MIT
 *
 *  Declares: Settings (load/save/dialog), JSON report export, benchmark,
 *  and system info dialog.
 * ============================================================================
 */
#pragma once
#ifndef TOOLS_H
#define TOOLS_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "smart.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Settings - persistent alert thresholds                             */
/*  Stored at %APPDATA%\HDDH\settings.ini                              */
/* ------------------------------------------------------------------ */

typedef struct _APP_SETTINGS {
    int  nTempWarnC;         /* default 55 */
    int  nTempCritC;         /* default 65 */
    int  nHealthWarnPct;     /* default 40 */
    int  nHealthCritPct;     /* default 20 */
    int  nRefreshIntervalMs; /* default 5000 */
} APP_SETTINGS;

extern APP_SETTINGS g_Settings;

/* Load / save from %APPDATA%\HDDH\settings.ini */
void Settings_Load(void);
void Settings_Save(void);

/* Modal dialog. Returns TRUE if user pressed OK and settings changed. */
BOOL Settings_ShowDialog(HWND hParent);

/* ------------------------------------------------------------------ */
/*  JSON report export                                                 */
/* ------------------------------------------------------------------ */

/* Saves a JSON-formatted report of all detected drives to:
 *   <Documents>\HDDH_Reports\HDDH_Report_YYYYMMDD_HHMMSS.json
 * Returns TRUE on success; shows a MessageBox on failure. */
BOOL Tools_SaveJSONReport(HWND hWnd);

/* ------------------------------------------------------------------ */
/*  Quick sequential read benchmark                                     */
/* ------------------------------------------------------------------ */

/* Shows the benchmark dialog. Performs a quick (5-second) sequential
 * read benchmark of the currently selected drive using ReadFile on
 * \\.\PhysicalDriveN. Reports read MB/s with a live progress bar. */
void Tools_ShowBenchmarkDialog(HWND hWnd);

/* ------------------------------------------------------------------ */
/*  System information dialog                                          */
/* ------------------------------------------------------------------ */

/* Shows a dialog with: Windows version, CPU brand string, total RAM,
 * and total detected drives. Useful for support / bug reports. */
void Tools_ShowSystemInfoDialog(HWND hWnd);

#ifdef __cplusplus
}
#endif

#endif /* TOOLS_H */
