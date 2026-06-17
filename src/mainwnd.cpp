/* ============================================================================
 *  HDDHealth Monitor - Main window implementation
 *  ---------------------------------------------------------------------------
 *  100% Free and Open Source Software (FOSS).
 *
 *  Author  : Ari Sohandri Putra
 *  Company : ARImetic Inc.
 *  Sponsor : https://github.com/sponsors/arisohandriputra/
 *  License : MIT
 *
 *  This translation unit implements the entire main-window experience:
 *    - Drive-selection buttons (custom owner-drawn buttons)
 *    - Health / Performance custom progress bars
 *    - S.M.A.R.T. attribute list view
 *    - Tray icon management with per-drive sub-icons
 *    - Device arrival / removal (hot-plug) handling
 *    - Temperature / health / failure critical alerts
 *    - About dialog
 *    - Save-screenshot feature (PNG via GDI+)
 *
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <dbt.h>
/* MSVC-specific linker pragma.  MinGW / GCC ignores this with a warning,
   so we wrap it in _MSC_VER; the Makefile already passes -lmsimg32 in
   LDFLAGS for MinGW builds. */
#ifdef _MSC_VER
#pragma comment(lib, "Msimg32.lib")
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define GDIPVER 0x0110
#include <objbase.h>
#include <shlobj.h>
#include <gdiplus.h>
using namespace Gdiplus;

#include "mainwnd.h"
#include "smart.h"
#include "smart_history.h"
#include "donate.h"    

static unsigned __int64 NVMeRead128Lo(const BYTE* p)
{
    unsigned __int64 lo = 0;
    int i;
    for (i = 7; i >= 0; i--) lo = (lo << 8) | p[i];
    return lo;
}

static WORD ReadLE16(const BYTE* p)
{
    return (WORD)p[0] | ((WORD)p[1] << 8);
}

DRIVE_INFO  g_Drives[MAX_DRIVES];
int         g_nDriveCount    = 0;
int         g_nSelectedDrive = 0;
HINSTANCE   g_hInst          = NULL;
HWND        g_hMainWnd       = NULL;
HWND        g_hHealthBar     = NULL;
HWND        g_hPerfBar       = NULL;
HWND        g_hDriveBtn[MAX_DRIVES];

static NOTIFYICONDATAA g_nid[MAX_DRIVES];
static BOOL            g_bTraySlot[MAX_DRIVES];
static int             g_nTrayCount    = 0;
static BOOL            g_bMinToTray    = FALSE;

static HDEVNOTIFY      g_hDevNotify    = NULL;
static DRIVE_INFO      g_PrevDrives[MAX_DRIVES];
static int             g_nPrevCount    = 0;
#define HOTPLUG_DELAY_MS  1200

#define ALERT_TEMP_WARN_C       55
#define ALERT_TEMP_CRITICAL_C   65
#define ALERT_HEALTH_WARN       40
#define ALERT_HEALTH_CRITICAL   20

typedef struct {
    BOOL bTempWarnSent;
    BOOL bTempCritSent;
    BOOL bHealthWarnSent;
    BOOL bHealthCritSent;
    BOOL bFailurePredSent;
    BOOL bNVMeCritWarnSent;
    BOOL bReallocSent;
    BOOL bUncorrectSent;
} DRIVE_ALERT_STATE;

static DRIVE_ALERT_STATE g_AlertState[MAX_DRIVES];
static BOOL g_bAlertStateInit = FALSE;

/* Note: the previous WinRAR-style nag timer state variables
   (g_nagSecondsLeft, g_bNagPending) have been removed because the
   program is 100% free and open source
   reminder to show anymore. */

static void UpdateWindowTitle(HWND hWnd)
{
    SetWindowTextA(hWnd, "HDDHealth Monitor 1.1");
}

HBRUSH  g_hbrBG     = NULL;
HBRUSH  g_hbrPanel  = NULL;
HBRUSH  g_hbrGreen  = NULL;
HBRUSH  g_hbrYellow = NULL;
HBRUSH  g_hbrRed    = NULL;
HFONT   g_hFontTitle  = NULL;
HFONT   g_hFontNormal = NULL;
HFONT   g_hFontSmall  = NULL;
HFONT   g_hFontBig    = NULL;

static WNDPROC g_wpOrigBtn = NULL;

void CreateGDIObjects(void)
{
    g_hbrBG     = CreateSolidBrush(CLR_BG);
    g_hbrPanel  = CreateSolidBrush(CLR_PANEL);
    g_hbrGreen  = CreateSolidBrush(CLR_GREEN);
    g_hbrYellow = CreateSolidBrush(CLR_YELLOW);
    g_hbrRed    = CreateSolidBrush(CLR_RED);

    g_hFontTitle  = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_hFontNormal = CreateFontA(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_hFontSmall  = CreateFontA(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_hFontBig    = CreateFontA(-32, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE, ANSI_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
}

static HICON BuildHealthIcon(int nHealth)
{
    const int SZ = 16;
    COLORREF clrBg;
    if      (nHealth < 0)   clrBg = RGB(30, 100, 210);
    else if (nHealth >= 70) clrBg = RGB(30, 150,  60);
    else if (nHealth >= 40) clrBg = RGB(180,120,   0);
    else                    clrBg = RGB(190,  30,  30);

    HDC hdcScreen = GetDC(NULL);
    HDC hdcColor  = CreateCompatibleDC(hdcScreen);
    HDC hdcMask   = CreateCompatibleDC(hdcScreen);

    HBITMAP hbmColor = CreateCompatibleBitmap(hdcScreen, SZ, SZ);
    HBITMAP hbmMask  = CreateBitmap(SZ, SZ, 1, 1, NULL);

    HBITMAP hbmOldColor = (HBITMAP)SelectObject(hdcColor, hbmColor);
    HBITMAP hbmOldMask  = (HBITMAP)SelectObject(hdcMask,  hbmMask);

    HBRUSH hbrBg = CreateSolidBrush(clrBg);
    RECT rc = { 0, 0, SZ, SZ };
    FillRect(hdcColor, &rc, hbrBg);
    DeleteObject(hbrBg);

    char szText[8];
    if (nHealth < 0)
        lstrcpyA(szText, "--");
    else
        _snprintf(szText, sizeof(szText), "%d", nHealth);

    HFONT hFont = CreateFontA(-9, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                               ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, "Arial");
    HFONT hOldFont = (HFONT)SelectObject(hdcColor, hFont);
    SetBkMode(hdcColor, TRANSPARENT);
    SetTextColor(hdcColor, RGB(255, 255, 255));

    int len = (int)strlen(szText);
    SIZE sz;
    GetTextExtentPoint32A(hdcColor, szText, len, &sz);
    int x = (SZ - sz.cx) / 2;
    int y = (SZ - sz.cy) / 2;
    TextOutA(hdcColor, x, y, szText, len);

    SelectObject(hdcColor, hOldFont);
    DeleteObject(hFont);

    PatBlt(hdcMask, 0, 0, SZ, SZ, BLACKNESS);

    SelectObject(hdcColor, hbmOldColor);
    SelectObject(hdcMask,  hbmOldMask);

    DeleteDC(hdcColor);
    DeleteDC(hdcMask);
    ReleaseDC(NULL, hdcScreen);

    ICONINFO ii;
    ii.fIcon    = TRUE;
    ii.xHotspot = 0;
    ii.yHotspot = 0;
    ii.hbmColor = hbmColor;
    ii.hbmMask  = hbmMask;
    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hbmColor);
    DeleteObject(hbmMask);
    return hIcon;
}

static void TrayBalloon(const char* szTitle, const char* szMsg, DWORD niif)
{
    if (!g_bTraySlot[0]) return;

    NOTIFYICONDATAA nid;
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize      = sizeof(NOTIFYICONDATAA);
    nid.hWnd        = g_nid[0].hWnd;
    nid.uID         = g_nid[0].uID;
    nid.uFlags      = NIF_INFO;
    nid.dwInfoFlags = niif;
    nid.uTimeout    = 4000;
    lstrcpynA(nid.szInfoTitle, szTitle, sizeof(nid.szInfoTitle) - 1);
    lstrcpynA(nid.szInfo,      szMsg,   sizeof(nid.szInfo)      - 1);
    Shell_NotifyIconA(NIM_MODIFY, &nid);
}

static ULONG_PTR g_gdiplusToken = 0;

static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT nEncoders = 0, nSize = 0;
    GetImageEncodersSize(&nEncoders, &nSize);
    if (nSize == 0) return -1;

    ImageCodecInfo* pInfo = (ImageCodecInfo*)malloc(nSize);
    if (!pInfo) return -1;
    GetImageEncoders(nEncoders, nSize, pInfo);

    for (UINT i = 0; i < nEncoders; i++) {
        if (wcscmp(pInfo[i].MimeType, format) == 0) {
            *pClsid = pInfo[i].Clsid;
            free(pInfo);
            return (int)i;
        }
    }
    free(pInfo);
    return -1;
}

static BOOL SaveScreenshotPNG(HWND hWnd, char* szPathOut, int nPathMax,
                               char* szErrOut, int nErrMax)
{

    char szDocDir[MAX_PATH] = "";
    if (!SHGetSpecialFolderPathA(NULL, szDocDir, CSIDL_PERSONAL, TRUE)) {

        GetModuleFileNameA(NULL, szDocDir, MAX_PATH);
        char* p = strrchr(szDocDir, '\\');
        if (p) *p = '\0';
    }
    char szOutDir[MAX_PATH];
    _snprintf(szOutDir, sizeof(szOutDir), "%s\\HDDH_Screenshots", szDocDir);
    CreateDirectoryA(szOutDir, NULL);

    SYSTEMTIME st;
    GetLocalTime(&st);
    char szFile[MAX_PATH];
    _snprintf(szFile, sizeof(szFile),
        "%s\\HDDH_%04d%02d%02d_%02d%02d%02d.png",
        szOutDir,
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);
    if (szPathOut) lstrcpynA(szPathOut, szFile, nPathMax);

    RECT rc;
    GetClientRect(hWnd, &rc);
    int w = rc.right  - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
        if (szErrOut) lstrcpynA(szErrOut, "Window has zero size.", nErrMax);
        return FALSE;
    }

    HDC hdcWin = GetDC(hWnd);
    HDC hdcMem = CreateCompatibleDC(hdcWin);
    HBITMAP hbm = CreateCompatibleBitmap(hdcWin, w, h);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbm);

    if (!PrintWindow(hWnd, hdcMem, PW_CLIENTONLY)) {

        POINT pt = {rc.left, rc.top};
        ClientToScreen(hWnd, &pt);
        HDC hdcScreen = GetDC(NULL);
        BitBlt(hdcMem, 0, 0, w, h, hdcScreen, pt.x, pt.y, SRCCOPY);
        ReleaseDC(NULL, hdcScreen);
    }

    SelectObject(hdcMem, hbmOld);
    DeleteDC(hdcMem);
    ReleaseDC(hWnd, hdcWin);

    BOOL bOK = FALSE;
    Bitmap* pBmp = Bitmap::FromHBITMAP(hbm, NULL);
    if (pBmp && pBmp->GetLastStatus() == Ok) {
        CLSID clsidPng;
        if (GetEncoderClsid(L"image/png", &clsidPng) >= 0) {

            WCHAR wszFile[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, szFile, -1, wszFile, MAX_PATH);
            Status st2 = pBmp->Save(wszFile, &clsidPng, NULL);
            bOK = (st2 == Ok);
            if (!bOK && szErrOut)
                _snprintf(szErrOut, nErrMax,
                    "GDI+ Save failed (status %d).", (int)st2);
        } else {
            if (szErrOut) lstrcpynA(szErrOut, "PNG encoder not found.", nErrMax);
        }
    } else {
        if (szErrOut) lstrcpynA(szErrOut, "GDI+ Bitmap creation failed.", nErrMax);
    }
    delete pBmp;
    DeleteObject(hbm);
    return bOK;
}

static void DoSaveScreenshot(HWND hWnd)
{
    char szPath[MAX_PATH] = "";
    char szErr[256]       = "";
    if (SaveScreenshotPNG(hWnd, szPath, MAX_PATH, szErr, sizeof(szErr))) {
        char szMsg[MAX_PATH + 128];
        _snprintf(szMsg, sizeof(szMsg),
            "Screenshot saved successfully!\n\n%s\n\nOpen folder now?", szPath);
        int nRet = MessageBoxA(hWnd, szMsg, "HDDHealth Monitor - Screenshot Saved",
                               MB_YESNO | MB_ICONINFORMATION);
        if (nRet == IDYES) {

            char szCmd[MAX_PATH + 32];
            _snprintf(szCmd, sizeof(szCmd), "/select,\"%s\"", szPath);
            ShellExecuteA(NULL, "open", "explorer.exe", szCmd, NULL, SW_SHOWNORMAL);
        }
    } else {
        char szMsg[320];
        _snprintf(szMsg, sizeof(szMsg), "Screenshot failed:\n%s", szErr);
        MessageBoxA(hWnd, szMsg, "HDDHealth Monitor - Screenshot Error", MB_OK | MB_ICONERROR);
    }
}

static void Snapshot_Save(void)
{
    int i;
    g_nPrevCount = g_nDriveCount;
    for (i = 0; i < g_nDriveCount; i++)
        g_PrevDrives[i] = g_Drives[i];
}

static const char* DriveName(DRIVE_INFO* pD)
{
    return (strlen(pD->szModel) > 0) ? pD->szModel : "Unknown Drive";
}

static void CheckCriticalAlerts(void)
{
    int i;

    if (!g_bAlertStateInit) {
        ZeroMemory(g_AlertState, sizeof(g_AlertState));
        g_bAlertStateInit = TRUE;
    }

    for (i = 0; i < g_nDriveCount && i < MAX_DRIVES; i++) {
        DRIVE_INFO*       pD = &g_Drives[i];
        DRIVE_ALERT_STATE* pA = &g_AlertState[i];
        char szTitle[64], szMsg[256];

        if (i < g_nPrevCount) {
            BOOL bSameSerial = (strlen(pD->szSerial) > 0 &&
                                strcmp(pD->szSerial, g_PrevDrives[i].szSerial) == 0);
            BOOL bSameModel  = (strlen(pD->szModel) > 0 &&
                                strcmp(pD->szModel,  g_PrevDrives[i].szModel)  == 0);
            if (!bSameSerial && !bSameModel)
                ZeroMemory(pA, sizeof(*pA));
        }

        if (!pD->bSMART_Supported) continue;

        if (pD->nTemperatureC > 0) {
            if (pD->nTemperatureC >= ALERT_TEMP_CRITICAL_C && !pA->bTempCritSent) {
                lstrcpyA(szTitle, "!! CRITICAL: Drive Overheating !!");
                _snprintf(szMsg, sizeof(szMsg),
                    "%s\nTemperature: %d\xb0""C (critical threshold: %d\xb0""C)\n"
                    "Power down or improve airflow immediately!",
                    DriveName(pD), pD->nTemperatureC, ALERT_TEMP_CRITICAL_C);
                TrayBalloon(szTitle, szMsg, NIIF_ERROR);
                pA->bTempCritSent = TRUE;
                pA->bTempWarnSent = TRUE;
            } else if (pD->nTemperatureC >= ALERT_TEMP_WARN_C && !pA->bTempWarnSent) {
                lstrcpyA(szTitle, "Warning: Drive Temperature High");
                _snprintf(szMsg, sizeof(szMsg),
                    "%s\nTemperature: %d\xb0""C (warning threshold: %d\xb0""C)\n"
                    "Check system cooling.",
                    DriveName(pD), pD->nTemperatureC, ALERT_TEMP_WARN_C);
                TrayBalloon(szTitle, szMsg, NIIF_WARNING);
                pA->bTempWarnSent = TRUE;
            }

            if (pD->nTemperatureC < ALERT_TEMP_WARN_C - 5) {
                pA->bTempWarnSent = FALSE;
                pA->bTempCritSent = FALSE;
            }
        }

        if (pD->nHealthPercent >= 0) {
            if (pD->nHealthPercent < ALERT_HEALTH_CRITICAL && !pA->bHealthCritSent) {
                lstrcpyA(szTitle, "!! CRITICAL: Drive Health Very Poor !!");
                _snprintf(szMsg, sizeof(szMsg),
                    "%s\nHealth: %d%% — Back up all data immediately!\n"
                    "Drive failure may be imminent.",
                    DriveName(pD), pD->nHealthPercent);
                TrayBalloon(szTitle, szMsg, NIIF_ERROR);
                pA->bHealthCritSent = TRUE;
                pA->bHealthWarnSent = TRUE;
            } else if (pD->nHealthPercent < ALERT_HEALTH_WARN && !pA->bHealthWarnSent) {
                lstrcpyA(szTitle, "Warning: Drive Health Degraded");
                _snprintf(szMsg, sizeof(szMsg),
                    "%s\nHealth: %d%% — Monitor closely and back up data.",
                    DriveName(pD), pD->nHealthPercent);
                TrayBalloon(szTitle, szMsg, NIIF_WARNING);
                pA->bHealthWarnSent = TRUE;
            }
        }

        if (pD->bPredictFailure && !pA->bFailurePredSent) {
            lstrcpyA(szTitle, "!! DRIVE FAILURE PREDICTED !!");
            _snprintf(szMsg, sizeof(szMsg),
                "%s\nThe drive's SMART firmware predicts imminent failure.\n"
                "Back up all data now!",
                DriveName(pD));
            TrayBalloon(szTitle, szMsg, NIIF_ERROR);
            pA->bFailurePredSent = TRUE;
        }

        if (pD->bIsNVMe) {
            BYTE crit = pD->nvmeHealth.CriticalWarning;
            if (crit != 0 && !pA->bNVMeCritWarnSent) {
                lstrcpyA(szTitle, "NVMe Critical Warning");
                _snprintf(szMsg, sizeof(szMsg),
                    "%s\nNVMe critical warning flags:%s%s%s%s%s",
                    DriveName(pD),
                    (crit & NVME_CRIT_WARN_SPARE_BELOW_THRESH)   ? " [Spare Low]"     : "",
                    (crit & NVME_CRIT_WARN_TEMP_THRESHOLD)       ? " [Temp Exceeded]" : "",
                    (crit & NVME_CRIT_WARN_RELIABILITY_DEGRADED) ? " [Reliability!]"  : "",
                    (crit & NVME_CRIT_WARN_READ_ONLY)            ? " [Read-Only!]"    : "",
                    (crit & NVME_CRIT_WARN_VOLATILE_MEM_BACKUP)  ? " [Volatile Mem]"  : "");
                TrayBalloon(szTitle, szMsg, NIIF_ERROR);
                pA->bNVMeCritWarnSent = TRUE;
            }
            if (crit == 0) pA->bNVMeCritWarnSent = FALSE;
        }

        if (!pD->bIsNVMe && !pD->bIsUSB) {
            int k;
            DWORD dwR05 = 0, dwRC6 = 0;
            for (k = 0; k < 30; k++) {
                BYTE  id = pD->attrData.stAttributes[k].bAttrID;
                DWORD dw = GetRawValue(pD->attrData.stAttributes[k].bRawValue);
                if (id == 0x05) dwR05 = dw;
                if (id == 0xC6) dwRC6 = dw;
            }
            if (dwR05 > 0 && !pA->bReallocSent) {
                lstrcpyA(szTitle, "Warning: Reallocated Sectors Detected");
                _snprintf(szMsg, sizeof(szMsg),
                    "%s\nReallocated sectors: %lu\n"
                    "This indicates physical damage on the drive surface.",
                    DriveName(pD), (unsigned long)dwR05);
                TrayBalloon(szTitle, szMsg, NIIF_WARNING);
                pA->bReallocSent = TRUE;
            }
            if (dwRC6 > 0 && !pA->bUncorrectSent) {
                lstrcpyA(szTitle, "!! Uncorrectable Errors Detected !!");
                _snprintf(szMsg, sizeof(szMsg),
                    "%s\nUncorrectable errors: %lu\n"
                    "Data integrity at risk — back up immediately!",
                    DriveName(pD), (unsigned long)dwRC6);
                TrayBalloon(szTitle, szMsg, NIIF_ERROR);
                pA->bUncorrectSent = TRUE;
            }
        }
    }

    for (i = g_nDriveCount; i < MAX_DRIVES; i++)
        ZeroMemory(&g_AlertState[i], sizeof(DRIVE_ALERT_STATE));
}

static void Snapshot_Diff(void)
{

    (void)g_nPrevCount;
}

static void DeviceNotify_Register(HWND hWnd)
{
    static const GUID GUID_DEVINTERFACE_DISK =
    { 0x53F56307, 0xB6BF, 0x11D0,
      { 0x94, 0xF2, 0x00, 0xA0, 0xC9, 0x1E, 0xFB, 0x8B } };

    DEV_BROADCAST_DEVICEINTERFACE_A dbi;
    ZeroMemory(&dbi, sizeof(dbi));
    dbi.dbcc_size       = sizeof(dbi);
    dbi.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    dbi.dbcc_classguid  = GUID_DEVINTERFACE_DISK;

    g_hDevNotify = RegisterDeviceNotificationA(hWnd, &dbi, DEVICE_NOTIFY_WINDOW_HANDLE);
}

static void DeviceNotify_Unregister(void)
{
    if (g_hDevNotify) {
        UnregisterDeviceNotification(g_hDevNotify);
        g_hDevNotify = NULL;
    }
}

void TrayIcon_Add(HWND hWnd)
{
    int i;
    ZeroMemory(g_nid,      sizeof(g_nid));
    ZeroMemory(g_bTraySlot,sizeof(g_bTraySlot));
    g_nTrayCount = 0;

    g_nid[0].cbSize           = sizeof(NOTIFYICONDATAA);
    g_nid[0].hWnd             = hWnd;
    g_nid[0].uID              = IDI_TRAY + 0;
    g_nid[0].uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid[0].uCallbackMessage = WM_TRAYICON;
    g_nid[0].hIcon            = BuildHealthIcon(-1);
    lstrcpyA(g_nid[0].szTip,  "HDDHealth Monitor - scanning...");
    Shell_NotifyIconA(NIM_ADD, &g_nid[0]);
    g_bTraySlot[0] = TRUE;
    g_nTrayCount   = 1;
    (void)i;
}

void TrayIcon_Remove(void)
{
    int i;
    for (i = 0; i < MAX_DRIVES; i++) {
        if (g_bTraySlot[i]) {
            if (g_nid[i].hIcon) { DestroyIcon(g_nid[i].hIcon); g_nid[i].hIcon = NULL; }
            Shell_NotifyIconA(NIM_DELETE, &g_nid[i]);
            g_bTraySlot[i] = FALSE;
        }
    }
    g_nTrayCount = 0;
}

void TrayIcon_Update(void)
{
    int i;
    HWND hWnd = g_hMainWnd;

    for (i = g_nDriveCount; i < MAX_DRIVES; i++) {
        if (g_bTraySlot[i]) {
            if (g_nid[i].hIcon) { DestroyIcon(g_nid[i].hIcon); g_nid[i].hIcon = NULL; }
            Shell_NotifyIconA(NIM_DELETE, &g_nid[i]);
            g_bTraySlot[i] = FALSE;
        }
    }

    for (i = 0; i < g_nDriveCount; i++) {
        int    h       = g_Drives[i].nHealthPercent;
        UINT   uID     = (UINT)(IDI_TRAY + i);
        char   szTip[128];
        HICON  hNewIcon = BuildHealthIcon(h);

        if (h >= 0)
            _snprintf(szTip, sizeof(szTip), "Drive %d: Health %d%%\n%s",
                      g_Drives[i].nDriveIndex, h,
                      (strlen(g_Drives[i].szModel) ? g_Drives[i].szModel : "HDDHealth Monitor"));
        else
            _snprintf(szTip, sizeof(szTip), "Drive %d: Health N/A\n%s",
                      g_Drives[i].nDriveIndex,
                      (strlen(g_Drives[i].szModel) ? g_Drives[i].szModel : "HDDHealth Monitor"));

        if (!g_bTraySlot[i]) {
            ZeroMemory(&g_nid[i], sizeof(NOTIFYICONDATAA));
            g_nid[i].cbSize           = sizeof(NOTIFYICONDATAA);
            g_nid[i].hWnd             = hWnd;
            g_nid[i].uID              = uID;
            g_nid[i].uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            g_nid[i].uCallbackMessage = WM_TRAYICON;
            g_nid[i].hIcon            = hNewIcon;
            lstrcpynA(g_nid[i].szTip, szTip, sizeof(g_nid[i].szTip) - 1);
            Shell_NotifyIconA(NIM_ADD, &g_nid[i]);
            g_bTraySlot[i] = TRUE;
        } else {
            if (g_nid[i].hIcon) DestroyIcon(g_nid[i].hIcon);
            g_nid[i].hIcon  = hNewIcon;
            g_nid[i].uFlags = NIF_ICON | NIF_TIP;
            lstrcpynA(g_nid[i].szTip, szTip, sizeof(g_nid[i].szTip) - 1);
            Shell_NotifyIconA(NIM_MODIFY, &g_nid[i]);
        }
    }

    g_nTrayCount = g_nDriveCount;
}

void TrayIcon_ShowContextMenu(HWND hWnd)
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    AppendMenuA(hMenu, MF_STRING, IDM_SHOW_WINDOW,  "Open HDDHealth Monitor");
    AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hMenu, MF_STRING, IDM_EXIT, "Exit");

    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
    DestroyMenu(hMenu);
}

void DestroyGDIObjects(void){
    if (g_hbrBG)     DeleteObject(g_hbrBG);
    if (g_hbrPanel)  DeleteObject(g_hbrPanel);
    if (g_hbrGreen)  DeleteObject(g_hbrGreen);
    if (g_hbrYellow) DeleteObject(g_hbrYellow);
    if (g_hbrRed)    DeleteObject(g_hbrRed);
    if (g_hFontTitle)  DeleteObject(g_hFontTitle);
    if (g_hFontNormal) DeleteObject(g_hFontNormal);
    if (g_hFontSmall)  DeleteObject(g_hFontSmall);
    if (g_hFontBig)    DeleteObject(g_hFontBig);
}

COLORREF GetHealthColor(int nHealth)
{
    if (nHealth < 0)   return CLR_ACCENT;
    if (nHealth >= 70) return CLR_GREEN;
    if (nHealth >= 40) return CLR_YELLOW;
    return CLR_RED;
}

static void DrawGlassShine(HDC hdc, RECT* prc)
{
    int w = prc->right  - prc->left;
    int h = (prc->bottom - prc->top) / 2;
    if (w <= 0 || h <= 0) return;

    HDC     hdcMem  = CreateCompatibleDC(hdc);
    HBITMAP hbm     = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP hbmOld  = (HBITMAP)SelectObject(hdcMem, hbm);

    int y;
    for (y = 0; y < h; y++) {
        int alpha = 110 - (int)((110 - 18) * y / (h > 1 ? h - 1 : 1));
        int r = (255 * alpha) / 255;
        int g = (255 * alpha) / 255;
        int b = (255 * alpha) / 255;
        HBRUSH hbrLine = CreateSolidBrush(RGB(r, g, b));
        RECT rcLine = { 0, y, w, y + 1 };
        FillRect(hdcMem, &rcLine, hbrLine);
        DeleteObject(hbrLine);
    }

    for (y = 0; y < h; y++) {
        int alpha = 110 - (int)((110 - 18) * y / (h > 1 ? h - 1 : 1));
        BLENDFUNCTION bfRow = { AC_SRC_OVER, 0, (BYTE)alpha, 0 };
        AlphaBlend(hdc, prc->left, prc->top + y, w, 1,
                   hdcMem, 0, y, w, 1, bfRow);
    }

    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
}

static void DrawRoundRect(HDC hdc, RECT* prc, int rx, HBRUSH hbr, HPEN hpen)
{
    HPEN   hOldPen = (HPEN)SelectObject(hdc, hpen);
    HBRUSH hOldBr  = (HBRUSH)SelectObject(hdc, hbr);
    RoundRect(hdc, prc->left, prc->top, prc->right, prc->bottom, rx * 2, rx * 2);
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBr);
}

LRESULT CALLBACK PerfBarWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK HealthBarWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdcReal = BeginPaint(hWnd, &ps);

            RECT rc;
            GetClientRect(hWnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;

            HDC     hdc    = CreateCompatibleDC(hdcReal);
            HBITMAP hbmBuf = CreateCompatibleBitmap(hdcReal, w, h);
            HBITMAP hbmOldBuf = (HBITMAP)SelectObject(hdc, hbmBuf);

            int nHealth = -1;
            if (g_nDriveCount > 0 && g_nSelectedDrive >= 0 && g_nSelectedDrive < g_nDriveCount)
                nHealth = g_Drives[g_nSelectedDrive].nHealthPercent;

            int cxBar = rc.right  - rc.left;
            int cyBar = rc.bottom - rc.top;
            int rx    = 5;

            {
                COLORREF clrTop, clrBot;
                if (nHealth < 0 || nHealth == 100) {
                    clrTop = RGB( 50, 200,  80);
                    clrBot = RGB( 20, 140,  50);
                } else if (nHealth >= 70) {
                    clrTop = RGB( 90, 210,  60);
                    clrBot = RGB( 50, 155,  30);
                } else if (nHealth >= 40) {
                    clrTop = RGB(240, 160,  20);
                    clrBot = RGB(190, 110,   5);
                } else {
                    clrTop = RGB(220,  50,  40);
                    clrBot = RGB(160,  20,  15);
                }
                int y;
                for (y = 0; y < cyBar; y++) {
                    float t  = (cyBar > 1) ? (float)y / (float)(cyBar - 1) : 0.0f;
                    int cr = (int)(GetRValue(clrTop) + t * (int)(GetRValue(clrBot) - GetRValue(clrTop)));
                    int cg = (int)(GetGValue(clrTop) + t * (int)(GetGValue(clrBot) - GetGValue(clrTop)));
                    int cb = (int)(GetBValue(clrTop) + t * (int)(GetBValue(clrBot) - GetBValue(clrTop)));
                    HPEN hpCol = CreatePen(PS_SOLID, 1, RGB(cr, cg, cb));
                    HPEN hpOld = (HPEN)SelectObject(hdc, hpCol);
                    MoveToEx(hdc, 0,     y, NULL);
                    LineTo  (hdc, cxBar, y);
                    SelectObject(hdc, hpOld);
                    DeleteObject(hpCol);
                }
            }

            int nFillPct = (nHealth < 0) ? 100 : nHealth;
            int nFillW   = (cxBar * nFillPct / 100);
            if (nFillW < cxBar) {
                int emptyW = cxBar - nFillW;
                HDC hdcMem2 = CreateCompatibleDC(hdc);
                HBITMAP hbm2 = CreateCompatibleBitmap(hdc, emptyW, cyBar);
                HBITMAP hbmOld2 = (HBITMAP)SelectObject(hdcMem2, hbm2);
                HBRUSH hbrDark = CreateSolidBrush(RGB(20, 20, 20));
                RECT rcFill2 = { 0, 0, emptyW, cyBar };
                FillRect(hdcMem2, &rcFill2, hbrDark);
                DeleteObject(hbrDark);
                BLENDFUNCTION bf2 = { AC_SRC_OVER, 0, 155, 0 };
                AlphaBlend(hdc, nFillW, 0, emptyW, cyBar,
                           hdcMem2, 0, 0, emptyW, cyBar, bf2);
                SelectObject(hdcMem2, hbmOld2);
                DeleteObject(hbm2);
                DeleteDC(hdcMem2);
            }

            { RECT rcShine = { 0, 0, cxBar, cyBar }; DrawGlassShine(hdc, &rcShine); }

            {
                HDC hdcS = CreateCompatibleDC(hdc);
                HBITMAP hbmS = CreateCompatibleBitmap(hdc, cxBar, 3);
                HBITMAP hbmSOld = (HBITMAP)SelectObject(hdcS, hbmS);
                HBRUSH hbrS = CreateSolidBrush(RGB(0,0,0));
                RECT rcS0 = {0,0,cxBar,3}; FillRect(hdcS, &rcS0, hbrS);
                DeleteObject(hbrS);
                BLENDFUNCTION bfS = { AC_SRC_OVER, 0, 45, 0 };
                AlphaBlend(hdc, 0, cyBar - 3, cxBar, 3, hdcS, 0, 0, cxBar, 3, bfS);
                SelectObject(hdcS, hbmSOld);
                DeleteObject(hbmS);
                DeleteDC(hdcS);
            }

            {
                HPEN   hpBorder = CreatePen(PS_SOLID, 1, RGB(120, 120, 120));
                HBRUSH hbrNull2 = (HBRUSH)GetStockObject(NULL_BRUSH);
                HPEN   hpOld    = (HPEN)SelectObject(hdc, hpBorder);
                HBRUSH hbOld    = (HBRUSH)SelectObject(hdc, hbrNull2);
                RoundRect(hdc, 0, 0, cxBar, cyBar, rx*2, rx*2);
                SelectObject(hdc, hpOld);
                SelectObject(hdc, hbOld);
                DeleteObject(hpBorder);
            }

            {
                char szPct[16];
                if (nHealth < 0)
                    _snprintf(szPct, sizeof(szPct), "N/A");
                else
                    _snprintf(szPct, sizeof(szPct), "%d%%", nHealth);

                HFONT hUseFont = g_hFontBig ? g_hFontBig : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
                HFONT hOldFont = (HFONT)SelectObject(hdc, hUseFont);
                SetBkMode(hdc, TRANSPARENT);

                RECT rcBufText = { 0, 0, cxBar, cyBar };
                RECT rcBufSh   = rcBufText; rcBufSh.left++; rcBufSh.top++;
                SetTextColor(hdc, RGB(0, 0, 0));
                DrawTextA(hdc, szPct, -1, &rcBufSh, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                SetTextColor(hdc, RGB(255, 255, 255));
                DrawTextA(hdc, szPct, -1, &rcBufText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, hOldFont);
            }

            BitBlt(hdcReal, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
            SelectObject(hdc, hbmOldBuf);
            DeleteObject(hbmBuf);
            DeleteDC(hdc);

            EndPaint(hWnd, &ps);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;
    }

    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK PerfBarWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdcReal = BeginPaint(hWnd, &ps);

            RECT rc;
            GetClientRect(hWnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;

            HDC     hdc    = CreateCompatibleDC(hdcReal);
            HBITMAP hbmBuf = CreateCompatibleBitmap(hdcReal, w, h);
            HBITMAP hbmOldBuf = (HBITMAP)SelectObject(hdc, hbmBuf);

            int nPerf = -1;
            if (g_nDriveCount > 0 && g_nSelectedDrive >= 0 && g_nSelectedDrive < g_nDriveCount)
                nPerf = g_Drives[g_nSelectedDrive].nPerformancePercent;

            int cxBar = rc.right  - rc.left;
            int cyBar = rc.bottom - rc.top;
            int rx    = 5;

            {
                int y;
                for (y = 0; y < cyBar; y++) {
                    float t  = (cyBar > 1) ? (float)y / (float)(cyBar - 1) : 0.0f;
                    int cr = 204 - (int)(t * 30);
                    int cg = 232 - (int)(t * 40);
                    int cb = 248 - (int)(t * 32);
                    HPEN hpR = CreatePen(PS_SOLID, 1, RGB(cr, cg, cb));
                    HPEN hpO = (HPEN)SelectObject(hdc, hpR);
                    MoveToEx(hdc, 0,      y, NULL);
                    LineTo  (hdc, cxBar,  y);
                    SelectObject(hdc, hpO);
                    DeleteObject(hpR);
                }
            }

            int nFillPct = (nPerf < 0) ? 100 : nPerf;
            int nFillW   = (cxBar * nFillPct / 100);
            if (nFillW < 2) nFillW = 2;
            if (nFillW > 0) {
                COLORREF clrTop, clrBot;
                if (nPerf < 0 || nPerf >= 70) {
                    clrTop = RGB( 20, 160, 220);
                    clrBot = RGB(  5, 110, 170);
                } else if (nPerf >= 40) {
                    clrTop = RGB(220, 160,  10);
                    clrBot = RGB(160, 100,   0);
                } else {
                    clrTop = RGB(220,  70,  50);
                    clrBot = RGB(160,  20,  15);
                }

                int y;
                for (y = 0; y < cyBar; y++) {
                    float t  = (cyBar > 1) ? (float)y / (float)(cyBar - 1) : 0.0f;
                    int cr = (int)(GetRValue(clrTop) + t * (GetRValue(clrBot) - GetRValue(clrTop)));
                    int cg = (int)(GetGValue(clrTop) + t * (GetGValue(clrBot) - GetGValue(clrTop)));
                    int cb = (int)(GetBValue(clrTop) + t * (GetBValue(clrBot) - GetBValue(clrTop)));
                    HPEN hpR = CreatePen(PS_SOLID, 1, RGB(cr, cg, cb));
                    HPEN hpO = (HPEN)SelectObject(hdc, hpR);
                    MoveToEx(hdc, 0,          y, NULL);
                    LineTo  (hdc, nFillW,     y);
                    SelectObject(hdc, hpO);
                    DeleteObject(hpR);
                }

                HPEN hpEdge = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
                HPEN hpEdgeOld = (HPEN)SelectObject(hdc, hpEdge);
                HDC hdcEdge = CreateCompatibleDC(hdc);
                HBITMAP hbmEdge = CreateCompatibleBitmap(hdc, 1, cyBar);
                HBITMAP hbmEdgeOld = (HBITMAP)SelectObject(hdcEdge, hbmEdge);
                HBRUSH hbrW = CreateSolidBrush(RGB(255,255,255));
                RECT rcEdgeFill = {0,0,1,cyBar};
                FillRect(hdcEdge, &rcEdgeFill, hbrW);
                DeleteObject(hbrW);
                BLENDFUNCTION bfEdge = { AC_SRC_OVER, 0, 60, 0 };
                AlphaBlend(hdc, nFillW - 1, 0, 1, cyBar,
                           hdcEdge, 0, 0, 1, cyBar, bfEdge);
                SelectObject(hdcEdge, hbmEdgeOld);
                DeleteObject(hbmEdge);
                DeleteDC(hdcEdge);
                SelectObject(hdc, hpEdgeOld);
                DeleteObject(hpEdge);
            }

            { RECT rcShine = { 0, 0, cxBar, cyBar }; DrawGlassShine(hdc, &rcShine); }

            {
                HDC hdcS = CreateCompatibleDC(hdc);
                HBITMAP hbmS = CreateCompatibleBitmap(hdc, cxBar, 3);
                HBITMAP hbmSOld = (HBITMAP)SelectObject(hdcS, hbmS);
                HBRUSH hbrS = CreateSolidBrush(RGB(0,0,0));
                RECT rcS0 = {0,0,cxBar,3}; FillRect(hdcS, &rcS0, hbrS);
                DeleteObject(hbrS);
                BLENDFUNCTION bfS = { AC_SRC_OVER, 0, 45, 0 };
                AlphaBlend(hdc, 0, cyBar - 3, cxBar, 3,
                           hdcS, 0, 0, cxBar, 3, bfS);
                SelectObject(hdcS, hbmSOld);
                DeleteObject(hbmS);
                DeleteDC(hdcS);
            }

            {
                HPEN   hpBorder = CreatePen(PS_SOLID, 1, RGB(100, 160, 210));
                HBRUSH hbrNull2 = (HBRUSH)GetStockObject(NULL_BRUSH);
                HPEN   hpOld    = (HPEN)SelectObject(hdc, hpBorder);
                HBRUSH hbOld    = (HBRUSH)SelectObject(hdc, hbrNull2);
                RoundRect(hdc, 0, 0, cxBar, cyBar, rx*2, rx*2);
                SelectObject(hdc, hpOld);
                SelectObject(hdc, hbOld);
                DeleteObject(hpBorder);
            }

            {
                char szPct[16];
                if (nPerf < 0)
                    _snprintf(szPct, sizeof(szPct), "N/A");
                else
                    _snprintf(szPct, sizeof(szPct), "%d%%", nPerf);

                HFONT hUseFont = g_hFontBig ? g_hFontBig : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
                HFONT hOldFont = (HFONT)SelectObject(hdc, hUseFont);
                SetBkMode(hdc, TRANSPARENT);

                RECT rcBufText = { 0, 0, cxBar, cyBar };
                RECT rcBufSh   = rcBufText; rcBufSh.left++; rcBufSh.top++;
                SetTextColor(hdc, RGB(0, 0, 0));
                DrawTextA(hdc, szPct, -1, &rcBufSh, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                SetTextColor(hdc, RGB(255, 255, 255));
                DrawTextA(hdc, szPct, -1, &rcBufText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, hOldFont);
            }

            BitBlt(hdcReal, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
            SelectObject(hdc, hbmOldBuf);
            DeleteObject(hbmBuf);
            DeleteDC(hdc);

            EndPaint(hWnd, &ps);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;
    }

    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK DriveBtnWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
        {
            int nIdx = (int)GetWindowLongPtrA(hWnd, GWLP_USERDATA);
            PAINTSTRUCT ps;
            HDC hdcReal = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);

            int w = rc.right  - rc.left;
            int h = rc.bottom - rc.top;

            HDC     hdc    = CreateCompatibleDC(hdcReal);
            HBITMAP hbmBuf = CreateCompatibleBitmap(hdcReal, w, h);
            HBITMAP hbmOldBuf = (HBITMAP)SelectObject(hdc, hbmBuf);

            RECT rcBuf = { 0, 0, w, h };

            BOOL bSelected = (nIdx == g_nSelectedDrive);
            BOOL bHover    = (GetPropA(hWnd, "hover") != NULL);

            COLORREF clrBg, clrBorder, clrText;
            if (bSelected) {
                clrBg     = RGB(30, 100, 210);
                clrBorder = RGB(20,  70, 170);
                clrText   = RGB(255, 255, 255);
            } else if (bHover) {
                clrBg     = RGB(220, 228, 248);
                clrBorder = RGB(140, 165, 220);
                clrText   = RGB(30,  35,  50);
            } else {
                clrBg     = RGB(255, 255, 255);
                clrBorder = RGB(200, 205, 220);
                clrText   = RGB(30,  35,  50);
            }

            int rx = 6;

            {
                HBRUSH hbrWinBg = CreateSolidBrush(CLR_BG);
                FillRect(hdc, &rcBuf, hbrWinBg);
                DeleteObject(hbrWinBg);
            }

            HRGN hClipRgn = CreateRoundRectRgn(rcBuf.left, rcBuf.top, rcBuf.right+1, rcBuf.bottom+1, rx*2, rx*2);
            SelectClipRgn(hdc, hClipRgn);

            {
                int y;
                COLORREF clrTop, clrBot;
                if (bSelected) {
                    clrTop = RGB( 60, 130, 240);
                    clrBot = RGB( 20,  80, 190);
                } else if (bHover) {
                    clrTop = RGB(235, 242, 255);
                    clrBot = RGB(205, 218, 248);
                } else {
                    clrTop = RGB(252, 254, 255);
                    clrBot = RGB(228, 233, 245);
                }
                for (y = 0; y < h; y++) {
                    float t  = (h > 1) ? (float)y / (float)(h - 1) : 0.0f;
                    int cr = (int)(GetRValue(clrTop) + t * (int)(GetRValue(clrBot) - GetRValue(clrTop)));
                    int cg = (int)(GetGValue(clrTop) + t * (int)(GetGValue(clrBot) - GetGValue(clrTop)));
                    int cb = (int)(GetBValue(clrTop) + t * (int)(GetBValue(clrBot) - GetBValue(clrTop)));
                    HPEN hpR = CreatePen(PS_SOLID, 1, RGB(cr, cg, cb));
                    HPEN hpO = (HPEN)SelectObject(hdc, hpR);
                    MoveToEx(hdc, 0,  y, NULL);
                    LineTo  (hdc, w,  y);
                    SelectObject(hdc, hpO);
                    DeleteObject(hpR);
                }
            }

            { RECT rcShineBtn = { 0, 0, w, h }; DrawGlassShine(hdc, &rcShineBtn); }

            {
                HPEN   hpBord = CreatePen(PS_SOLID, 1, clrBorder);
                HPEN   hpOld  = (HPEN)SelectObject(hdc, hpBord);
                HBRUSH hbOld  = (HBRUSH)SelectObject(hdc, (HBRUSH)GetStockObject(NULL_BRUSH));
                RoundRect(hdc, rcBuf.left, rcBuf.top, rcBuf.right, rcBuf.bottom, rx*2, rx*2);
                SelectObject(hdc, hpOld);
                SelectObject(hdc, hbOld);
                DeleteObject(hpBord);
            }

            SelectClipRgn(hdc, NULL);
            DeleteObject(hClipRgn);

            SetBkMode(hdc, TRANSPARENT);

            if (nIdx >= 0 && nIdx < g_nDriveCount) {
                DRIVE_INFO* pD = &g_Drives[nIdx];

                char szName[64];
                if (strlen(pD->szModel) > 0) {
                    _snprintf(szName, sizeof(szName), "%s", pD->szModel);
                    if (strlen(szName) > 26) { szName[24] = '.'; szName[25] = '.'; szName[26] = '\0'; }
                } else {
                    _snprintf(szName, sizeof(szName), "Drive %d", pD->nDriveIndex);
                }

                char szType[16];
                const char* szT = GetDriveTypeName(pD->eType);
                _snprintf(szType, sizeof(szType), "[%s]", szT ? szT : "?");

                char szPerf[24];
                if (pD->nPerformancePercent >= 0)
                    _snprintf(szPerf, sizeof(szPerf), "Perf: %d%%", pD->nPerformancePercent);
                else
                    _snprintf(szPerf, sizeof(szPerf), "Perf: N/A");

                char szHealth[24];
                if (pD->nHealthPercent >= 0)
                    _snprintf(szHealth, sizeof(szHealth), "Health: %d%%", pD->nHealthPercent);
                else
                    _snprintf(szHealth, sizeof(szHealth), "Health: N/A");

                char szCap[24];
                FormatSize(pD->dwCapacityMB, szCap, sizeof(szCap));

                char szTempStr[24];
                if (pD->nTemperatureC > 0)
                    _snprintf(szTempStr, sizeof(szTempStr), "%d\xb0""C", pD->nTemperatureC);
                else
                    szTempStr[0] = '\0';

                int nH = (int)(pD->nHealthPercent);
                COLORREF clrH;
                if      (nH < 0)   clrH = bSelected ? RGB(180,210,255) : CLR_ACCENT;
                else if (nH >= 70) clrH = bSelected ? RGB(180,255,200) : CLR_GREEN;
                else if (nH >= 40) clrH = bSelected ? RGB(255,240,160) : CLR_YELLOW;
                else               clrH = bSelected ? RGB(255,180,180) : CLR_RED;

                COLORREF clrTemp;
                if      (pD->nTemperatureC <= 0)  clrTemp = bSelected ? RGB(180,210,255) : CLR_TEXT_DIM;
                else if (pD->nTemperatureC < 50)  clrTemp = bSelected ? RGB(180,255,200) : CLR_GREEN;
                else if (pD->nTemperatureC < 60)  clrTemp = bSelected ? RGB(255,240,160) : CLR_YELLOW;
                else                              clrTemp = bSelected ? RGB(255,180,180) : CLR_RED;

                HFONT hOldFont;

                hOldFont = (HFONT)SelectObject(hdc, g_hFontNormal);
                SetTextColor(hdc, clrText);
                RECT rcName = { rcBuf.left + 8, rcBuf.top + 5, rcBuf.right - 8, rcBuf.top + 20 };
                DrawTextA(hdc, szName, -1, &rcName, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

                SelectObject(hdc, g_hFontSmall);
                RECT rcType = { rcBuf.left + 8, rcBuf.top + 22, rcBuf.right - 8, rcBuf.top + 35 };
                SetTextColor(hdc, bSelected ? RGB(200, 220, 255) : CLR_TEXT_DIM);
                DrawTextA(hdc, szType, -1, &rcType, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

                RECT rcPerf = { rcBuf.left + 8, rcBuf.top + 35, (rcBuf.left + rcBuf.right) / 2, rcBuf.bottom - 4 - 14 };
                SetTextColor(hdc, clrText);
                DrawTextA(hdc, szPerf, -1, &rcPerf, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

                RECT rcHealth = { (rcBuf.left + rcBuf.right) / 2, rcBuf.top + 35, rcBuf.right - 8, rcBuf.bottom - 4 - 14 };
                SetTextColor(hdc, clrH);
                DrawTextA(hdc, szHealth, -1, &rcHealth, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);

                {
                    RECT rcCap  = { rcBuf.left + 8, rcBuf.bottom - 17, (rcBuf.left + rcBuf.right) / 2, rcBuf.bottom - 4 };
                    RECT rcTmp  = { (rcBuf.left + rcBuf.right) / 2, rcBuf.bottom - 17, rcBuf.right - 8, rcBuf.bottom - 4 };
                    SetTextColor(hdc, clrText);
                    DrawTextA(hdc, szCap, -1, &rcCap, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                    if (szTempStr[0]) {
                        SetTextColor(hdc, clrTemp);
                        DrawTextA(hdc, szTempStr, -1, &rcTmp, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
                    }
                }
                SelectObject(hdc, hOldFont);
            }

            BitBlt(hdcReal, 0, 0, w, h, hdc, 0, 0, SRCCOPY);

            SelectObject(hdc, hbmOldBuf);
            DeleteObject(hbmBuf);
            DeleteDC(hdc);

            EndPaint(hWnd, &ps);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEMOVE:
        if (!GetPropA(hWnd, "hover")) {
            SetPropA(hWnd, "hover", (HANDLE)1);
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hWnd, 0 };
            TrackMouseEvent(&tme);
            InvalidateRect(hWnd, NULL, TRUE);
        }
        return 0;

    case WM_MOUSELEAVE:
        RemovePropA(hWnd, "hover");
        InvalidateRect(hWnd, NULL, TRUE);
        return 0;

    case WM_LBUTTONUP:
        {
            int nIdx = (int)GetWindowLongPtrA(hWnd, GWLP_USERDATA);
            if (nIdx >= 0 && nIdx < g_nDriveCount) {
                HWND hParent = GetParent(hWnd);
                g_nSelectedDrive = nIdx;
                int i;
                for (i = 0; i < g_nDriveCount; i++)
                    if (g_hDriveBtn[i]) InvalidateRect(g_hDriveBtn[i], NULL, TRUE);
                UpdateDriveInfo(hParent, nIdx);
                UpdateAttrList(hParent, nIdx);
                InvalidateRect(hParent, NULL, FALSE);
                UpdateWindow(hParent);
            }
        }
        return 0;

    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, (LPCTSTR)IDC_HAND));
        return TRUE;
    }

    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

void RegisterHealthBarClass(HINSTANCE hInst)
{
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = HealthBarWndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = NULL;
    wc.lpszClassName = "LLHDHealthBar";
    RegisterClassA(&wc);

    ZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = PerfBarWndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = NULL;
    wc.lpszClassName = "LLHDPerfBar";
    RegisterClassA(&wc);

    ZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_DBLCLKS;
    wc.lpfnWndProc   = DriveBtnWndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = NULL;
    wc.lpszClassName = "LLHDDriveBtn";
    RegisterClassA(&wc);
}

void RepaintHealthBar(void)
{
    if (g_hHealthBar) { InvalidateRect(g_hHealthBar, NULL, TRUE); UpdateWindow(g_hHealthBar); }
    if (g_hPerfBar)   { InvalidateRect(g_hPerfBar,   NULL, TRUE); UpdateWindow(g_hPerfBar);   }
}

void UpdateDriveButtons(HWND hWnd)
{
    int i;
    int nBtnW  = DRIVE_BTN_PANEL_W - 12;
    int nBtnH  = DRIVE_BTN_H;
    int nStartY = 40;

    BOOL bNeedRebuild = FALSE;

    if (g_hDriveBtn[0]) {
        char szClass[32] = "";
        GetClassNameA(g_hDriveBtn[0], szClass, sizeof(szClass));
        BOOL bIsStatic   = (strcmp(szClass, "Static") == 0 || strcmp(szClass, "static") == 0);
        BOOL bNeedStatic = (g_nDriveCount == 0);
        if (bIsStatic != bNeedStatic) bNeedRebuild = TRUE;
    }

    {
        int nExisting = 0;
        for (i = 0; i < MAX_DRIVES; i++)
            if (g_hDriveBtn[i]) nExisting++;
        if (nExisting != (g_nDriveCount == 0 ? 1 : g_nDriveCount))
            bNeedRebuild = TRUE;
    }

    if (bNeedRebuild) {
        for (i = 0; i < MAX_DRIVES; i++) {
            if (g_hDriveBtn[i]) {
                DestroyWindow(g_hDriveBtn[i]);
                g_hDriveBtn[i] = NULL;
            }
        }

        if (g_nDriveCount == 0) {
            HWND hPlaceholder = CreateWindowExA(0, "STATIC", "No drives found",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                6, nStartY, nBtnW, nBtnH,
                hWnd, (HMENU)(IDC_DRIVE_BTN_BASE), g_hInst, NULL);
            SendMessage(hPlaceholder, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
            g_hDriveBtn[0] = hPlaceholder;
            return;
        }

        for (i = 0; i < g_nDriveCount && i < MAX_DRIVES; i++) {
            int nY = nStartY + i * (nBtnH + DRIVE_BTN_GAP);
            g_hDriveBtn[i] = CreateWindowExA(
                0, "LLHDDriveBtn", "",
                WS_CHILD | WS_VISIBLE,
                6, nY, nBtnW, nBtnH,
                hWnd, (HMENU)(UINT_PTR)(IDC_DRIVE_BTN_BASE + i), g_hInst, NULL
            );
            SetWindowLongPtrA(g_hDriveBtn[i], GWLP_USERDATA, (LONG_PTR)i);
        }
    } else {

        for (i = 0; i < g_nDriveCount && i < MAX_DRIVES; i++) {
            if (g_hDriveBtn[i])
                InvalidateRect(g_hDriveBtn[i], NULL, FALSE);
        }
    }
}

void UpdateDriveInfo(HWND hWnd, int nDriveIdx)
{
    if (nDriveIdx < 0 || nDriveIdx >= g_nDriveCount) {
        SetDlgItemTextA(hWnd, IDC_MODEL_STATIC,       "-");
        SetDlgItemTextA(hWnd, IDC_SERIAL_STATIC,      "-");
        SetDlgItemTextA(hWnd, IDC_FIRMWARE_STATIC,    "-");
        SetDlgItemTextA(hWnd, IDC_SIZE_STATIC,        "-");
        SetDlgItemTextA(hWnd, IDC_TEMP_STATIC,        "-");
        SetDlgItemTextA(hWnd, IDC_STATUS_STATIC,      "Not Available");
        SetDlgItemTextA(hWnd, IDC_READ_SPEED_STATIC,  "-");

        SetDlgItemTextA(hWnd, IDC_PREDICT_STATIC,     "");
        return;
    }

    DRIVE_INFO* pInfo = &g_Drives[nDriveIdx];
    char szBuf[256];

    SetDlgItemTextA(hWnd, IDC_MODEL_STATIC,
                    strlen(pInfo->szModel) ? pInfo->szModel : "-");

    SetDlgItemTextA(hWnd, IDC_SERIAL_STATIC,
                    strlen(pInfo->szSerial) ? pInfo->szSerial : "-");

    SetDlgItemTextA(hWnd, IDC_FIRMWARE_STATIC,
                    strlen(pInfo->szFirmware) ? pInfo->szFirmware : "-");

    {
        char szSize[32];
        FormatSize(pInfo->dwCapacityMB, szSize, sizeof(szSize));
        _snprintf(szBuf, sizeof(szBuf), "%s   Type: %s",
                  szSize, GetDriveTypeName(pInfo->eType));
        SetDlgItemTextA(hWnd, IDC_SIZE_STATIC, szBuf);
    }

    if (pInfo->nTemperatureC > 0)
        _snprintf(szBuf, sizeof(szBuf), "%d\xb0""C", pInfo->nTemperatureC);
    else
        _snprintf(szBuf, sizeof(szBuf), "-");
    SetDlgItemTextA(hWnd, IDC_TEMP_STATIC, szBuf);

    if (pInfo->bIsNVMe && pInfo->bSMART_Supported) {
        _snprintf(szBuf, sizeof(szBuf),
            "NVMe Health Log   Spare: %d%%   Used: %d%%",
            (int)pInfo->nvmeHealth.AvailableSpare,
            (int)pInfo->nvmeHealth.PercentageUsed);
    } else if (pInfo->bIsNVMe && !pInfo->bSMART_Supported) {
        _snprintf(szBuf, sizeof(szBuf), "NVMe - Health Log not readable (run as Administrator)");
    } else if (pInfo->bIsUSB && !pInfo->bSMART_Supported) {
        _snprintf(szBuf, sizeof(szBuf), "Not available (USB bridge not supported)");
    } else if (pInfo->bIsUSB && pInfo->bSMART_Supported) {
        if (pInfo->eAccessMethod == SMART_ACCESS_SAT16 &&
            pInfo->attrData.stAttributes[0].bAttrID == 0 &&
            pInfo->attrData.wRevisionNumber == 0) {
            /* No ATA attribute table was populated — this came from the
             * SCSI LOG SENSE fallback only (temperature / failure flag). */
            _snprintf(szBuf, sizeof(szBuf), "USB - Limited (SCSI Log Sense)");
        } else {
            _snprintf(szBuf, sizeof(szBuf), "USB SAT passthrough - %s",
                      pInfo->bSMART_Enabled ? "Enabled" : "Detected");
        }
    } else if (pInfo->bSMART_Supported) {
        _snprintf(szBuf, sizeof(szBuf), "Supported   %s",
                  pInfo->bSMART_Enabled ? "Enabled" : "Disabled");
    } else {
        _snprintf(szBuf, sizeof(szBuf), "Not Supported");
    }
    SetDlgItemTextA(hWnd, IDC_STATUS_STATIC, szBuf);

    if (pInfo->nReadSpeedMBs > 0)
        _snprintf(szBuf, sizeof(szBuf), "%d MB/s", pInfo->nReadSpeedMBs);
    else
        _snprintf(szBuf, sizeof(szBuf), "-");
    SetDlgItemTextA(hWnd, IDC_READ_SPEED_STATIC, szBuf);

    char szReason[256];
    szReason[0] = '\0';
    if (pInfo->bIsNVMe && pInfo->bSMART_Supported) {
        BYTE crit = pInfo->nvmeHealth.CriticalWarning;
        if (crit != 0) {
            _snprintf(szReason, sizeof(szReason), "Critical Warning:%s%s%s%s%s",
                (crit & NVME_CRIT_WARN_SPARE_BELOW_THRESH)   ? " [Spare Low]"       : "",
                (crit & NVME_CRIT_WARN_TEMP_THRESHOLD)       ? " [Temp High]"       : "",
                (crit & NVME_CRIT_WARN_RELIABILITY_DEGRADED) ? " [Reliability!]"    : "",
                (crit & NVME_CRIT_WARN_READ_ONLY)            ? " [Read Only!]"      : "",
                (crit & NVME_CRIT_WARN_VOLATILE_MEM_BACKUP)  ? " [Volatile Mem]"    : "");
        }
    } else if (pInfo->bSMART_Supported && !pInfo->bIsUSB) {
        int k;
        DWORD dwR05=0, dwRC4=0, dwRC5=0, dwRC6=0, dwRBB=0;
        for (k = 0; k < 30; k++) {
            BYTE id = pInfo->attrData.stAttributes[k].bAttrID;
            DWORD dw = GetRawValue(pInfo->attrData.stAttributes[k].bRawValue);
            if      (id == 0x05) dwR05 = dw;
            else if (id == 0xC4) dwRC4 = dw;
            else if (id == 0xC5) dwRC5 = dw;
            else if (id == 0xC6) dwRC6 = dw;
            else if (id == 0xBB) dwRBB = dw;
        }
        if (dwRC6 > 0)
            _snprintf(szReason, sizeof(szReason),
                "Uncorrectable: %u   Reallocated: %u   Pending: %u   Events: %u",
                (unsigned)dwRC6, (unsigned)dwR05, (unsigned)dwRC5, (unsigned)dwRC4);
        else if (dwRC5 > 0 || dwR05 > 0)
            _snprintf(szReason, sizeof(szReason),
                "Reallocated: %u   Pending: %u   Events: %u   ECC: %u",
                (unsigned)dwR05, (unsigned)dwRC5, (unsigned)dwRC4, (unsigned)dwRBB);
    }

    if (pInfo->bIsUSB && !pInfo->bSMART_Supported) {
        char szBridge[64];
        if (pInfo->szBridgeVendor[0] || pInfo->szBridgeProduct[0]) {
            _snprintf(szBridge, sizeof(szBridge), " [Bridge: %s %s]",
                pInfo->szBridgeVendor, pInfo->szBridgeProduct);
        } else {
            szBridge[0] = '\0';
        }

        DWORD dwErr = pInfo->dwErrSat16;
        if (dwErr == 0) dwErr = pInfo->dwErrSat12;
        if (dwErr == 0) dwErr = pInfo->dwErrStorageProtocol;
        if (dwErr == 0) dwErr = pInfo->dwErrNvmeProtocol;
        if (dwErr == 0) dwErr = pInfo->dwErrNvmePassthrough;
        if (dwErr == 0) dwErr = pInfo->dwErrLogSense;

        const char* pszMeaning = "";
        if (dwErr == ERROR_INVALID_FUNCTION)      pszMeaning = " (bridge does not implement this command)";
        else if (dwErr == ERROR_NOT_SUPPORTED)    pszMeaning = " (bridge explicitly rejected the command)";
        else if (dwErr == ERROR_IO_DEVICE)        pszMeaning = " (USB driver/bridge I/O error)";
        else if (dwErr == ERROR_INVALID_DATA)     pszMeaning = " (bridge returned empty/zeroed data)";
        else if (dwErr >= 0x10000)                pszMeaning = " (SCSI command rejected with non-zero status)";

        if (dwErr != 0) {
            _snprintf(szBuf, sizeof(szBuf),
                "USB/External drive detected. SMART not accessible%s. "
                "Last error: %lu%s.",
                szBridge, (unsigned long)dwErr, pszMeaning);
        } else {
            _snprintf(szBuf, sizeof(szBuf),
                "USB/External drive detected%s. SMART not accessible "
                "(bridge does not support SAT passthrough).", szBridge);
        }
        SetDlgItemTextA(hWnd, IDC_PREDICT_STATIC, szBuf);
    } else if (pInfo->bIsNVMe && !pInfo->bSMART_Supported) {
        SetDlgItemTextA(hWnd, IDC_PREDICT_STATIC,
            "NVMe SMART data could not be read. Run as Administrator and click Refresh. "
            "Some controllers (e.g. Samsung, SK Hynix, Micron) require Windows 10 v1903+ drivers.");
    } else if (pInfo->nHealthPercent < 0) {
        SetDlgItemTextA(hWnd, IDC_PREDICT_STATIC,
            "SMART data could not be read. Run as Administrator and click Refresh.");
    } else if (pInfo->bPredictFailure) {
        SetDlgItemTextA(hWnd, IDC_PREDICT_STATIC, "!! DRIVE FAILURE PREDICTED BY DRIVE !!");
    } else if (pInfo->nHealthPercent < 40) {
        char szMsg[320];
        _snprintf(szMsg, sizeof(szMsg), "!! Poor Health - Back up data immediately!  %s", szReason);
        SetDlgItemTextA(hWnd, IDC_PREDICT_STATIC, szMsg);
    } else if (pInfo->nHealthPercent < 70) {
        char szMsg[320];
        _snprintf(szMsg, sizeof(szMsg), "Caution ! Monitor drive closely.  %s", szReason);
        SetDlgItemTextA(hWnd, IDC_PREDICT_STATIC, szMsg);
    } else if (pInfo->nHealthPercent < 100) {
        char szMsg[320];
        _snprintf(szMsg, sizeof(szMsg), "Good.  %s",
            (szReason[0] ? szReason : "All critical attributes normal."));
        SetDlgItemTextA(hWnd, IDC_PREDICT_STATIC, szMsg);
    } else {
        SetDlgItemTextA(hWnd, IDC_PREDICT_STATIC, "Excellent - Drive is in perfect condition.");
    }

    RepaintHealthBar();
}

static void ListViewSetCellIfChanged(HWND hList, int iItem, int iSubItem, const char* pszNew)
{
    char szOld[128] = "";
    LVITEMA lvi;
    ZeroMemory(&lvi, sizeof(lvi));
    lvi.mask       = LVIF_TEXT;
    lvi.iItem      = iItem;
    lvi.iSubItem   = iSubItem;
    lvi.pszText    = szOld;
    lvi.cchTextMax = (int)sizeof(szOld);
    ListView_GetItem(hList, &lvi);
    if (strcmp(szOld, pszNew) != 0) {
        lvi.pszText = (LPSTR)pszNew;
        ListView_SetItem(hList, &lvi);
    }
}

typedef struct {
    char col[7][128];
} ATTR_ROW;

#define MAX_ATTR_ROWS 32

static void FormatSmartValue(BYTE bID, BYTE* pRaw,
                              BYTE bVal, BYTE bWorst, BYTE bThresh,
                              char* szBuf, int nBufLen)
{

    DWORD dw32 = ((DWORD)pRaw[3] << 24) | ((DWORD)pRaw[2] << 16) |
                 ((DWORD)pRaw[1] <<  8) |  (DWORD)pRaw[0];
    WORD  w16  = ((WORD)pRaw[1] << 8) | (WORD)pRaw[0];
    unsigned __int64 qw48 =
        ((unsigned __int64)pRaw[5] << 40) |
        ((unsigned __int64)pRaw[4] << 32) |
        ((unsigned __int64)pRaw[3] << 24) |
        ((unsigned __int64)pRaw[2] << 16) |
        ((unsigned __int64)pRaw[1] <<  8) |
         (unsigned __int64)pRaw[0];

    char szSuffix[48] = "";
    if (bThresh > 0)
        _snprintf(szSuffix, sizeof(szSuffix), "  [%d=%d>%d]", bVal, bWorst, bThresh);
    else if (bVal > 0 || bWorst > 0)
        _snprintf(szSuffix, sizeof(szSuffix), "  [%d=%d>0]", bVal, bWorst);

    char szMain[80] = "";

    switch (bID)
    {

    case 0xBE:
    {
        int nC = (int)pRaw[0];
        int nF = nC * 9 / 5 + 32;
        int nMin = (int)pRaw[2], nMax = (int)pRaw[4];
        if (nMin > 0 && nMax > nMin && nMax < 100)
            _snprintf(szMain, sizeof(szMain), "%d \xb0""C (%d \xb0""F)  min:%d max:%d", nC, nF, nMin, nMax);
        else
            _snprintf(szMain, sizeof(szMain), "%d \xb0""C (%d \xb0""F)", nC, nF);
        break;
    }
    case 0xC2:
    {
        int nC = (int)w16;
        if (nC <= 0 || nC > 100) nC = (int)bVal;
        int nF = nC * 9 / 5 + 32;
        int nMin = (int)pRaw[2], nMax = (int)pRaw[4];
        if (nMin > 0 && nMax > nMin && nMax < 100)
            _snprintf(szMain, sizeof(szMain), "%d \xb0""C (%d \xb0""F)  min:%d max:%d", nC, nF, nMin, nMax);
        else
            _snprintf(szMain, sizeof(szMain), "%d \xb0""C (%d \xb0""F)", nC, nF);
        break;
    }
    case 0xE7:
        if (bVal > 0 && bVal <= 100) {
            int nF = (int)bVal * 9 / 5 + 32;
            _snprintf(szMain, sizeof(szMain), "%d \xb0""C (%d \xb0""F)", (int)bVal, nF);
        } else {
            _snprintf(szMain, sizeof(szMain), "%lu", (unsigned long)dw32);
        }
        break;

    case 0x09:
    {
        unsigned __int64 nHours = qw48;
        if (nHours > 200000) nHours = (unsigned __int64)w16;
        unsigned long nDays  = (unsigned long)(nHours / 24);
        double fYears = (double)nHours / 8760.0;
        if (fYears >= 1.0)
            _snprintf(szMain, sizeof(szMain), "%llu hours  (%.1f years)", nHours, fYears);
        else
            _snprintf(szMain, sizeof(szMain), "%llu hours  (%lu days)", nHours, nDays);
        break;
    }

    case 0x04:
        _snprintf(szMain, sizeof(szMain), "%lu times", (unsigned long)dw32);
        break;

    case 0x0C:
        _snprintf(szMain, sizeof(szMain), "%lu cycles", (unsigned long)dw32);
        break;

    case 0x03:
    {
        WORD wMs = w16;
        if (wMs > 0 && wMs < 30000)
            _snprintf(szMain, sizeof(szMain), "%u ms", wMs);
        else
            _snprintf(szMain, sizeof(szMain), "%lu", (unsigned long)dw32);
        break;
    }

    case 0x0A:
        if (dw32 == 0)
            _snprintf(szMain, sizeof(szMain), "0  (OK)");
        else
            _snprintf(szMain, sizeof(szMain), "%lu retries  (!)", (unsigned long)dw32);
        break;

    case 0x05:
    {
        DWORD dwSec = dw32 & 0xFFFF;
        if (dwSec == 0)
            _snprintf(szMain, sizeof(szMain), "0  (OK)");
        else
            _snprintf(szMain, sizeof(szMain), "%lu sectors  (!)", (unsigned long)dwSec);
        break;
    }

    case 0xC4:
        if (dw32 == 0)
            _snprintf(szMain, sizeof(szMain), "0 events  (OK)");
        else
            _snprintf(szMain, sizeof(szMain), "%lu events  (!)", (unsigned long)dw32);
        break;

    case 0xC5:
        if (dw32 == 0)
            _snprintf(szMain, sizeof(szMain), "0  (OK)");
        else
            _snprintf(szMain, sizeof(szMain), "%lu pending  (!)", (unsigned long)dw32);
        break;

    case 0xC6:
        if (dw32 == 0)
            _snprintf(szMain, sizeof(szMain), "0  (OK)");
        else
            _snprintf(szMain, sizeof(szMain), "%lu uncorrectable  (!)", (unsigned long)dw32);
        break;

    case 0xC7:
        if (dw32 == 0)
            _snprintf(szMain, sizeof(szMain), "0  (OK)");
        else
            _snprintf(szMain, sizeof(szMain), "%lu CRC errors  (!)", (unsigned long)dw32);
        break;

    case 0xBB:
    case 0xC3:
        if (dw32 == 0)
            _snprintf(szMain, sizeof(szMain), "0  (OK)");
        else
            _snprintf(szMain, sizeof(szMain), "%lu ECC errors", (unsigned long)dw32);
        break;

    case 0xBC:
    {
        WORD wTotal  = (WORD)pRaw[0] | ((WORD)pRaw[1] << 8);
        WORD wLatest = (WORD)pRaw[2] | ((WORD)pRaw[3] << 8);
        if (wTotal == 0)
            _snprintf(szMain, sizeof(szMain), "0  (OK)");
        else
            _snprintf(szMain, sizeof(szMain), "%u timeouts  (latest: %u)", wTotal, wLatest);
        break;
    }

    case 0xC1:
        _snprintf(szMain, sizeof(szMain), "%lu load/unload cycles", (unsigned long)dw32);
        break;

    case 0xC0:
        _snprintf(szMain, sizeof(szMain), "%lu emergency retracts", (unsigned long)dw32);
        break;

    case 0xB7:
        if (dw32 == 0)
            _snprintf(szMain, sizeof(szMain), "0  (OK)");
        else
            _snprintf(szMain, sizeof(szMain), "%lu downshifts  (!)", (unsigned long)dw32);
        break;

    case 0xB8:
        if (dw32 == 0)
            _snprintf(szMain, sizeof(szMain), "0  (OK)");
        else
            _snprintf(szMain, sizeof(szMain), "%lu errors  (!)", (unsigned long)dw32);
        break;

    case 0xBF:
        _snprintf(szMain, sizeof(szMain), "%lu shock events", (unsigned long)dw32);
        break;

    case 0x01:
    {
        DWORD dwErrHi = (((DWORD)pRaw[5] << 8) | (DWORD)pRaw[4]);
        if (dwErrHi > 0 && dw32 > 0 && dw32 < 0xFFFFFFFF)
            _snprintf(szMain, sizeof(szMain), "%lu / %lu  (err/total)", (unsigned long)dwErrHi, (unsigned long)dw32);
        else if (dw32 == 0)
            _snprintf(szMain, sizeof(szMain), "0  (OK)");
        else
            _snprintf(szMain, sizeof(szMain), "%lu", (unsigned long)dw32);
        break;
    }

    case 0x07:
    {
        DWORD dwErrHi = (((DWORD)pRaw[5] << 8) | (DWORD)pRaw[4]);
        if (dwErrHi > 0 && dw32 > 0)
            _snprintf(szMain, sizeof(szMain), "%lu / %lu  (err/seeks)", (unsigned long)dwErrHi, (unsigned long)dw32);
        else if (dw32 == 0)
            _snprintf(szMain, sizeof(szMain), "0  (OK)");
        else
            _snprintf(szMain, sizeof(szMain), "%lu", (unsigned long)dw32);
        break;
    }

    case 0xF1:
    case 0xF2:
    case 0xF3:
    case 0xF4:
    {
        unsigned __int64 nLBA = qw48;
        unsigned __int64 nGB  = nLBA / (1024ULL * 1024ULL * 2ULL);
        if (nGB >= 1024)
            _snprintf(szMain, sizeof(szMain), "%llu LBA  (~%llu TB)", nLBA, nGB / 1024ULL);
        else if (nGB > 0)
            _snprintf(szMain, sizeof(szMain), "%llu LBA  (~%llu GB)", nLBA, nGB);
        else
            _snprintf(szMain, sizeof(szMain), "%llu LBA", nLBA);
        break;
    }

    case 0xF9:
    case 0xE9:
    {
        unsigned __int64 nGiB = qw48;
        if (nGiB > 0 && nGiB < 1000000ULL)
            _snprintf(szMain, sizeof(szMain), "%llu GiB written", nGiB);
        else
            _snprintf(szMain, sizeof(szMain), "%llu", nGiB);
        break;
    }

    case 0xA9:
    {
        DWORD pct = dw32 ? dw32 : (DWORD)bVal;
        _snprintf(szMain, sizeof(szMain), "%lu%%  SSD life remaining", (unsigned long)pct);
        break;
    }

    case 0xAD:
        _snprintf(szMain, sizeof(szMain), "%lu wear-leveling cycles", (unsigned long)dw32);
        break;

    case 0xAB:
    case 0xAC:
    case 0xB5:
    case 0xB6:
    case 0xB0:
        if (dw32 == 0)
            _snprintf(szMain, sizeof(szMain), "0 failures  (OK)");
        else
            _snprintf(szMain, sizeof(szMain), "%lu failures  (!)", (unsigned long)dw32);
        break;

    case 0xAA:
    case 0xE8:
    {
        DWORD pct = dw32 ? dw32 : (DWORD)bVal;
        _snprintf(szMain, sizeof(szMain), "%lu%%  reserved space", (unsigned long)pct);
        break;
    }

    case 0xF0:
        _snprintf(szMain, sizeof(szMain), "%lu head flying hours", (unsigned long)(DWORD)w16);
        break;

    default:
        if (qw48 > 0xFFFFFFFFULL)
            _snprintf(szMain, sizeof(szMain), "%llu", qw48);
        else if (dw32 == 0)
            _snprintf(szMain, sizeof(szMain), "0");
        else
            _snprintf(szMain, sizeof(szMain), "%lu", (unsigned long)dw32);
        break;
    }

    _snprintf(szBuf, nBufLen, "%s%s", szMain, szSuffix);
}

void UpdateAttrList(HWND hWnd, int nDriveIdx)
{
    HWND hList = GetDlgItem(hWnd, IDC_ATTR_LIST);

    ATTR_ROW rows[MAX_ATTR_ROWS];
    int      nDesired = 0;

    if (nDriveIdx < 0 || nDriveIdx >= g_nDriveCount) {
        SendMessage(hList, WM_SETREDRAW, FALSE, 0);
        ListView_DeleteAllItems(hList);
        SendMessage(hList, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(hList, NULL, TRUE);
        return;
    }

    DRIVE_INFO* pInfo = &g_Drives[nDriveIdx];

    if (pInfo->bIsUSB && !pInfo->bSMART_Supported) {
        ATTR_ROW* r = &rows[0];
        _snprintf(r->col[0], sizeof(r->col[0]), "--");
        _snprintf(r->col[1], sizeof(r->col[1]), "SMART not available for USB/External drives");
        _snprintf(r->col[2], sizeof(r->col[2]), ""); _snprintf(r->col[3], sizeof(r->col[3]), "");
        nDesired = 1;
    }
    else if (pInfo->bIsNVMe && !pInfo->bSMART_Supported) {
        ATTR_ROW* r = &rows[0];
        _snprintf(r->col[0], sizeof(r->col[0]), "--");
        _snprintf(r->col[1], sizeof(r->col[1]), "NVMe SMART: Run as Administrator and click Refresh");
        _snprintf(r->col[2], sizeof(r->col[2]), ""); _snprintf(r->col[3], sizeof(r->col[3]), "");
        r = &rows[1];
        _snprintf(r->col[0], sizeof(r->col[0]), "--");
        _snprintf(r->col[1], sizeof(r->col[1]), "Some NVMe controllers require Windows 10 v1903+ or newer driver");
        _snprintf(r->col[2], sizeof(r->col[2]), ""); _snprintf(r->col[3], sizeof(r->col[3]), "");
        nDesired = 2;
    }
    else if (pInfo->bIsNVMe && pInfo->bSMART_Supported) {
        NVME_HEALTH_INFO_LOG* pLog = &pInfo->nvmeHealth;
        unsigned __int64 qwDataRead    = NVMeRead128Lo(pLog->DataUnitsRead);
        unsigned __int64 qwDataWritten = NVMeRead128Lo(pLog->DataUnitsWritten);
        unsigned __int64 qwPOH         = NVMeRead128Lo(pLog->PowerOnHours);
        unsigned __int64 qwPowerCycles = NVMeRead128Lo(pLog->PowerCycles);
        unsigned __int64 qwUnsafeSDs   = NVMeRead128Lo(pLog->UnsafeShutdowns);
        unsigned __int64 qwMediaErr    = NVMeRead128Lo(pLog->MediaErrors);
        unsigned __int64 qwErrLog      = NVMeRead128Lo(pLog->NumErrLogEntries);
        WORD wTempK = ReadLE16(pLog->CompositeTemperature);
        int  nTempC = (wTempK > 273) ? (int)wTempK - 273 : 0;

        #define NVME_ROW(id_, name_, val_, stat_) \
        { \
            ATTR_ROW* r = &rows[nDesired++]; \
            _snprintf(r->col[0], sizeof(r->col[0]), "%s", id_); \
            _snprintf(r->col[1], sizeof(r->col[1]), "%s", name_); \
            _snprintf(r->col[2], 128, "%s", val_); \
            _snprintf(r->col[3], sizeof(r->col[3]), "%s", stat_); \
        }

        char szCrit[64], szVBuf[64], szSpare[32], szSpTh[32], szPctU[32];
        char szDUR[64], szDUW[64], szPOH[32], szPC[32];
        char szUS[32], szME[32], szEL[32], szWCT[32], szCCT[32];

        if (pLog->CriticalWarning == 0) _snprintf(szCrit, sizeof(szCrit), "0 (None)");
        else _snprintf(szCrit, sizeof(szCrit), "0x%02X (!)", pLog->CriticalWarning);
        NVME_ROW("01h","Critical Warning",szCrit,(pLog->CriticalWarning?"WARNING":"OK"));

        _snprintf(szVBuf,sizeof(szVBuf),"%d C (%d K)",nTempC,(int)wTempK);
        NVME_ROW("02h","Composite Temperature",szVBuf,(nTempC>70?"Warning":"OK"));

        _snprintf(szSpare,sizeof(szSpare),"%d %%",(int)pLog->AvailableSpare);
        NVME_ROW("03h","Available Spare",szSpare,
            (pLog->AvailableSpare<pLog->AvailableSpareThreshold?"WARNING":"OK"));

        _snprintf(szSpTh,sizeof(szSpTh),"%d %%",(int)pLog->AvailableSpareThreshold);
        NVME_ROW("04h","Available Spare Threshold",szSpTh,"--");

        _snprintf(szPctU,sizeof(szPctU),"%d %%",(int)pLog->PercentageUsed);
        NVME_ROW("05h","Percentage Used (Endurance)",szPctU,
            (pLog->PercentageUsed>=100?"Warning":"OK"));

        if (qwDataRead>2048) _snprintf(szDUR,sizeof(szDUR),"%llu GB",(unsigned __int64)(qwDataRead/2048));
        else                  _snprintf(szDUR,sizeof(szDUR),"%llu units",qwDataRead);
        NVME_ROW("06h","Data Units Read",szDUR,"OK");

        if (qwDataWritten>2048) _snprintf(szDUW,sizeof(szDUW),"%llu GB",(unsigned __int64)(qwDataWritten/2048));
        else                     _snprintf(szDUW,sizeof(szDUW),"%llu units",qwDataWritten);
        NVME_ROW("07h","Data Units Written",szDUW,"OK");

        _snprintf(szPOH,sizeof(szPOH),"%llu hours",qwPOH);
        NVME_ROW("09h","Power On Hours",szPOH,"OK");

        _snprintf(szPC,sizeof(szPC),"%llu",qwPowerCycles);
        NVME_ROW("0Ch","Power Cycles",szPC,"OK");

        _snprintf(szUS,sizeof(szUS),"%llu",qwUnsafeSDs);
        NVME_ROW("10h","Unsafe Shutdowns",szUS,"OK");

        _snprintf(szME,sizeof(szME),"%llu",qwMediaErr);
        NVME_ROW("11h","Media and Data Integrity Errors",szME,(qwMediaErr>0?"WARNING":"OK"));

        _snprintf(szEL,sizeof(szEL),"%llu",qwErrLog);
        NVME_ROW("12h","Number of Error Log Entries",szEL,(qwErrLog>0?"Warning":"OK"));

        _snprintf(szWCT,sizeof(szWCT),"%lu min",pLog->WarningCompTempTime);
        NVME_ROW("13h","Warning Composite Temp Time",szWCT,(pLog->WarningCompTempTime>0?"Warning":"OK"));

        _snprintf(szCCT,sizeof(szCCT),"%lu min",pLog->CriticalCompTempTime);
        NVME_ROW("14h","Critical Composite Temp Time",szCCT,(pLog->CriticalCompTempTime>0?"WARNING":"OK"));

        #undef NVME_ROW
    }
    else if (pInfo->bSMART_Supported) {
        int i, j;
        for (i = 0; i < 30 && nDesired < MAX_ATTR_ROWS; i++) {
            SMART_ATTRIBUTE* pAttr = &pInfo->attrData.stAttributes[i];
            if (pAttr->bAttrID == 0) continue;

            BYTE bThresh = 0;
            for (j = 0; j < 30; j++) {
                if (pInfo->threshData.stThresholds[j].bAttrID == pAttr->bAttrID) {
                    bThresh = pInfo->threshData.stThresholds[j].bThresholdValue;
                    break;
                }
            }
            BOOL bFailed = (bThresh > 0 && pAttr->bAttrValue <= bThresh);
            DWORD dwRaw  = GetRawValue(pAttr->bRawValue);

            ATTR_ROW* r = &rows[nDesired++];
            _snprintf(r->col[0], 128, "%02Xh",   pAttr->bAttrID);
            _snprintf(r->col[1], 128, "%s",       GetAttrName(pAttr->bAttrID));
            FormatSmartValue(pAttr->bAttrID, pAttr->bRawValue,
                             pAttr->bAttrValue, pAttr->bWorstValue, bThresh,
                             r->col[2], 128);
            if (bFailed)
                _snprintf(r->col[3], sizeof(r->col[3]), "FAILED");
            else if (bThresh > 0 && pAttr->bAttrValue < bThresh + 10)
                _snprintf(r->col[3], sizeof(r->col[3]), "Warning");
            else
                _snprintf(r->col[3], sizeof(r->col[3]), "OK");
        }

        if (nDesired == 0) {
            /* Bridge gave us only SCSI LOG SENSE data (no ATA attribute
             * table). Show what was retrieved instead of an empty list. */
            ATTR_ROW* r = &rows[nDesired++];
            _snprintf(r->col[0], sizeof(r->col[0]), "--");
            _snprintf(r->col[1], sizeof(r->col[1]), "Predictive Failure (SCSI Informational Exceptions)");
            _snprintf(r->col[2], sizeof(r->col[2]), "%s", pInfo->bPredictFailure ? "Failure predicted" : "No failure predicted");
            _snprintf(r->col[3], sizeof(r->col[3]), "%s", pInfo->bPredictFailure ? "FAILED" : "OK");

            if (nDesired < MAX_ATTR_ROWS) {
                r = &rows[nDesired++];
                _snprintf(r->col[0], sizeof(r->col[0]), "--");
                _snprintf(r->col[1], sizeof(r->col[1]), "Temperature (SCSI Log Page)");
                if (pInfo->nTemperatureC > 0)
                    _snprintf(r->col[2], sizeof(r->col[2]), "%d C", pInfo->nTemperatureC);
                else
                    _snprintf(r->col[2], sizeof(r->col[2]), "Unavailable");
                _snprintf(r->col[3], sizeof(r->col[3]), "--");
            }

            if (nDesired < MAX_ATTR_ROWS) {
                r = &rows[nDesired++];
                _snprintf(r->col[0], sizeof(r->col[0]), "--");
                _snprintf(r->col[1], sizeof(r->col[1]), "Note: full SMART attribute table not exposed by this bridge");
                _snprintf(r->col[2], sizeof(r->col[2]), "");
                _snprintf(r->col[3], sizeof(r->col[3]), "");
            }
        }
    }

    SendMessage(hList, WM_SETREDRAW, FALSE, 0);

    int nCurrent = ListView_GetItemCount(hList);

    int row;
    for (row = 0; row < nDesired; row++) {
        if (row >= nCurrent) {
            LVITEMA lvi;
            ZeroMemory(&lvi, sizeof(lvi));
            lvi.mask     = LVIF_TEXT;
            lvi.iItem    = row;
            lvi.iSubItem = 0;
            lvi.pszText  = rows[row].col[0];
            ListView_InsertItem(hList, &lvi);
        }
        int col;
        for (col = 0; col < 4; col++)
            ListViewSetCellIfChanged(hList, row, col, rows[row].col[col]);
    }

    while (ListView_GetItemCount(hList) > nDesired)
        ListView_DeleteItem(hList, ListView_GetItemCount(hList) - 1);

    SendMessage(hList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hList, NULL, FALSE);
    UpdateWindow(hList);
}

static volatile LONG g_bScanBusy = 0;

static DWORD WINAPI RefreshThreadProc(LPVOID lpParam)
{
    HWND hWnd = (HWND)lpParam;
    Snapshot_Save();
    g_nDriveCount = ScanDrives(g_Drives, MAX_DRIVES);
    History_Record(g_Drives, g_nDriveCount);
    Snapshot_Diff();
    CheckCriticalAlerts();
    PostMessage(hWnd, WM_APP_REFRESH_DONE, 0, 0);
    return 0;
}

void RefreshData(HWND hWnd)
{
    if (InterlockedCompareExchange(&g_bScanBusy, 1, 0) != 0)
        return;

    HANDLE hThread = CreateThread(NULL, 0, RefreshThreadProc, hWnd, 0, NULL);
    if (hThread)
        CloseHandle(hThread);
    else {
        g_nDriveCount = ScanDrives(g_Drives, MAX_DRIVES);
        PostMessage(hWnd, WM_APP_REFRESH_DONE, 0, 0);
    }
}

#define ABOUT_W  440
#define ABOUT_H  360

/* Control identifiers used inside the About dialog. */
#define IDC_ABOUT_LINK       3500
#define IDC_ABOUT_LIC_STATUS 3501   
#define IDC_ABOUT_ACTIVATE   3502   
#define ABOUT_URL       "https://github.com/sponsors/arisohandriputra/"

static HCURSOR s_hCursorHand = NULL;

static LRESULT CALLBACK AboutDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        {
            int cx = ABOUT_W;

            HWND hIco = CreateWindowExA(0, "STATIC", "",
                WS_CHILD | WS_VISIBLE | SS_ICON | SS_CENTERIMAGE,
                (cx - 32) / 2, 18, 32, 32,
                hDlg, (HMENU)0, g_hInst, NULL);
            HICON hIc = (HICON)LoadImageA(g_hInst, MAKEINTRESOURCEA(IDI_APPICON),
                IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
            if (hIc) SendMessageA(hIco, STM_SETICON, (WPARAM)hIc, 0);

            HFONT hFontBold = CreateFontA(-15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            HWND hName = CreateWindowExA(0, "STATIC", "HDDHealth Monitor",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                20, 58, cx - 40, 22,
                hDlg, (HMENU)0, g_hInst, NULL);
            SendMessageA(hName, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            HWND hDesc = CreateWindowExA(0, "STATIC",
                "Monitors HDD health and S.M.A.R.T. data at the low level.",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                20, 82, cx - 40, 18,
                hDlg, (HMENU)0, g_hInst, NULL);
            SendMessageA(hDesc, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            CreateWindowExA(0, "STATIC", "",
                WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
                20, 108, cx - 40, 2,
                hDlg, (HMENU)0, g_hInst, NULL);

            HWND hCopy = CreateWindowExA(0, "STATIC",
                "Copyright \xa9 2026 ARImetic Inc. All Rights Reserved.",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                20, 118, cx - 40, 18,
                hDlg, (HMENU)0, g_hInst, NULL);
            SendMessageA(hCopy, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            HWND hDrv = CreateWindowExA(0, "STATIC",
                "Author: Ari Sohandri Putra -  Open Source / MIT License",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                20, 140, cx - 40, 18,
                hDlg, (HMENU)0, g_hInst, NULL);
            SendMessageA(hDrv, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            CreateWindowExA(0, "STATIC", "",
                WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
                20, 166, cx - 40, 2,
                hDlg, (HMENU)0, g_hInst, NULL);

            {
                HWND hLicStatus = CreateWindowExA(0, "STATIC",
                    "Free Open Source Software",
                    WS_CHILD | WS_VISIBLE | SS_CENTER,
                    20, 176, cx - 40, 18,
                    hDlg, (HMENU)IDC_ABOUT_LIC_STATUS, g_hInst, NULL);
                SendMessageA(hLicStatus, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

                HWND hActivate = CreateWindowExA(0, "BUTTON", "Donate",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    (cx - 120) / 2, 200, 120, 26,
                    hDlg, (HMENU)IDC_ABOUT_ACTIVATE, g_hInst, NULL);
                SendMessageA(hActivate, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            }

            CreateWindowExA(0, "STATIC", "",
                WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
                20, 234, cx - 40, 2,
                hDlg, (HMENU)0, g_hInst, NULL);

            HFONT hFontLink = CreateFontA(-12, 0, 0, 0, FW_NORMAL, FALSE, TRUE, FALSE,
                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            HWND hLink = CreateWindowExA(0, "STATIC",
                "Sponsor this project on GitHub",
                WS_CHILD | WS_VISIBLE | SS_CENTER | SS_NOTIFY,
                20, 244, cx - 40, 18,
                hDlg, (HMENU)IDC_ABOUT_LINK, g_hInst, NULL);
            SendMessageA(hLink, WM_SETFONT, (WPARAM)hFontLink, TRUE);

            HWND hBtn = CreateWindowExA(0, "BUTTON", "OK",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                (cx - 80) / 2, 274, 80, 26,
                hDlg, (HMENU)IDOK, g_hInst, NULL);
            SendMessageA(hBtn, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            if (!s_hCursorHand)
                s_hCursorHand = LoadCursorA(NULL, (LPCSTR)IDC_HAND);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            DestroyWindow(hDlg);
        }
        else if (LOWORD(wParam) == IDC_ABOUT_ACTIVATE) {
            Donate_OpenSponsorsPage(hDlg);
        }
        return 0;

    case WM_NOTIFY:
        return 0;

    case WM_CTLCOLORSTATIC:
        {
            HWND hCtrl = (HWND)lParam;
            int  nID   = GetDlgCtrlID(hCtrl);
            HDC  hdcSt = (HDC)wParam;
            SetBkMode(hdcSt, TRANSPARENT);
            if (nID == IDC_ABOUT_LINK) {
                /* Sponsor link - render in hyperlink blue. */
                SetTextColor(hdcSt, RGB(0, 102, 204));
            } else if (nID == IDC_ABOUT_LIC_STATUS) {
                /* FOSS banner - render in green to signal "free / good". */
                SetTextColor(hdcSt, RGB(0, 140, 0));
            }
            return (LRESULT)(HBRUSH)(COLOR_WINDOW + 1);
        }

    case WM_SETCURSOR:
        {

            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hDlg, &pt);
            HWND hOver = ChildWindowFromPoint(hDlg, pt);
            if (hOver && GetDlgCtrlID(hOver) == IDC_ABOUT_LINK && s_hCursorHand) {
                SetCursor(s_hCursorHand);
                return TRUE;
            }
            break;
        }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE || wParam == VK_RETURN) {
            DestroyWindow(hDlg);
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hDlg);
        return 0;
    }

    if (uMsg == WM_COMMAND &&
        LOWORD(wParam) == IDC_ABOUT_LINK &&
        HIWORD(wParam) == STN_CLICKED) {
        /* Sponsor link clicked - open the GitHub Sponsors page. */
        Donate_OpenSponsorsPage(hDlg);
        return 0;
    }

    return DefWindowProcA(hDlg, uMsg, wParam, lParam);
}

void ShowAboutDialog(HWND hWndParent)
{

    static BOOL bRegistered = FALSE;
    if (!bRegistered) {
        WNDCLASSA wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc   = AboutDlgProc;
        wc.hInstance     = g_hInst;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hCursor       = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
        wc.hIcon         = LoadIconA(g_hInst, MAKEINTRESOURCEA(IDI_APPICON));
        wc.lpszClassName = "LLHDAboutDlg";
        RegisterClassA(&wc);
        bRegistered = TRUE;
    }

    HWND hExist = FindWindowA("LLHDAboutDlg", NULL);
    if (hExist) { SetForegroundWindow(hExist); return; }

    int nScrW = GetSystemMetrics(SM_CXSCREEN);
    int nScrH = GetSystemMetrics(SM_CYSCREEN);

    int nX, nY;
    if (hWndParent) {
        RECT rcP;
        GetWindowRect(hWndParent, &rcP);
        nX = rcP.left + (rcP.right  - rcP.left - ABOUT_W) / 2;
        nY = rcP.top  + (rcP.bottom - rcP.top  - ABOUT_H) / 2;
    } else {
        nX = (nScrW - ABOUT_W) / 2;
        nY = (nScrH - ABOUT_H) / 2;
    }

    RECT rcAdj = {0, 0, ABOUT_W, ABOUT_H};
    AdjustWindowRectEx(&rcAdj, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                       FALSE, WS_EX_DLGMODALFRAME);
    int nWinW = rcAdj.right  - rcAdj.left;
    int nWinH = rcAdj.bottom - rcAdj.top;

    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "LLHDAboutDlg",
        "About HDDHealth Monitor",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        nX, nY, nWinW, nWinH,
        hWndParent, NULL, g_hInst, NULL
    );
    if (!hDlg) return;

    HICON hIco = LoadIconA(g_hInst, MAKEINTRESOURCEA(IDI_APPICON));
    SendMessageA(hDlg, WM_SETICON, ICON_BIG,   (LPARAM)hIco);
    SendMessageA(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)
        LoadImageA(g_hInst, MAKEINTRESOURCEA(IDI_APPICON),
                   IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
}

static void CreateMenuBar(HWND hWnd)
{
    HMENU hMenuBar = CreateMenu();

    HMENU hFile = CreatePopupMenu();
    AppendMenuA(hFile, MF_STRING, IDM_SCREENSHOT, "Save Screenshot\tCtrl+S");
    AppendMenuA(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hFile, MF_STRING, IDM_EXIT,       "Exit");
    AppendMenuA(hMenuBar, MF_POPUP, (UINT_PTR)hFile, "File");

    HMENU hView = CreatePopupMenu();
    AppendMenuA(hView, MF_STRING, IDM_HISTORY, "History Graph");
    AppendMenuA(hMenuBar, MF_POPUP, (UINT_PTR)hView, "View");

    HMENU hHelp = CreatePopupMenu();
    AppendMenuA(hHelp, MF_STRING, IDM_DONATE, "Donate...");
    AppendMenuA(hHelp, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hHelp, MF_STRING, IDM_ABOUT, "About HDDHealth Monitor");
    AppendMenuA(hMenuBar, MF_POPUP, (UINT_PTR)hHelp, "Help");

    SetMenu(hWnd, hMenuBar);
}

void CreateControls(HWND hWnd)
{
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC  = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    int i;
    for (i = 0; i < MAX_DRIVES; i++) g_hDriveBtn[i] = NULL;

    HWND hDriveLabel = CreateWindowExA(0, "STATIC", "DRIVES",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        6, 8, DRIVE_BTN_PANEL_W - 12, 16,
        hWnd, (HMENU)(IDC_DRIVE_LIST), g_hInst, NULL);
    SendMessage(hDriveLabel, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

    int nRightX = DRIVE_BTN_PANEL_W + 10;
    int nBarsW  = 190;

    HWND hPerfLabel = CreateWindowExA(0, "STATIC", "DISK PERFORMANCE",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nRightX, 40, nBarsW, 14,
        hWnd, (HMENU)IDC_PERF_LABEL, g_hInst, NULL);
    SendMessage(hPerfLabel, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

    g_hPerfBar = CreateWindowExA(WS_EX_CLIENTEDGE, "LLHDPerfBar", "",
        WS_CHILD | WS_VISIBLE,
        nRightX, 56, nBarsW, 40,
        hWnd, (HMENU)IDC_PERF_BAR_FRAME, g_hInst, NULL);

    HWND hLabel = CreateWindowExA(0, "STATIC", "DISK HEALTH",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nRightX, 102, nBarsW, 14,
        hWnd, (HMENU)IDC_HEALTH_LABEL, g_hInst, NULL);
    SendMessage(hLabel, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

    g_hHealthBar = CreateWindowExA(WS_EX_CLIENTEDGE, "LLHDHealthBar", "",
        WS_CHILD | WS_VISIBLE,
        nRightX, 118, nBarsW, 40,
        hWnd, (HMENU)IDC_HEALTH_BAR_FRAME, g_hInst, NULL);

    int nInfoX   = nRightX + nBarsW + 10;
    int nInfoY   = 36;
    int nInfoH   = 18;
    int nInfoGap = 4;
    int nLblW    = 90;
    int nValX    = nInfoX + nLblW + 4;
    int nValW    = WINDOW_W - nValX - 8;

    { HWND h = CreateWindowExA(0, "STATIC", "Model",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nInfoX, nInfoY + (nInfoH + nInfoGap) * 0, nLblW, nInfoH,
        hWnd, (HMENU)IDC_MODEL_LABEL, g_hInst, NULL);
      SendMessage(h, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE); }
    { HWND h = CreateWindowExA(0, "STATIC", "-",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nValX, nInfoY + (nInfoH + nInfoGap) * 0, nValW, nInfoH,
        hWnd, (HMENU)IDC_MODEL_STATIC, g_hInst, NULL);
      SendMessage(h, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE); }

    { HWND h = CreateWindowExA(0, "STATIC", "Serial No.",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nInfoX, nInfoY + (nInfoH + nInfoGap) * 1, nLblW, nInfoH,
        hWnd, (HMENU)IDC_SERIAL_LABEL, g_hInst, NULL);
      SendMessage(h, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE); }
    { HWND h = CreateWindowExA(0, "STATIC", "-",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nValX, nInfoY + (nInfoH + nInfoGap) * 1, nValW, nInfoH,
        hWnd, (HMENU)IDC_SERIAL_STATIC, g_hInst, NULL);
      SendMessage(h, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE); }

    { HWND h = CreateWindowExA(0, "STATIC", "Firmware",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nInfoX, nInfoY + (nInfoH + nInfoGap) * 2, nLblW, nInfoH,
        hWnd, (HMENU)IDC_FIRMWARE_LABEL, g_hInst, NULL);
      SendMessage(h, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE); }
    { HWND h = CreateWindowExA(0, "STATIC", "-",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nValX, nInfoY + (nInfoH + nInfoGap) * 2, nValW, nInfoH,
        hWnd, (HMENU)IDC_FIRMWARE_STATIC, g_hInst, NULL);
      SendMessage(h, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE); }

    { HWND h = CreateWindowExA(0, "STATIC", "Capacity",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nInfoX, nInfoY + (nInfoH + nInfoGap) * 3, nLblW, nInfoH,
        hWnd, (HMENU)IDC_SIZE_LABEL, g_hInst, NULL);
      SendMessage(h, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE); }
    { HWND h = CreateWindowExA(0, "STATIC", "-",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nValX, nInfoY + (nInfoH + nInfoGap) * 3, nValW, nInfoH,
        hWnd, (HMENU)IDC_SIZE_STATIC, g_hInst, NULL);
      SendMessage(h, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE); }

    { HWND h = CreateWindowExA(0, "STATIC", "Temperature",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nInfoX, nInfoY + (nInfoH + nInfoGap) * 4, nLblW, nInfoH,
        hWnd, (HMENU)IDC_TEMP_LABEL, g_hInst, NULL);
      SendMessage(h, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE); }
    { HWND h = CreateWindowExA(0, "STATIC", "-",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nValX, nInfoY + (nInfoH + nInfoGap) * 4, nValW, nInfoH,
        hWnd, (HMENU)IDC_TEMP_STATIC, g_hInst, NULL);
      SendMessage(h, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE); }

    { HWND h = CreateWindowExA(0, "STATIC", "S.M.A.R.T.",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nInfoX, nInfoY + (nInfoH + nInfoGap) * 5, nLblW, nInfoH,
        hWnd, (HMENU)IDC_STATUS_LABEL, g_hInst, NULL);
      SendMessage(h, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE); }
    { HWND h = CreateWindowExA(0, "STATIC", "-",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nValX, nInfoY + (nInfoH + nInfoGap) * 5, nValW, nInfoH,
        hWnd, (HMENU)IDC_STATUS_STATIC, g_hInst, NULL);
      SendMessage(h, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE); }

    { HWND h = CreateWindowExA(0, "STATIC", "Sec. Speed",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nInfoX, nInfoY + (nInfoH + nInfoGap) * 6, nLblW, nInfoH,
        hWnd, (HMENU)IDC_READ_SPEED_LABEL, g_hInst, NULL);
      SendMessage(h, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE); }
    { HWND h = CreateWindowExA(0, "STATIC", "-",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nValX, nInfoY + (nInfoH + nInfoGap) * 6, nValW, nInfoH,
        hWnd, (HMENU)IDC_READ_SPEED_STATIC, g_hInst, NULL);
      SendMessage(h, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE); }

    HWND hPred = CreateWindowExA(0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nRightX, 240, 430, 17,
        hWnd, (HMENU)IDC_PREDICT_STATIC, g_hInst, NULL);

    SendMessage(hPred, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    HWND hHistBtn = CreateWindowExA(0, "BUTTON", "History Graph",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        nRightX, 160, 120, 24,
        hWnd, (HMENU)IDC_HISTORY_BTN_MAIN, g_hInst, NULL);
    SendMessage(hHistBtn, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

    HWND hList = CreateWindowExA(
        WS_EX_CLIENTEDGE, WC_LISTVIEWA, NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
        nRightX, 262, 540, 340,
        hWnd, (HMENU)IDC_ATTR_LIST, g_hInst, NULL
    );
    SendMessage(hList, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    ListView_SetExtendedListViewStyle(hList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    ListView_SetBkColor(hList, CLR_PANEL);
    ListView_SetTextBkColor(hList, CLR_ROW1);
    ListView_SetTextColor(hList, CLR_TEXT);

    LVCOLUMNA col;
    ZeroMemory(&col, sizeof(col));
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    col.fmt  = LVCFMT_LEFT;

    col.cx = 42;  col.pszText = (LPSTR)"ID";          ListView_InsertColumn(hList, 0, &col);
    col.cx = 150; col.pszText = (LPSTR)"Attribute";    ListView_InsertColumn(hList, 1, &col);
    col.cx = 240; col.pszText = (LPSTR)"Value / Info"; ListView_InsertColumn(hList, 2, &col);
    col.cx = 80;  col.pszText = (LPSTR)"Status";       ListView_InsertColumn(hList, 3, &col);
}

static LRESULT HandleCtlColor(HWND hWnd, WPARAM wParam)
{
    HDC  hdc     = (HDC)wParam;

    HWND hSender = WindowFromDC(hdc);
    if (hSender) {
        int id = GetDlgCtrlID(hSender);
        if (id == IDC_MODEL_LABEL    || id == IDC_SERIAL_LABEL   ||
            id == IDC_FIRMWARE_LABEL || id == IDC_SIZE_LABEL      ||
            id == IDC_TEMP_LABEL     || id == IDC_STATUS_LABEL    ||
            id == IDC_READ_SPEED_LABEL) {
            SetTextColor(hdc, CLR_TEXT_DIM);
            SetBkColor(hdc, CLR_BG);
            return (LRESULT)g_hbrBG;
        }
    }
    SetTextColor(hdc, CLR_TEXT);
    SetBkColor(hdc, CLR_BG);
    return (LRESULT)g_hbrBG;
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        g_hMainWnd = hWnd;
        RegisterHealthBarClass(g_hInst);
        Graph_RegisterClass(g_hInst);
        History_Init();
        History_Load();
        CreateGDIObjects();
        CreateMenuBar(hWnd);
        CreateControls(hWnd);
        TrayIcon_Add(hWnd);
        DeviceNotify_Register(hWnd);
        SetTimer(hWnd, IDT_REFRESH, REFRESH_INTERVAL_MS, NULL);
        SetTimer(hWnd, IDT_TITLE_UPDATE, 1000, NULL);

        UpdateWindowTitle(hWnd);
        RefreshData(hWnd);
        {
            GdiplusStartupInput gdipInput;
            GdiplusStartup(&g_gdiplusToken, &gdipInput, NULL);
        }
        return 0;

    case WM_ERASEBKGND:
        {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hWnd, &rc);
            FillRect(hdc, &rc, g_hbrBG);
        }
        return 1;

    case WM_CTLCOLORSTATIC:
        return HandleCtlColor(hWnd, wParam);

    case WM_CTLCOLORBTN:
        return (LRESULT)(HBRUSH)(COLOR_BTNFACE + 1);

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, CLR_TEXT);
            SetBkColor(hdc, CLR_PANEL);
            return (LRESULT)g_hbrPanel;
        }

    case WM_NOTIFY:
        {
            NMHDR* pHdr = (NMHDR*)lParam;
            if (pHdr->idFrom == IDC_ATTR_LIST && pHdr->code == NM_CUSTOMDRAW) {
                NMLVCUSTOMDRAW* pCD = (NMLVCUSTOMDRAW*)lParam;

                switch (pCD->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;

                case CDDS_ITEMPREPAINT:
                    pCD->clrTextBk = (pCD->nmcd.dwItemSpec % 2 == 0) ? CLR_ROW1 : CLR_ROW2;
                    pCD->clrText   = CLR_TEXT;
                    return CDRF_NOTIFYSUBITEMDRAW;

                case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
                    {
                        COLORREF clrRowBk = (pCD->nmcd.dwItemSpec % 2 == 0)
                                            ? CLR_ROW1 : CLR_ROW2;
                        pCD->clrTextBk = clrRowBk;
                        pCD->clrText   = CLR_TEXT;

                        if (pCD->iSubItem == 2) {

                            char szVal[128], szStat[32];
                            LVITEMA lvi;
                            ZeroMemory(&lvi, sizeof(lvi));
                            lvi.mask       = LVIF_TEXT;
                            lvi.iItem      = (int)pCD->nmcd.dwItemSpec;
                            lvi.iSubItem   = 2;
                            lvi.pszText    = szVal;
                            lvi.cchTextMax = sizeof(szVal);
                            ListView_GetItem(pCD->nmcd.hdr.hwndFrom, &lvi);

                            lvi.iSubItem   = 3;
                            lvi.pszText    = szStat;
                            lvi.cchTextMax = sizeof(szStat);
                            ListView_GetItem(pCD->nmcd.hdr.hwndFrom, &lvi);

                            COLORREF clrVal;
                            BOOL     bBold      = FALSE;
                            BOOL     bHasAlert  = (strstr(szVal, "(!)") != NULL);

                            if (strcmp(szStat, "FAILED") == 0) {
                                clrVal = RGB(220, 38,  38);
                                bBold  = TRUE;
                            } else if (_stricmp(szStat, "Warning") == 0 || bHasAlert) {
                                clrVal = RGB(194, 100,  0);
                                bBold  = TRUE;
                            } else if (strcmp(szStat, "OK") == 0) {

                                clrVal = CLR_TEXT;
                            } else {

                                clrVal = CLR_TEXT_DIM;
                            }

                            HDC  hdc    = pCD->nmcd.hdc;
                            RECT rcCell = pCD->nmcd.rc;

                            HBRUSH hbrRow2 = CreateSolidBrush(clrRowBk);
                            FillRect(hdc, &rcCell, hbrRow2);
                            DeleteObject(hbrRow2);

                            if (szVal[0] == '\0') return CDRF_SKIPDEFAULT;

                            SetBkMode(hdc, TRANSPARENT);

                            int nFull = lstrlenA(szVal);

                            char* pBracket = strchr(szVal, '[');
                            char* pAlert   = strstr(szVal, "(!)");
                            char* pOK      = strstr(szVal, "(OK)");
                            char* pNone    = strstr(szVal, "(None)");

                            HFONT hCurFont = (HFONT)GetCurrentObject(hdc, OBJ_FONT);
                            LOGFONTA lf;
                            GetObjectA(hCurFont, sizeof(lf), &lf);
                            HFONT hBoldFont = NULL, hDimFont = NULL;
                            if (bBold) {
                                LOGFONTA lfBold = lf;
                                lfBold.lfWeight = FW_SEMIBOLD;
                                hBoldFont = CreateFontIndirectA(&lfBold);
                            }
                            {
                                LOGFONTA lfDim = lf;
                                lfDim.lfWeight = FW_NORMAL;
                                hDimFont = CreateFontIndirectA(&lfDim);
                            }

                            RECT rcDraw = rcCell;
                            rcDraw.left += 4;

                            if (pBracket && (pAlert == NULL)) {

                                int nMain  = (int)(pBracket - szVal);

                                while (nMain > 0 && szVal[nMain-1] == ' ') nMain--;

                                if (bBold && hBoldFont) SelectObject(hdc, hBoldFont);
                                SetTextColor(hdc, clrVal);
                                DrawTextA(hdc, szVal, nMain, &rcDraw,
                                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
                                int nMainW = rcDraw.right - rcDraw.left;
                                rcDraw.right = rcCell.right;
                                DrawTextA(hdc, szVal, nMain, &rcDraw,
                                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                                RECT rcBrk = rcDraw;
                                rcBrk.left += nMainW + 4;
                                SelectObject(hdc, hDimFont);
                                SetTextColor(hdc, RGB(148, 163, 184));
                                DrawTextA(hdc, pBracket, -1, &rcBrk,
                                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                            } else {

                                int iSplit = -1;
                                {
                                    int k;
                                    for (k = 0; k < nFull; k++) {
                                        if (szVal[k] == ' ' && k > 0) { iSplit = k; break; }
                                    }
                                }

                                COLORREF clrUnit = CLR_TEXT_DIM;

                                if (pAlert) {

                                    if (bBold && hBoldFont) SelectObject(hdc, hBoldFont);
                                    SetTextColor(hdc, clrVal);
                                    DrawTextA(hdc, szVal, -1, &rcDraw,
                                              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                                } else if (pOK) {

                                    int nPre = (int)(pOK - szVal);
                                    while (nPre > 0 && szVal[nPre-1] == ' ') nPre--;
                                    SelectObject(hdc, bBold && hBoldFont ? hBoldFont : hDimFont);
                                    SetTextColor(hdc, clrVal);
                                    if (nPre > 0) {
                                        DrawTextA(hdc, szVal, nPre, &rcDraw,
                                                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
                                        int nW = rcDraw.right - rcDraw.left;
                                        rcDraw.right = rcCell.right;
                                        DrawTextA(hdc, szVal, nPre, &rcDraw,
                                                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                                        RECT rcOK2 = rcDraw; rcOK2.left += nW + 4;
                                        SelectObject(hdc, hDimFont);
                                        SetTextColor(hdc, RGB(22, 163, 74));
                                        DrawTextA(hdc, "(OK)", -1, &rcOK2,
                                                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                                    } else {
                                        SetTextColor(hdc, RGB(22, 163, 74));
                                        DrawTextA(hdc, szVal, -1, &rcDraw,
                                                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                                    }
                                } else if (pNone) {

                                    SelectObject(hdc, hDimFont);
                                    SetTextColor(hdc, CLR_TEXT_DIM);
                                    DrawTextA(hdc, szVal, -1, &rcDraw,
                                              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                                } else if (iSplit > 0 && iSplit < nFull - 1) {

                                    if (bBold && hBoldFont) SelectObject(hdc, hBoldFont);
                                    SetTextColor(hdc, clrVal);
                                    DrawTextA(hdc, szVal, iSplit, &rcDraw,
                                              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
                                    int nNumW = rcDraw.right - rcDraw.left;
                                    rcDraw.right = rcCell.right;
                                    DrawTextA(hdc, szVal, iSplit, &rcDraw,
                                              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                                    RECT rcUnit = rcDraw;
                                    rcUnit.left += nNumW + 3;
                                    SelectObject(hdc, hDimFont);
                                    SetTextColor(hdc, clrUnit);
                                    DrawTextA(hdc, szVal + iSplit + 1, -1, &rcUnit,
                                              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                                } else {

                                    if (bBold && hBoldFont) SelectObject(hdc, hBoldFont);
                                    SetTextColor(hdc, clrVal);
                                    DrawTextA(hdc, szVal, -1, &rcDraw,
                                              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                                }
                            }

                            SelectObject(hdc, hCurFont);
                            if (hBoldFont) DeleteObject(hBoldFont);
                            if (hDimFont)  DeleteObject(hDimFont);
                            return CDRF_SKIPDEFAULT;
                        }
                        else if (pCD->iSubItem == 3) {

                            char szStatus[32];
                            LVITEMA lvi;
                            ZeroMemory(&lvi, sizeof(lvi));
                            lvi.mask       = LVIF_TEXT;
                            lvi.iItem      = (int)pCD->nmcd.dwItemSpec;
                            lvi.iSubItem   = 3;
                            lvi.pszText    = szStatus;
                            lvi.cchTextMax = sizeof(szStatus);
                            ListView_GetItem(pCD->nmcd.hdr.hwndFrom, &lvi);

                            COLORREF clrBadgeBg, clrBadgeBorder, clrBadgeText;
                            if (strcmp(szStatus, "FAILED") == 0) {
                                clrBadgeBg     = RGB(220, 38,  38);
                                clrBadgeBorder = RGB(185, 28,  28);
                                clrBadgeText   = RGB(255, 255, 255);
                            } else if (_stricmp(szStatus, "Warning") == 0) {
                                clrBadgeBg     = RGB(217, 119,  6);
                                clrBadgeBorder = RGB(180,  83,  9);
                                clrBadgeText   = RGB(255, 255, 255);
                            } else if (strcmp(szStatus, "OK") == 0) {
                                clrBadgeBg     = RGB(22,  163, 74);
                                clrBadgeBorder = RGB(21,  128, 61);
                                clrBadgeText   = RGB(255, 255, 255);
                            } else if (strcmp(szStatus, "--") == 0) {
                                clrBadgeBg     = RGB(148, 163, 184);
                                clrBadgeBorder = RGB(100, 116, 139);
                                clrBadgeText   = RGB(255, 255, 255);
                            } else {

                                return CDRF_NEWFONT;
                            }

                            HDC hdc = pCD->nmcd.hdc;
                            RECT rcCell = pCD->nmcd.rc;
                            SetBkColor(hdc, clrRowBk);
                            HBRUSH hbrRow = CreateSolidBrush(clrRowBk);
                            FillRect(hdc, &rcCell, hbrRow);
                            DeleteObject(hbrRow);

                            int nLen = lstrlenA(szStatus);
                            SIZE sz;
                            GetTextExtentPoint32A(hdc, szStatus, nLen, &sz);

                            int badgeH  = sz.cy + 6;
                            int badgeW  = sz.cx + 16;
                            int cellCX  = rcCell.right  - rcCell.left;
                            int cellCY  = rcCell.bottom - rcCell.top;
                            int bx      = rcCell.left + (cellCX - badgeW) / 2;
                            int by      = rcCell.top  + (cellCY - badgeH) / 2;
                            RECT rcBadge = { bx, by, bx + badgeW, by + badgeH };

                            HBRUSH hbrBadge  = CreateSolidBrush(clrBadgeBg);
                            HPEN   hpBorder  = CreatePen(PS_SOLID, 1, clrBadgeBorder);
                            HBRUSH hbrOld    = (HBRUSH)SelectObject(hdc, hbrBadge);
                            HPEN   hpOld     = (HPEN)SelectObject(hdc, hpBorder);

                            RoundRect(hdc, rcBadge.left, rcBadge.top,
                                           rcBadge.right, rcBadge.bottom, 8, 8);
                            SelectObject(hdc, hbrOld);
                            SelectObject(hdc, hpOld);
                            DeleteObject(hbrBadge);
                            DeleteObject(hpBorder);

                            SetBkMode(hdc, TRANSPARENT);
                            SetTextColor(hdc, clrBadgeText);
                            DrawTextA(hdc, szStatus, nLen, &rcBadge,
                                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                            return CDRF_SKIPDEFAULT;
                        }
                        else if (pCD->iSubItem == 0) {
                            char szID[16];
                            LVITEMA lvi;
                            ZeroMemory(&lvi, sizeof(lvi));
                            lvi.mask       = LVIF_TEXT;
                            lvi.iItem      = (int)pCD->nmcd.dwItemSpec;
                            lvi.iSubItem   = 0;
                            lvi.pszText    = szID;
                            lvi.cchTextMax = sizeof(szID);
                            ListView_GetItem(pCD->nmcd.hdr.hwndFrom, &lvi);
                            BYTE bID = (BYTE)strtol(szID, NULL, 16);
                            if (IsAttrCritical(bID))
                                pCD->clrText = CLR_ACCENT;
                        }
                        return CDRF_NEWFONT;
                    }

                default:
                    return CDRF_DODEFAULT;
                }
            }
        }
        return 0;

    case WM_TRAYICON:
        {
            int nClickedDrive = (int)(wParam - IDI_TRAY);
            if (nClickedDrive < 0 || nClickedDrive >= MAX_DRIVES)
                nClickedDrive = 0;

            if (lParam == WM_LBUTTONDBLCLK) {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
                g_bMinToTray = FALSE;
                if (nClickedDrive < g_nDriveCount) {
                    g_nSelectedDrive = nClickedDrive;
                    int i;
                    for (i = 0; i < g_nDriveCount; i++)
                        if (g_hDriveBtn[i]) InvalidateRect(g_hDriveBtn[i], NULL, TRUE);
                    UpdateDriveInfo(hWnd, nClickedDrive);
                    UpdateAttrList(hWnd, nClickedDrive);
                    InvalidateRect(hWnd, NULL, FALSE);
                    UpdateWindow(hWnd);
                }
            } else if (lParam == WM_RBUTTONUP) {
                TrayIcon_ShowContextMenu(hWnd);
            }
        }
        return 0;

    case WM_DEVICECHANGE:
        if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) {
            KillTimer(hWnd, IDT_HOTPLUG);
            SetTimer(hWnd, IDT_HOTPLUG, HOTPLUG_DELAY_MS, NULL);
        }
        return TRUE;

    case WM_KEYDOWN:
        if (wParam == 'S' && (GetKeyState(VK_CONTROL) & 0x8000))
            DoSaveScreenshot(hWnd);
        return 0;

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        g_bMinToTray = TRUE;
        return 0;

    case WM_COMMAND:
        {
            int nCtrl = LOWORD(wParam);

            if (nCtrl >= IDC_DRIVE_BTN_BASE && nCtrl < IDC_DRIVE_BTN_BASE + MAX_DRIVES) {
                int nSel = nCtrl - IDC_DRIVE_BTN_BASE;
                if (nSel >= 0 && nSel < g_nDriveCount) {
                    g_nSelectedDrive = nSel;
                    int i;
                    for (i = 0; i < g_nDriveCount; i++)
                        if (g_hDriveBtn[i]) InvalidateRect(g_hDriveBtn[i], NULL, TRUE);
                    UpdateDriveInfo(hWnd, nSel);
                    UpdateAttrList(hWnd, nSel);
                    InvalidateRect(hWnd, NULL, FALSE);
                    UpdateWindow(hWnd);
                }
            }
            else if (nCtrl == IDM_SCREENSHOT) {
                DoSaveScreenshot(hWnd);
            }
            else if (nCtrl == IDM_SHOW_WINDOW) {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
                g_bMinToTray = FALSE;
            }
            else if (nCtrl == IDC_HISTORY_BTN_MAIN || nCtrl == IDM_HISTORY) {
                Graph_ShowWindow(hWnd, g_hInst, g_nSelectedDrive);
            }
            else if (nCtrl == IDM_ABOUT) {
                ShowAboutDialog(hWnd);
            }
            else if (nCtrl == IDM_DONATE) {
                Donate_ShowDialog(hWnd);
            }
            else if (nCtrl == IDM_EXIT) {
                TrayIcon_Remove();
                DestroyWindow(hWnd);
            }
        }
        return 0;

    case WM_APP_REFRESH_DONE:
        InterlockedExchange(&g_bScanBusy, 0);
        {
            UpdateDriveButtons(hWnd);

            if (g_nSelectedDrive >= g_nDriveCount) g_nSelectedDrive = 0;

            UpdateDriveInfo(hWnd, g_nSelectedDrive);
            UpdateAttrList(hWnd, g_nSelectedDrive);
            RepaintHealthBar();
            TrayIcon_Update();
            Graph_Repaint();

            int i;
            for (i = 0; i < g_nDriveCount; i++)
                if (g_hDriveBtn[i]) InvalidateRect(g_hDriveBtn[i], NULL, TRUE);

            InvalidateRect(hWnd, NULL, FALSE);
            UpdateWindow(hWnd);
        }
        return 0;

    case WM_TIMER:
        if (wParam == IDT_REFRESH) {
            RefreshData(hWnd);
        } else if (wParam == IDT_TITLE_UPDATE) {
            /* Keep the title bar text in sync (e.g. on display changes).
               Previously this also ticked the nag timer - that logic has
               been removed because the program is unconditionally free. */
            UpdateWindowTitle(hWnd);
        } else if (wParam == IDT_HOTPLUG) {
            KillTimer(hWnd, IDT_HOTPLUG);
            RefreshData(hWnd);
        }
        return 0;

    case WM_SIZE:
        {
            int cxClient = LOWORD(lParam);
            int cyClient = HIWORD(lParam);
            if (cxClient < 100 || cyClient < 100) break;

            int i;
            int nBtnW = DRIVE_BTN_PANEL_W - 12;
            int nStartY = 40;
            for (i = 0; i < MAX_DRIVES; i++) {
                if (g_hDriveBtn[i]) {
                    int nY = nStartY + i * (DRIVE_BTN_H + DRIVE_BTN_GAP);
                    SetWindowPos(g_hDriveBtn[i], NULL, 6, nY, nBtnW, DRIVE_BTN_H, SWP_NOZORDER);
                }
            }

            int nRightX = DRIVE_BTN_PANEL_W + 10;
            int nBarsW  = 190;
            int nInfoX  = nRightX + nBarsW + 10;
            int nInfoW  = cxClient - nInfoX - 8;
            if (nInfoW < 80) nInfoW = 80;

            int nLblW2  = 90;
            int nValX2  = nInfoX + nLblW2 + 4;
            int nValW2  = cxClient - nValX2 - 8;
            if (nValW2 < 40) nValW2 = 40;
            int nInfoY2 = 36, nInfoH2 = 18, nInfoGap2 = 4;
            { int lblIds[] = { IDC_MODEL_LABEL, IDC_SERIAL_LABEL, IDC_FIRMWARE_LABEL,
                               IDC_SIZE_LABEL, IDC_TEMP_LABEL, IDC_STATUS_LABEL,
                               IDC_READ_SPEED_LABEL };
              int k2;
              for (k2 = 0; k2 < 7; k2++) {
                  HWND hL = GetDlgItem(hWnd, lblIds[k2]);
                  if (hL) SetWindowPos(hL, NULL, nInfoX,
                      nInfoY2 + (nInfoH2 + nInfoGap2) * k2, nLblW2, nInfoH2, SWP_NOZORDER);
              }
            }

            { int valIds[] = { IDC_MODEL_STATIC, IDC_SERIAL_STATIC, IDC_FIRMWARE_STATIC,
                               IDC_SIZE_STATIC, IDC_TEMP_STATIC, IDC_STATUS_STATIC,
                               IDC_READ_SPEED_STATIC };
              int k3;
              for (k3 = 0; k3 < 7; k3++) {
                  HWND hV = GetDlgItem(hWnd, valIds[k3]);
                  if (hV) SetWindowPos(hV, NULL, nValX2,
                      nInfoY2 + (nInfoH2 + nInfoGap2) * k3, nValW2, nInfoH2, SWP_NOZORDER);
              }
            }
            HWND hPred = GetDlgItem(hWnd, IDC_PREDICT_STATIC);
        if (hPred) SetWindowPos(hPred, NULL, nRightX, 218, cxClient - nRightX - 8, 17, SWP_NOZORDER);

            HWND hList = GetDlgItem(hWnd, IDC_ATTR_LIST);
            if (hList) {
                int nListTop = 243;
                int nListH   = cyClient - nListTop - 8;
                if (nListH < 50) nListH = 50;
                SetWindowPos(hList, NULL, nRightX, nListTop,
                             cxClient - nRightX - 8, nListH, SWP_NOZORDER);
            }
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hWnd, IDT_REFRESH);
        KillTimer(hWnd, IDT_TITLE_UPDATE);
        KillTimer(hWnd, IDT_HOTPLUG);
        DeviceNotify_Unregister();
        TrayIcon_Remove();
        History_Save();
        Graph_DestroyAll();
        DestroyGDIObjects();
        if (g_gdiplusToken) { GdiplusShutdown(g_gdiplusToken); g_gdiplusToken = 0; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}
