/* ============================================================================
 *  HDDHealth Monitor 1.3 - Tools module (JSON export, benchmark, etc.)
 *
 *  Author  : Ari Sohandri Putra
 *  Sponsor : https://github.com/sponsors/arisohandriputra/
 *  License : MIT
 *
 *  Auxiliary tools added in v1.3:
 *    - JSON report export
 *    - Quick benchmark (5-sec sequential read, with progress bar)
 *    - Settings dialog (alert thresholds + refresh interval)
 *    - System info dialog (OS / CPU / RAM / drive count)
 *
 *  All dialogs use raw Win32 CreateWindowExA, no MFC, no resource templates.
 * ============================================================================
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tools.h"
#include "mainwnd.h"   /* for g_Drives, g_nDriveCount, g_nSelectedDrive, g_hInst */
#include "smart.h"
#include "drive_db.h"

/* ------------------------------------------------------------------ */
/*  Global settings instance                                          */
/* ------------------------------------------------------------------ */
APP_SETTINGS g_Settings;

/* ------------------------------------------------------------------ */
/*  Settings - load / save                                            */
/* ------------------------------------------------------------------ */

static const char* GetSettingsPath(char* szBuf, int nBufLen)
{
    char szAppData[MAX_PATH] = "";
    if (!SHGetSpecialFolderPathA(NULL, szAppData, CSIDL_APPDATA, TRUE)) {
        lstrcpynA(szBuf, ".\\HDDH_settings.ini", nBufLen);
        return szBuf;
    }
    _snprintf(szBuf, nBufLen, "%s\\HDDH", szAppData);
    CreateDirectoryA(szBuf, NULL);
    lstrcpynA(szBuf + strlen(szBuf), "\\settings.ini",
              nBufLen - (int)strlen(szBuf));
    return szBuf;
}

void Settings_Load(void)
{
    char szPath[MAX_PATH];
    GetSettingsPath(szPath, sizeof(szPath));

    g_Settings.nTempWarnC         = GetPrivateProfileIntA("Alerts",  "TempWarnC",  55, szPath);
    g_Settings.nTempCritC         = GetPrivateProfileIntA("Alerts",  "TempCritC",  65, szPath);
    g_Settings.nHealthWarnPct     = GetPrivateProfileIntA("Alerts",  "HealthWarnPct", 40, szPath);
    g_Settings.nHealthCritPct     = GetPrivateProfileIntA("Alerts",  "HealthCritPct", 20, szPath);
    g_Settings.nRefreshIntervalMs = GetPrivateProfileIntA("Refresh", "IntervalMs", 5000, szPath);

    /* Sanity bounds */
    if (g_Settings.nTempWarnC < 30 || g_Settings.nTempWarnC > 90) g_Settings.nTempWarnC = 55;
    if (g_Settings.nTempCritC < 40 || g_Settings.nTempCritC > 100) g_Settings.nTempCritC = 65;
    if (g_Settings.nHealthWarnPct < 10 || g_Settings.nHealthWarnPct > 80) g_Settings.nHealthWarnPct = 40;
    if (g_Settings.nHealthCritPct < 5 || g_Settings.nHealthCritPct > 50) g_Settings.nHealthCritPct = 20;
    if (g_Settings.nRefreshIntervalMs < 1000 || g_Settings.nRefreshIntervalMs > 60000)
        g_Settings.nRefreshIntervalMs = 5000;
}

void Settings_Save(void)
{
    char szPath[MAX_PATH];
    GetSettingsPath(szPath, sizeof(szPath));

    char szVal[32];
    _snprintf(szVal, sizeof(szVal), "%d", g_Settings.nTempWarnC);
    WritePrivateProfileStringA("Alerts", "TempWarnC", szVal, szPath);
    _snprintf(szVal, sizeof(szVal), "%d", g_Settings.nTempCritC);
    WritePrivateProfileStringA("Alerts", "TempCritC", szVal, szPath);
    _snprintf(szVal, sizeof(szVal), "%d", g_Settings.nHealthWarnPct);
    WritePrivateProfileStringA("Alerts", "HealthWarnPct", szVal, szPath);
    _snprintf(szVal, sizeof(szVal), "%d", g_Settings.nHealthCritPct);
    WritePrivateProfileStringA("Alerts", "HealthCritPct", szVal, szPath);
    _snprintf(szVal, sizeof(szVal), "%d", g_Settings.nRefreshIntervalMs);
    WritePrivateProfileStringA("Refresh", "IntervalMs", szVal, szPath);
}

/* ================================================================== */
/*  SETTINGS DIALOG                                                    */
/*  Professional layout with proper labels, OK + Cancel buttons,     */
/*  and a working close (X) button via WM_CLOSE.                      */
/* ================================================================== */

#define ID_GRP_ALERTS       5001
#define ID_LBL_TEMP_WARN    5002
#define ID_EDT_TEMP_WARN    5003
#define ID_LBL_TEMP_CRIT    5004
#define ID_EDT_TEMP_CRIT     5005
#define ID_LBL_HEALTH_WARN  5006
#define ID_EDT_HEALTH_WARN  5007
#define ID_LBL_HEALTH_CRIT  5008
#define ID_EDT_HEALTH_CRIT  5009
#define ID_LBL_REFRESH      5010
#define ID_EDT_REFRESH       5011
#define ID_BTN_OK            5012
#define ID_BTN_CANCEL        5013

static LRESULT CALLBACK SettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        {
            /* ---- Title ---- */
            CreateWindowExA(0, "STATIC", "Alert Thresholds & Refresh Settings",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                16, 12, 380, 22, hDlg, NULL, g_hInst, NULL);

            /* ---- Group box: Temperature Alerts ---- */
            CreateWindowExA(0, "BUTTON", "Temperature Alerts",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                16, 44, 380, 80, hDlg, (HMENU)ID_GRP_ALERTS, g_hInst, NULL);

            /* Temperature Warning */
            CreateWindowExA(0, "STATIC", "Warning Threshold (C):",
                WS_CHILD | WS_VISIBLE | SS_RIGHT,
                32, 68, 180, 18, hDlg, (HMENU)ID_LBL_TEMP_WARN, g_hInst, NULL);
            HWND hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER,
                220, 66, 80, 22, hDlg, (HMENU)ID_EDT_TEMP_WARN, g_hInst, NULL);
            char szV[16]; _snprintf(szV, sizeof(szV), "%d", g_Settings.nTempWarnC);
            SetWindowTextA(hEdit, szV);

            /* Temperature Critical */
            CreateWindowExA(0, "STATIC", "Critical Threshold (C):",
                WS_CHILD | WS_VISIBLE | SS_RIGHT,
                32, 96, 180, 18, hDlg, (HMENU)ID_LBL_TEMP_CRIT, g_hInst, NULL);
            hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER,
                220, 94, 80, 22, hDlg, (HMENU)ID_EDT_TEMP_CRIT, g_hInst, NULL);
            _snprintf(szV, sizeof(szV), "%d", g_Settings.nTempCritC);
            SetWindowTextA(hEdit, szV);

            /* ---- Group box: Health Alerts ---- */
            CreateWindowExA(0, "BUTTON", "Health Alerts (%)",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                16, 134, 380, 80, hDlg, (HMENU)ID_GRP_ALERTS, g_hInst, NULL);

            /* Health Warning */
            CreateWindowExA(0, "STATIC", "Warning Threshold (%):",
                WS_CHILD | WS_VISIBLE | SS_RIGHT,
                32, 158, 180, 18, hDlg, (HMENU)ID_LBL_HEALTH_WARN, g_hInst, NULL);
            hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER,
                220, 156, 80, 22, hDlg, (HMENU)ID_EDT_HEALTH_WARN, g_hInst, NULL);
            _snprintf(szV, sizeof(szV), "%d", g_Settings.nHealthWarnPct);
            SetWindowTextA(hEdit, szV);

            /* Health Critical */
            CreateWindowExA(0, "STATIC", "Critical Threshold (%):",
                WS_CHILD | WS_VISIBLE | SS_RIGHT,
                32, 186, 180, 18, hDlg, (HMENU)ID_LBL_HEALTH_CRIT, g_hInst, NULL);
            hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER,
                220, 184, 80, 22, hDlg, (HMENU)ID_EDT_HEALTH_CRIT, g_hInst, NULL);
            _snprintf(szV, sizeof(szV), "%d", g_Settings.nHealthCritPct);
            SetWindowTextA(hEdit, szV);

            /* ---- Refresh interval ---- */
            CreateWindowExA(0, "BUTTON", "Refresh",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                16, 224, 380, 56, hDlg, NULL, g_hInst, NULL);

            CreateWindowExA(0, "STATIC", "Refresh Interval (seconds):",
                WS_CHILD | WS_VISIBLE | SS_RIGHT,
                32, 248, 180, 18, hDlg, (HMENU)ID_LBL_REFRESH, g_hInst, NULL);
            hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER,
                220, 246, 80, 22, hDlg, (HMENU)ID_EDT_REFRESH, g_hInst, NULL);
            _snprintf(szV, sizeof(szV), "%d", g_Settings.nRefreshIntervalMs / 1000);
            SetWindowTextA(hEdit, szV);

            /* ---- OK / Cancel buttons ---- */
            HWND hBtnOK = CreateWindowExA(0, "BUTTON", "OK",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_DEFPUSHBUTTON,
                200, 296, 90, 28, hDlg, (HMENU)ID_BTN_OK, g_hInst, NULL);
            HWND hBtnCancel = CreateWindowExA(0, "BUTTON", "Cancel",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                300, 296, 90, 28, hDlg, (HMENU)ID_BTN_CANCEL, g_hInst, NULL);

            /* Apply default GUI font to all children */
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND hChild = GetWindow(hDlg, GW_CHILD);
            while (hChild) {
                SendMessageA(hChild, WM_SETFONT, (WPARAM)hFont, TRUE);
                hChild = GetWindow(hChild, GW_HWNDNEXT);
            }
        }
        return 0;

    case WM_COMMAND:
        {
            int nCtrl = LOWORD(wParam);
            if (nCtrl == ID_BTN_OK) {
                /* Read values back from the dialog controls */
                char szText[32];
                HWND h;

                h = GetDlgItem(hDlg, ID_EDT_TEMP_WARN);
                GetWindowTextA(h, szText, sizeof(szText));
                int v = atoi(szText);
                if (v >= 30 && v <= 90) g_Settings.nTempWarnC = v;

                h = GetDlgItem(hDlg, ID_EDT_TEMP_CRIT);
                GetWindowTextA(h, szText, sizeof(szText));
                v = atoi(szText);
                if (v >= 40 && v <= 100) g_Settings.nTempCritC = v;

                h = GetDlgItem(hDlg, ID_EDT_HEALTH_WARN);
                GetWindowTextA(h, szText, sizeof(szText));
                v = atoi(szText);
                if (v >= 10 && v <= 80) g_Settings.nHealthWarnPct = v;

                h = GetDlgItem(hDlg, ID_EDT_HEALTH_CRIT);
                GetWindowTextA(h, szText, sizeof(szText));
                v = atoi(szText);
                if (v >= 5 && v <= 50) g_Settings.nHealthCritPct = v;

                h = GetDlgItem(hDlg, ID_EDT_REFRESH);
                GetWindowTextA(h, szText, sizeof(szText));
                int nSec = atoi(szText);
                if (nSec >= 1 && nSec <= 60) g_Settings.nRefreshIntervalMs = nSec * 1000;

                Settings_Save();

                MessageBoxA(hDlg,
                    "Settings saved successfully.\n\n"
                    "Changes to alert thresholds will apply on the next refresh.\n"
                    "Refresh interval changes will apply on the next refresh cycle.",
                    "Settings Saved", MB_OK | MB_ICONINFORMATION);

                DestroyWindow(hDlg);
                return TRUE;
            }
            else if (nCtrl == ID_BTN_CANCEL) {
                DestroyWindow(hDlg);
                return TRUE;
            }
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hDlg);
        return TRUE;
    }
    return DefWindowProcA(hDlg, uMsg, wParam, lParam);
}

BOOL Settings_ShowDialog(HWND hParent)
{
    static BOOL bClassReg = FALSE;
    if (!bClassReg) {
        WNDCLASSEXA wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = SettingsDlgProc;
        wc.hInstance     = g_hInst;
        wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "HDDHSettingsDlg";
        wc.hIconSm       = NULL;
        RegisterClassExA(&wc);
        bClassReg = TRUE;
    }

    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "HDDHSettingsDlg", "Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        0, 0, 420, 370,
        hParent, NULL, g_hInst, NULL);
    if (!hDlg) return FALSE;

    /* Center on parent */
    RECT rcP, rcD;
    GetWindowRect(hParent, &rcP);
    GetWindowRect(hDlg, &rcD);
    int x = rcP.left + ((rcP.right - rcP.left) - (rcD.right - rcD.left)) / 2;
    int y = rcP.top  + ((rcP.bottom - rcP.top) - (rcD.bottom - rcD.top)) / 2;
    SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    EnableWindow(hParent, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        if (!IsWindowEnabled(hParent) && IsDialogMessageA(hDlg, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
        if (!IsWindow(hDlg)) break;
    }

    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);
    return TRUE;
}

/* ================================================================== */
/*  JSON REPORT EXPORT                                                 */
/* ================================================================== */

static void JSON_WriteStr(HANDLE hFile, const char* sz)
{
    DWORD dw;
    WriteFile(hFile, "\"", 1, &dw, NULL);
    for (const char* p = sz; *p; p++) {
        char buf[2];
        if (*p == '"' || *p == '\\') { buf[0] = '\\'; buf[1] = *p; }
        else { buf[0] = *p; buf[1] = '\0'; }
        WriteFile(hFile, buf, (DWORD)(buf[1] ? 2 : 1), &dw, NULL);
    }
    WriteFile(hFile, "\"", 1, &dw, NULL);
}

static void JSON_WriteRaw(HANDLE hFile, const char* sz)
{
    DWORD dw;
    WriteFile(hFile, sz, (DWORD)strlen(sz), &dw, NULL);
}

static void JSON_WriteIndent(HANDLE hFile, int nLevel)
{
    DWORD dw;
    int i;
    for (i = 0; i < nLevel * 2; i++) WriteFile(hFile, " ", 1, &dw, NULL);
}

BOOL Tools_SaveJSONReport(HWND hWnd)
{
    char szDocDir[MAX_PATH] = "";
    if (!SHGetSpecialFolderPathA(NULL, szDocDir, CSIDL_PERSONAL, TRUE)) {
        GetModuleFileNameA(NULL, szDocDir, MAX_PATH);
        char* p = strrchr(szDocDir, '\\');
        if (p) *p = '\0';
    }

    char szOutDir[MAX_PATH];
    _snprintf(szOutDir, sizeof(szOutDir), "%s\\HDDH_Reports", szDocDir);
    CreateDirectoryA(szOutDir, NULL);

    SYSTEMTIME st;
    GetLocalTime(&st);
    char szFile[MAX_PATH];
    _snprintf(szFile, sizeof(szFile),
        "%s\\HDDH_Report_%04d%02d%02d_%02d%02d%02d.json",
        szOutDir, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    HANDLE hFile = CreateFileA(szFile, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxA(hWnd, "Failed to create JSON report file.", "Save JSON Report", MB_ICONERROR);
        return FALSE;
    }

    JSON_WriteRaw(hFile, "{\r\n");
    JSON_WriteIndent(hFile, 1); JSON_WriteStr(hFile, "application");  JSON_WriteRaw(hFile, ": ");
    JSON_WriteStr(hFile, "HDDHealth Monitor"); JSON_WriteRaw(hFile, ",\r\n");

    JSON_WriteIndent(hFile, 1); JSON_WriteStr(hFile, "version");     JSON_WriteRaw(hFile, ": ");
    JSON_WriteStr(hFile, "1.3"); JSON_WriteRaw(hFile, ",\r\n");

    JSON_WriteIndent(hFile, 1); JSON_WriteStr(hFile, "generated");   JSON_WriteRaw(hFile, ": ");
    char szTS[32];
    _snprintf(szTS, sizeof(szTS), "%04d-%02d-%02dT%02d:%02d:%02d",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    JSON_WriteStr(hFile, szTS); JSON_WriteRaw(hFile, ",\r\n");

    JSON_WriteIndent(hFile, 1); JSON_WriteStr(hFile, "author");       JSON_WriteRaw(hFile, ": ");
    JSON_WriteStr(hFile, "Ari Sohandri Putra"); JSON_WriteRaw(hFile, ",\r\n");

    JSON_WriteIndent(hFile, 1); JSON_WriteStr(hFile, "license");      JSON_WriteRaw(hFile, ": ");
    JSON_WriteStr(hFile, "MIT"); JSON_WriteRaw(hFile, ",\r\n");

    JSON_WriteIndent(hFile, 1); JSON_WriteStr(hFile, "drive_count");  JSON_WriteRaw(hFile, ": ");
    char szNum[16];
    _snprintf(szNum, sizeof(szNum), "%d", g_nDriveCount);
    JSON_WriteRaw(hFile, szNum); JSON_WriteRaw(hFile, ",\r\n");

    JSON_WriteIndent(hFile, 1); JSON_WriteStr(hFile, "drives");       JSON_WriteRaw(hFile, ": [\r\n");

    int i;
    for (i = 0; i < g_nDriveCount && i < MAX_DRIVES; i++) {
        DRIVE_INFO* pInfo = &g_Drives[i];

        /* Look up this drive in the embedded JSON database so the report
         * shows the same data as the main window. */
        DRIVE_DB_ENTRY dbEntry;
        ZeroMemory(&dbEntry, sizeof(dbEntry));
        DriveDB_Lookup(pInfo->szModel, pInfo->szFirmware, &dbEntry);

        const char* pszVendor   = (dbEntry.bFound && dbEntry.szTrademark[0])  ? dbEntry.szTrademark  : GetVendorName(pInfo->eVendor);
        const char* pszType     = (dbEntry.bFound && dbEntry.szType[0])        ? dbEntry.szType        : GetDriveTypeName(pInfo->eType);
        const char* pszCtrl     = (dbEntry.bFound && dbEntry.szController[0]) ? dbEntry.szController  : "";
        const char* pszRotRate  = (dbEntry.bFound && dbEntry.szRotationRate[0])? dbEntry.szRotationRate: "";

        char szCapDetected[32];
        FormatSize(pInfo->dwCapacityMB, szCapDetected, sizeof(szCapDetected));
        const char* pszCapacity = (dbEntry.bFound && dbEntry.szCapacity[0]) ? dbEntry.szCapacity : szCapDetected;

        JSON_WriteIndent(hFile, 2); JSON_WriteRaw(hFile, "{\r\n");

        JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "index");       JSON_WriteRaw(hFile, ": ");
        _snprintf(szNum, sizeof(szNum), "%d", i); JSON_WriteRaw(hFile, szNum); JSON_WriteRaw(hFile, ",\r\n");

        JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "model");        JSON_WriteRaw(hFile, ": ");
        JSON_WriteStr(hFile, pInfo->szModel); JSON_WriteRaw(hFile, ",\r\n");

        JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "serial");        JSON_WriteRaw(hFile, ": ");
        JSON_WriteStr(hFile, pInfo->szSerial); JSON_WriteRaw(hFile, ",\r\n");

        JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "firmware");      JSON_WriteRaw(hFile, ": ");
        JSON_WriteStr(hFile, pInfo->szFirmware); JSON_WriteRaw(hFile, ",\r\n");

        JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "capacity");   JSON_WriteRaw(hFile, ": ");
        JSON_WriteStr(hFile, pszCapacity); JSON_WriteRaw(hFile, ",\r\n");

        JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "type");          JSON_WriteRaw(hFile, ": ");
        JSON_WriteStr(hFile, pszType); JSON_WriteRaw(hFile, ",\r\n");

        JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "vendor");        JSON_WriteRaw(hFile, ": ");
        JSON_WriteStr(hFile, pszVendor); JSON_WriteRaw(hFile, ",\r\n");

        JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "health_status"); JSON_WriteRaw(hFile, ": ");
        JSON_WriteStr(hFile, GetHealthStatusName(pInfo->eHealthStatus)); JSON_WriteRaw(hFile, ",\r\n");

        JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "smart_supported"); JSON_WriteRaw(hFile, ": ");
        JSON_WriteRaw(hFile, pInfo->bSMART_Supported ? "true" : "false"); JSON_WriteRaw(hFile, ",\r\n");

        JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "smart_enabled"); JSON_WriteRaw(hFile, ": ");
        JSON_WriteRaw(hFile, pInfo->bSMART_Enabled ? "true" : "false"); JSON_WriteRaw(hFile, ",\r\n");

        JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "access_method"); JSON_WriteRaw(hFile, ": ");
        JSON_WriteStr(hFile, GetAccessMethodName(pInfo->eAccessMethod)); JSON_WriteRaw(hFile, ",\r\n");

        if (pszCtrl[0]) {
            JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "controller"); JSON_WriteRaw(hFile, ": ");
            JSON_WriteStr(hFile, pszCtrl); JSON_WriteRaw(hFile, ",\r\n");
        }

        if (pszRotRate[0]) {
            JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "rotation_rate"); JSON_WriteRaw(hFile, ": ");
            JSON_WriteStr(hFile, pszRotRate); JSON_WriteRaw(hFile, ",\r\n");
        }

        JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "temperature_c"); JSON_WriteRaw(hFile, ": ");
        _snprintf(szNum, sizeof(szNum), "%d", pInfo->nTemperatureC);
        JSON_WriteRaw(hFile, szNum); JSON_WriteRaw(hFile, ",\r\n");

        JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "health_percent"); JSON_WriteRaw(hFile, ": ");
        _snprintf(szNum, sizeof(szNum), "%d", pInfo->nHealthPercent);
        JSON_WriteRaw(hFile, szNum); JSON_WriteRaw(hFile, ",\r\n");

        JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "performance_percent"); JSON_WriteRaw(hFile, ": ");
        _snprintf(szNum, sizeof(szNum), "%d", pInfo->nPerformancePercent);
        JSON_WriteRaw(hFile, szNum); JSON_WriteRaw(hFile, ",\r\n");

        JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "power_on_hours"); JSON_WriteRaw(hFile, ": ");
        _snprintf(szNum, sizeof(szNum), "%lu", (unsigned long)pInfo->dwPowerOnHours);
        JSON_WriteRaw(hFile, szNum); JSON_WriteRaw(hFile, ",\r\n");

        JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "power_cycles");   JSON_WriteRaw(hFile, ": ");
        _snprintf(szNum, sizeof(szNum), "%lu", (unsigned long)pInfo->dwPowerCycleCount);
        JSON_WriteRaw(hFile, szNum); JSON_WriteRaw(hFile, ",\r\n");

        JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "read_speed_mbs"); JSON_WriteRaw(hFile, ": ");
        _snprintf(szNum, sizeof(szNum), "%d", pInfo->nReadSpeedMBs);
        JSON_WriteRaw(hFile, szNum); JSON_WriteRaw(hFile, ",\r\n");

        JSON_WriteIndent(hFile, 3); JSON_WriteStr(hFile, "predict_failure"); JSON_WriteRaw(hFile, ": ");
        JSON_WriteRaw(hFile, pInfo->bPredictFailure ? "true" : "false"); JSON_WriteRaw(hFile, "\r\n");

        JSON_WriteIndent(hFile, 2); JSON_WriteRaw(hFile, i < g_nDriveCount - 1 ? "},\r\n" : "}\r\n");
    }

    JSON_WriteIndent(hFile, 1); JSON_WriteRaw(hFile, "]\r\n");
    JSON_WriteRaw(hFile, "}\r\n");

    CloseHandle(hFile);

    char szMsg[MAX_PATH + 256];
    _snprintf(szMsg, sizeof(szMsg), "JSON report saved successfully!\n\n%s\n\nOpen folder now?", szFile);
    if (MessageBoxA(hWnd, szMsg, "Save JSON Report", MB_YESNO | MB_ICONINFORMATION) == IDYES) {
        char szCmd[MAX_PATH + 32];
        _snprintf(szCmd, sizeof(szCmd), "/select,\"%s\"", szFile);
        ShellExecuteA(NULL, "open", "explorer.exe", szCmd, NULL, SW_SHOWNORMAL);
    }
    return TRUE;
}

/* ================================================================== */
/*  BENCHMARK DIALOG (with live progress bar)                         */
/* ================================================================== */

/* Custom message: posted by benchmark thread to update progress */
#define WM_BENCH_PROGRESS  (WM_USER + 101)
#define WM_BENCH_DONE      (WM_USER + 102)

/* Dialog control IDs */
#define ID_BENCH_LABEL     6001
#define ID_BENCH_PROGRESS  6002  /* progress bar */
#define ID_BENCH_RESULT    6003  /* result text */
#define ID_BENCH_CLOSE     6004  /* close button (hidden during run) */

typedef struct _BENCH_PARAMS {
    HWND hDlg;
    int  nDriveIndex;
} BENCH_PARAMS;

static DWORD WINAPI BenchmarkThread(LPVOID lp)
{
    BENCH_PARAMS* p = (BENCH_PARAMS*)lp;
    HWND hDlg = p->hDlg;
    int nDriveIndex = p->nDriveIndex;
    free(p);

    if (nDriveIndex < 0 || nDriveIndex >= g_nDriveCount) {
        PostMessageA(hDlg, WM_BENCH_DONE, 0, 0);
        return 0;
    }

    char szPath[32];
    _snprintf(szPath, sizeof(szPath), "\\\\.\\PhysicalDrive%d",
              g_Drives[nDriveIndex].nDriveIndex);

    HANDLE hDrive = CreateFileA(szPath, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
        OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hDrive == INVALID_HANDLE_VALUE) {
        PostMessageA(hDlg, WM_BENCH_DONE, 0, 0);
        return 0;
    }

    #define BENCH_BLOCK_SIZE  (1024 * 1024)   /* 1 MB */
    BYTE* pBuf = (BYTE*)VirtualAlloc(NULL, BENCH_BLOCK_SIZE, MEM_COMMIT, PAGE_READWRITE);
    if (!pBuf) { CloseHandle(hDrive); PostMessageA(hDlg, WM_BENCH_DONE, 0, 0); return 0; }

    LARGE_INTEGER freq, tStart, tNow;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&tStart);

    DWORD dwTotalBytes = 0;
    int nLastPercent = -1;

    while (1) {
        DWORD dwRead = 0;
        if (!ReadFile(hDrive, pBuf, BENCH_BLOCK_SIZE, &dwRead, NULL) || dwRead == 0) break;
        dwTotalBytes += dwRead;
        QueryPerformanceCounter(&tNow);
        double elapsed = (double)(tNow.QuadPart - tStart.QuadPart) / (double)freq.QuadPart;

        /* Update progress bar (0-100% over 5 seconds) */
        int nPercent = (int)(elapsed / 5.0 * 100.0);
        if (nPercent > 100) nPercent = 100;
        if (nPercent != nLastPercent) {
            PostMessageA(hDlg, WM_BENCH_PROGRESS, (WPARAM)nPercent, 0);
            nLastPercent = nPercent;
        }

        if (elapsed >= 5.0) break;
    }

    QueryPerformanceCounter(&tNow);
    double dTotalSec = (double)(tNow.QuadPart - tStart.QuadPart) / (double)freq.QuadPart;
    double dMBs = (dTotalSec > 0.001) ?
        ((double)dwTotalBytes / (1024.0 * 1024.0)) / dTotalSec : 0.0;

    VirtualFree(pBuf, 0, MEM_RELEASE);
    CloseHandle(hDrive);

    /* Send result back: WPARAM = whole-number MB/s, LPARAM = tenths */
    int nMBs = (int)dMBs;
    int nTenths = (int)((dMBs - nMBs) * 10.0);
    PostMessageA(hDlg, WM_BENCH_DONE, (WPARAM)nMBs, (LPARAM)nTenths);
    return 0;
}

static LRESULT CALLBACK BenchmarkDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        {
            CreateWindowExA(0, "STATIC", "Quick Sequential Read Benchmark",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                16, 14, 380, 22, hDlg, (HMENU)ID_BENCH_LABEL, g_hInst, NULL);

            if (g_nSelectedDrive >= 0 && g_nSelectedDrive < g_nDriveCount) {
                char szDrive[80];
                _snprintf(szDrive, sizeof(szDrive), "Drive: %s",
                    g_Drives[g_nSelectedDrive].szModel);
                CreateWindowExA(0, "STATIC", szDrive,
                    WS_CHILD | WS_VISIBLE | SS_CENTER,
                    16, 40, 380, 18, hDlg, NULL, g_hInst, NULL);
            }

            /* Status text: "Benchmarking... Please wait" */
            HWND hStatus = CreateWindowExA(0, "STATIC", "Benchmarking... Please wait (5 seconds).",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                16, 70, 380, 20, hDlg, (HMENU)ID_BENCH_RESULT, g_hInst, NULL);

            /* Progress bar (0-100%) */
            HWND hProg = CreateWindowExA(0, PROGRESS_CLASSA, NULL,
                WS_CHILD | WS_VISIBLE,
                36, 100, 340, 20, hDlg, (HMENU)ID_BENCH_PROGRESS, g_hInst, NULL);
            SendMessageA(hProg, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            SendMessageA(hProg, PBM_SETSTEP, 1, 0);
            SendMessageA(hProg, PBM_SETPOS, 0, 0);

            /* Close button - hidden during benchmark, shown when done */
            HWND hBtn = CreateWindowExA(0, "BUTTON", "Close",
                WS_CHILD | BS_PUSHBUTTON,
                160, 140, 100, 24, hDlg, (HMENU)ID_BENCH_CLOSE, g_hInst, NULL);
            /* Hidden initially */
            ShowWindow(hBtn, SW_HIDE);

            /* Apply default GUI font to all children */
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND hChild = GetWindow(hDlg, GW_CHILD);
            while (hChild) {
                SendMessageA(hChild, WM_SETFONT, (WPARAM)hFont, TRUE);
                hChild = GetWindow(hChild, GW_HWNDNEXT);
            }

            /* Start benchmark thread */
            BENCH_PARAMS* p = (BENCH_PARAMS*)malloc(sizeof(BENCH_PARAMS));
            if (p) {
                p->hDlg = hDlg;
                p->nDriveIndex = g_nSelectedDrive;
                CreateThread(NULL, 0, BenchmarkThread, p, 0, NULL);
            }
        }
        return 0;

    case WM_BENCH_PROGRESS:
        {
            /* Update progress bar */
            HWND hProg = GetDlgItem(hDlg, ID_BENCH_PROGRESS);
            if (hProg) SendMessageA(hProg, PBM_SETPOS, (int)wParam, 0);
        }
        return TRUE;

    case WM_BENCH_DONE:
        {
            int nMBs = (int)wParam;
            int nTenths = (int)lParam;
            char szResult[128];
            if (nMBs > 0)
                _snprintf(szResult, sizeof(szResult),
                    "Sequential Read Speed: %d.%d MB/s", nMBs, nTenths);
            else
                lstrcpynA(szResult,
                    "Benchmark failed.\nRun as Administrator and select a valid drive.",
                    sizeof(szResult));

            SetDlgItemTextA(hDlg, ID_BENCH_RESULT, szResult);

            /* Show close button now */
            HWND hBtn = GetDlgItem(hDlg, ID_BENCH_CLOSE);
            if (hBtn) ShowWindow(hBtn, SW_SHOW);

            /* Set progress to 100% */
            HWND hProg = GetDlgItem(hDlg, ID_BENCH_PROGRESS);
            if (hProg) SendMessageA(hProg, PBM_SETPOS, 100, 0);
        }
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BENCH_CLOSE) {
            DestroyWindow(hDlg);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hDlg);
        return TRUE;
    }
    return DefWindowProcA(hDlg, uMsg, wParam, lParam);
}

void Tools_ShowBenchmarkDialog(HWND hWnd)
{
    static BOOL bReg = FALSE;
    if (!bReg) {
        WNDCLASSEXA wc; ZeroMemory(&wc, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = BenchmarkDlgProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "HDDHBenchDlg";
        RegisterClassExA(&wc);
        bReg = TRUE;
    }

    HWND hDlg = CreateWindowExA(WS_EX_DLGMODALFRAME, "HDDHBenchDlg",
        "Quick Benchmark",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        0, 0, 420, 210, hWnd, NULL, g_hInst, NULL);
    if (!hDlg) return;

    RECT rcP, rcD;
    GetWindowRect(hWnd, &rcP);
    GetWindowRect(hDlg, &rcD);
    int x = rcP.left + ((rcP.right - rcP.left) - (rcD.right - rcD.left)) / 2;
    int y = rcP.top  + ((rcP.bottom - rcP.top) - (rcD.bottom - rcD.top)) / 2;
    SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    EnableWindow(hWnd, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        if (!IsWindowEnabled(hWnd) && IsDialogMessageA(hDlg, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
        if (!IsWindow(hDlg)) break;
    }
    EnableWindow(hWnd, TRUE);
    SetForegroundWindow(hWnd);
}

/* ================================================================== */
/*  SYSTEM INFORMATION DIALOG                                         */
/* ================================================================== */

static void GetOSString(char* szBuf, int nLen)
{
    OSVERSIONINFOEXA osvi;
    ZeroMemory(&osvi, sizeof(osvi));
    osvi.dwOSVersionInfoSize = sizeof(osvi);

    #pragma warning(suppress: 4996)
    if (!GetVersionExA((OSVERSIONINFOA*)&osvi)) {
        lstrcpynA(szBuf, "Unknown", nLen);
        return;
    }

    const char* pszName = "Unknown Windows";
    if (osvi.dwMajorVersion == 10) {
        if (osvi.dwBuildNumber >= 22000) pszName = "Windows 11";
        else pszName = "Windows 10";
    }
    else if (osvi.dwMajorVersion == 6) {
        if (osvi.dwMinorVersion == 3) pszName = "Windows 8.1";
        else if (osvi.dwMinorVersion == 2) pszName = "Windows 8";
        else if (osvi.dwMinorVersion == 1) pszName = "Windows 7";
        else if (osvi.dwMinorVersion == 0) pszName = "Windows Vista";
    }
    else if (osvi.dwMajorVersion == 5) {
        if (osvi.dwMinorVersion == 1) pszName = "Windows XP";
        else if (osvi.dwMinorVersion == 2) pszName = "Windows Server 2003";
    }

    _snprintf(szBuf, nLen, "%s (Build %lu)", pszName, (unsigned long)osvi.dwBuildNumber);
}

static void GetCPUString(char* szBuf, int nLen)
{
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        lstrcpynA(szBuf, "Unknown", nLen);
        return;
    }

    char szCpu[256] = "";
    DWORD dwSize = sizeof(szCpu);
    DWORD dwType = 0;
    RegQueryValueExA(hKey, "ProcessorNameString", NULL, &dwType,
        (LPBYTE)szCpu, &dwSize);
    RegCloseKey(hKey);

    if (szCpu[0]) lstrcpynA(szBuf, szCpu, nLen);
    else lstrcpynA(szBuf, "Unknown", nLen);
}

static void GetRAMString(char* szBuf, int nLen)
{
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);

    DWORDLONG totalMB = ms.ullTotalPhys / (1024 * 1024);
    DWORDLONG availMB = ms.ullAvailPhys / (1024 * 1024);

    _snprintf(szBuf, nLen, "%llu MB total (%llu MB available)",
        (unsigned __int64)totalMB, (unsigned __int64)availMB);
}

static LRESULT CALLBACK SysInfoDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        {
            char szBuf[256];

            CreateWindowExA(0, "STATIC", "System Information",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                16, 14, 400, 22, hDlg, NULL, g_hInst, NULL);

            /* Group box */
            CreateWindowExA(0, "BUTTON", "System Details",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                16, 44, 400, 200, hDlg, NULL, g_hInst, NULL);

            int y = 68;
            CreateWindowExA(0, "STATIC", "Operating System:",
                WS_CHILD | WS_VISIBLE | SS_RIGHT,
                32, y, 150, 18, hDlg, NULL, g_hInst, NULL);
            GetOSString(szBuf, sizeof(szBuf));
            CreateWindowExA(0, "STATIC", szBuf,
                WS_CHILD | WS_VISIBLE, 190, y, 210, 18, hDlg, NULL, g_hInst, NULL);
            y += 28;

            CreateWindowExA(0, "STATIC", "Processor:",
                WS_CHILD | WS_VISIBLE | SS_RIGHT,
                32, y, 150, 18, hDlg, NULL, g_hInst, NULL);
            GetCPUString(szBuf, sizeof(szBuf));
            CreateWindowExA(0, "STATIC", szBuf,
                WS_CHILD | WS_VISIBLE, 190, y, 210, 32, hDlg, NULL, g_hInst, NULL);
            y += 40;

            CreateWindowExA(0, "STATIC", "Total Memory (RAM):",
                WS_CHILD | WS_VISIBLE | SS_RIGHT,
                32, y, 150, 18, hDlg, NULL, g_hInst, NULL);
            GetRAMString(szBuf, sizeof(szBuf));
            CreateWindowExA(0, "STATIC", szBuf,
                WS_CHILD | WS_VISIBLE, 190, y, 210, 18, hDlg, NULL, g_hInst, NULL);
            y += 28;

            CreateWindowExA(0, "STATIC", "Drives Detected:",
                WS_CHILD | WS_VISIBLE | SS_RIGHT,
                32, y, 150, 18, hDlg, NULL, g_hInst, NULL);
            _snprintf(szBuf, sizeof(szBuf), "%d drive(s)", g_nDriveCount);
            CreateWindowExA(0, "STATIC", szBuf,
                WS_CHILD | WS_VISIBLE, 190, y, 210, 18, hDlg, NULL, g_hInst, NULL);
            y += 36;

            CreateWindowExA(0, "STATIC",
                "HDDHealth Monitor 1.3 - 100% Free Open Source Software\n"
                "Author: Ari Sohandri Putra\n"
                "Sponsor: https://github.com/sponsors/arisohandriputra/\n"
                "License: MIT",
                WS_CHILD | WS_VISIBLE,
                32, y, 380, 64, hDlg, NULL, g_hInst, NULL);

            HWND hBtn = CreateWindowExA(0, "BUTTON", "Close",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_DEFPUSHBUTTON,
                170, y + 80, 100, 28, hDlg, (HMENU)9, g_hInst, NULL);

            /* Apply default GUI font to all children */
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND hChild = GetWindow(hDlg, GW_CHILD);
            while (hChild) {
                SendMessageA(hChild, WM_SETFONT, (WPARAM)hFont, TRUE);
                hChild = GetWindow(hChild, GW_HWNDNEXT);
            }
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == 9) { DestroyWindow(hDlg); return TRUE; }
        break;

    case WM_CLOSE:
        DestroyWindow(hDlg);
        return TRUE;
    }
    return DefWindowProcA(hDlg, uMsg, wParam, lParam);
}

void Tools_ShowSystemInfoDialog(HWND hWnd)
{
    static BOOL bReg = FALSE;
    if (!bReg) {
        WNDCLASSEXA wc; ZeroMemory(&wc, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = SysInfoDlgProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "HDDHSysInfoDlg";
        RegisterClassExA(&wc);
        bReg = TRUE;
    }

    HWND hDlg = CreateWindowExA(WS_EX_DLGMODALFRAME, "HDDHSysInfoDlg",
        "System Information",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        0, 0, 440, 360, hWnd, NULL, g_hInst, NULL);
    if (!hDlg) return;

    RECT rcP, rcD;
    GetWindowRect(hWnd, &rcP);
    GetWindowRect(hDlg, &rcD);
    int x = rcP.left + ((rcP.right - rcP.left) - (rcD.right - rcD.left)) / 2;
    int y = rcP.top  + ((rcP.bottom - rcP.top) - (rcD.bottom - rcD.top)) / 2;
    SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    EnableWindow(hWnd, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        if (!IsWindowEnabled(hWnd) && IsDialogMessageA(hDlg, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
        if (!IsWindow(hDlg)) break;
    }
    EnableWindow(hWnd, TRUE);
    SetForegroundWindow(hWnd);
}
