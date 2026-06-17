/* ============================================================================
 *  HDDHealth Monitor - S.M.A.R.T. history persistence and graphing
 *  ---------------------------------------------------------------------------
 *  100% Free and Open Source Software (FOSS).
 *
 *  Author  : Ari Soandri Putra
 *  Company : ARImetic Inc.
 *  Sponsor : https://github.com/sponsors/arisohandriputra/
 *  License : MIT
 *
 *  This translation unit provides two related services:
 *    1. History_Record / History_Load / History_Save
 *       Persists a rolling buffer of the last HISTORY_MAX_SAMPLES
 *       samples per drive, keyed by drive serial number, into a binary
 *       file stored under %APPDATA%\HDDHealth\history.dat.  The file
 *       format is forward-compatible (magic + version header).
 *    2. Graph_ShowWindow / Graph_Paint
 *       Renders an interactive history graph window with two tabs
 *       (Health % over time, individual S.M.A.R.T. attribute over time).
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "smart_history.h"
#include "mainwnd.h"

const BYTE g_HistoryAttrIDs[HISTORY_TRACKED_ATTRS] = {
    0xC2,
    0x05,
    0xC5,
    0xC6,
    0x09,
    0x0C,
    0x01,
    0xC7
};

const char* const g_HistoryAttrShortNames[HISTORY_TRACKED_ATTRS] = {
    "Temp (\xb0""C)",
    "Realloc'd Sect.",
    "Pending Sect.",
    "Uncorrect. Sect.",
    "Power-On Hrs",
    "Power Cycles",
    "Read Err Rate",
    "CRC Errors"
};

DRIVE_HISTORY g_DriveHistory[MAX_DRIVES];

static HWND   g_hGraphWnd    = NULL;
static int    g_nGraphDrive  = 0;
static HFONT  g_hGraphFontSm = NULL;
static HFONT  g_hGraphFontNm = NULL;

static int    g_nGraphTab    = GRAPH_TAB_HEALTH;
static int    g_nGraphAttr   = 0;

void History_Init(void)
{
    ZeroMemory(g_DriveHistory, sizeof(g_DriveHistory));
}

int History_FindSlot(const char* szSerial)
{
    int i;
    for (i = 0; i < MAX_DRIVES; i++) {
        if (g_DriveHistory[i].szSerial[0] != '\0' &&
            strcmp(g_DriveHistory[i].szSerial, szSerial) == 0)
            return i;
    }
    return -1;
}

static int History_AllocSlot(const char* szSerial)
{

    int i;
    for (i = 0; i < MAX_DRIVES; i++) {
        if (g_DriveHistory[i].szSerial[0] == '\0') {
            lstrcpynA(g_DriveHistory[i].szSerial, szSerial, HISTORY_SERIAL_LEN);
            return i;
        }
    }

    ZeroMemory(&g_DriveHistory[0], sizeof(DRIVE_HISTORY));
    lstrcpynA(g_DriveHistory[0].szSerial, szSerial, HISTORY_SERIAL_LEN);
    return 0;
}

void History_Record(DRIVE_INFO* pInfo, int nDriveCount)
{
    int d;
    for (d = 0; d < nDriveCount; d++) {
        DRIVE_INFO* pi = &pInfo[d];
        if (!pi->bSMART_Supported) continue;

        const char* ser = pi->szSerial;
        if (ser[0] == '\0') continue;

        int slot = History_FindSlot(ser);
        if (slot < 0) slot = History_AllocSlot(ser);

        DRIVE_HISTORY* ph = &g_DriveHistory[slot];

        int idx = ph->nWriteHead;
        HISTORY_SAMPLE* ps = &ph->aSamples[idx];
        ZeroMemory(ps, sizeof(HISTORY_SAMPLE));

        ps->dwTimestamp     = (DWORD)time(NULL);
        ps->nHealthPercent  = pi->nHealthPercent;

        int a;
        for (a = 0; a < HISTORY_TRACKED_ATTRS; a++) {
            BYTE id = g_HistoryAttrIDs[a];
            int j;
            for (j = 0; j < 30; j++) {
                if (pi->attrData.stAttributes[j].bAttrID == id) {
                    ps->dwAttrRaw[a] = (DWORD)(
                        (DWORD)pi->attrData.stAttributes[j].bRawValue[0]        |
                        ((DWORD)pi->attrData.stAttributes[j].bRawValue[1] << 8) |
                        ((DWORD)pi->attrData.stAttributes[j].bRawValue[2] << 16)|
                        ((DWORD)pi->attrData.stAttributes[j].bRawValue[3] << 24)
                    );
                    break;
                }
            }
        }

        ph->nWriteHead = (idx + 1) % HISTORY_MAX_SAMPLES;
        if (ph->nSampleCount < HISTORY_MAX_SAMPLES)
            ph->nSampleCount++;
    }
}

void History_Clear(const char* szSerial)
{
    int slot = History_FindSlot(szSerial);
    if (slot >= 0)
        ZeroMemory(&g_DriveHistory[slot], sizeof(DRIVE_HISTORY));
}

static BOOL GetHistoryPath(char* szOut, int nLen)
{
    char szAppData[MAX_PATH];
    if (!SHGetSpecialFolderPathA(NULL, szAppData, CSIDL_APPDATA, TRUE))
        return FALSE;
    _snprintf(szOut, nLen, "%s\\HDDH", szAppData);
    CreateDirectoryA(szOut, NULL);
    _snprintf(szOut, nLen, "%s\\HDDH\\history.dat", szAppData);
    return TRUE;
}

#pragma pack(push, 1)
typedef struct _HISTORY_FILE_HEADER {
    DWORD   dwMagic;
    DWORD   dwVersion;
    int     nSlots;
} HISTORY_FILE_HEADER;
#pragma pack(pop)

BOOL History_Save(void)
{
    char szPath[MAX_PATH];
    if (!GetHistoryPath(szPath, MAX_PATH)) return FALSE;

    HANDLE hFile = CreateFileA(szPath, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    HISTORY_FILE_HEADER hdr;
    hdr.dwMagic   = HISTORY_FILE_MAGIC;
    hdr.dwVersion = HISTORY_FILE_VERSION;
    hdr.nSlots    = MAX_DRIVES;

    DWORD dw;
    BOOL bOK = TRUE;

    if (!WriteFile(hFile, &hdr, sizeof(hdr), &dw, NULL) || dw != sizeof(hdr))
        bOK = FALSE;

    if (bOK) {
        if (!WriteFile(hFile, g_DriveHistory, sizeof(g_DriveHistory), &dw, NULL)
            || dw != (DWORD)sizeof(g_DriveHistory))
            bOK = FALSE;
    }

    CloseHandle(hFile);

    if (!bOK)
        DeleteFileA(szPath);

    return bOK;
}

BOOL History_Load(void)
{
    char szPath[MAX_PATH];
    if (!GetHistoryPath(szPath, MAX_PATH)) return FALSE;

    HANDLE hFile = CreateFileA(szPath, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    HISTORY_FILE_HEADER hdr;
    DWORD dw;
    if (!ReadFile(hFile, &hdr, sizeof(hdr), &dw, NULL) ||
        hdr.dwMagic != HISTORY_FILE_MAGIC ||
        hdr.dwVersion != HISTORY_FILE_VERSION) {
        CloseHandle(hFile);
        return FALSE;
    }

    if (!ReadFile(hFile, g_DriveHistory, sizeof(g_DriveHistory), &dw, NULL)
        || dw != (DWORD)sizeof(g_DriveHistory)) {
        CloseHandle(hFile);
        ZeroMemory(g_DriveHistory, sizeof(g_DriveHistory));
        return FALSE;
    }
    CloseHandle(hFile);
    return TRUE;
}

static COLORREF s_LineColors[HISTORY_TRACKED_ATTRS + 1] = {
    RGB(30,  100, 210),
    RGB(220,  80,  40),
    RGB( 30, 160,  60),
    RGB(160,  30, 200),
    RGB(200, 160,   0),
    RGB( 30, 190, 190),
    RGB(200,  30,  80),
    RGB(100, 140, 200),
    RGB(140, 200, 100),
};

void Graph_Paint(HDC hdc, const RECT* prc,
                 const DRIVE_HISTORY* pHist,
                 int nAttrIdx,
                 HFONT hFontSmall, HFONT hFontNormal)
{

    HBRUSH hbrBg = CreateSolidBrush(RGB(245, 247, 252));
    FillRect(hdc, prc, hbrBg);
    DeleteObject(hbrBg);

    int W  = prc->right  - prc->left;
    int H  = prc->bottom - prc->top;
    int mL = 54, mR = 16, mT = 24, mB = 36;
    int gW = W - mL - mR;
    int gH = H - mT - mB;
    if (gW < 20 || gH < 20) return;

    HPEN hPenBorder = CreatePen(PS_SOLID, 1, RGB(180, 185, 200));
    HPEN hPenGrid   = CreatePen(PS_DOT,   1, RGB(210, 213, 225));
    HPEN hPenOld    = (HPEN)SelectObject(hdc, hPenBorder);

    MoveToEx(hdc, prc->left + mL,      prc->top  + mT,      NULL);
    LineTo  (hdc, prc->left + mL + gW, prc->top  + mT);
    LineTo  (hdc, prc->left + mL + gW, prc->top  + mT + gH);
    LineTo  (hdc, prc->left + mL,      prc->top  + mT + gH);
    LineTo  (hdc, prc->left + mL,      prc->top  + mT);

    SelectObject(hdc, hPenGrid);
    int gi;
    for (gi = 1; gi < 5; gi++) {
        int y = prc->top + mT + gH * gi / 5;
        MoveToEx(hdc, prc->left + mL,      y, NULL);
        LineTo  (hdc, prc->left + mL + gW, y);
    }

    for (gi = 1; gi < 4; gi++) {
        int x = prc->left + mL + gW * gi / 4;
        MoveToEx(hdc, x, prc->top + mT,      NULL);
        LineTo  (hdc, x, prc->top + mT + gH);
    }

    SelectObject(hdc, hPenOld);
    DeleteObject(hPenBorder);
    DeleteObject(hPenGrid);

    HFONT hOldFont = (HFONT)SelectObject(hdc, hFontSmall);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(100, 105, 120));

    const char* szTitle = (nAttrIdx < 0) ? "Health (%)" :
                          g_HistoryAttrShortNames[nAttrIdx];
    {
        RECT rcTitle = { prc->left, prc->top + 4, prc->right, prc->top + mT };
        SetTextColor(hdc, RGB(30, 35, 50));
        SelectObject(hdc, hFontNormal);
        DrawTextA(hdc, szTitle, -1, &rcTitle, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, hFontSmall);
        SetTextColor(hdc, RGB(100, 105, 120));
    }

    int n = pHist->nSampleCount;
    if (n < 2) {

        RECT rcMsg = { prc->left + mL, prc->top + mT,
                       prc->left + mL + gW, prc->top + mT + gH };
        SetTextColor(hdc, RGB(150, 155, 170));
        DrawTextA(hdc, "Collecting data...\n(refreshes every 5 seconds)", -1,
                  &rcMsg, DT_CENTER | DT_VCENTER);
        SelectObject(hdc, hOldFont);
        return;
    }

    DWORD aVals[HISTORY_MAX_SAMPLES];
    int   k;
    int   startIdx = (pHist->nSampleCount < HISTORY_MAX_SAMPLES) ?
                     0 :
                     pHist->nWriteHead;

    DWORD valMin = 0xFFFFFFFF, valMax = 0;
    for (k = 0; k < n; k++) {
        int si = (startIdx + k) % HISTORY_MAX_SAMPLES;
        const HISTORY_SAMPLE* ps = &pHist->aSamples[si];
        DWORD v;
        if (nAttrIdx < 0)
            v = (DWORD)(ps->nHealthPercent < 0 ? 0 : ps->nHealthPercent);
        else
            v = ps->dwAttrRaw[nAttrIdx];
        aVals[k] = v;
        if (v < valMin) valMin = v;
        if (v > valMax) valMax = v;
    }

    if (valMax == valMin) {
        if (nAttrIdx < 0) { valMin = 0; valMax = 100; }
        else { valMin = (valMin > 0) ? valMin - 1 : 0; valMax = valMin + 10; }
    }

    if (nAttrIdx < 0) { valMin = 0; valMax = 100; }

    char szNum[32];
    for (gi = 0; gi <= 5; gi++) {
        DWORD v = valMin + (DWORD)((double)(valMax - valMin) * (5 - gi) / 5.0);
        _snprintf(szNum, sizeof(szNum), "%lu", (unsigned long)v);
        int y = prc->top + mT + gH * gi / 5 - 6;
        RECT rcLbl = { prc->left, y, prc->left + mL - 4, y + 14 };
        DrawTextA(hdc, szNum, -1, &rcLbl, DT_RIGHT | DT_SINGLELINE);
    }

    {
        char szX0[8] = "-N";
        _snprintf(szX0, sizeof(szX0), "-%d", n);
        int yLbl = prc->top + mT + gH + 2;
        RECT rc0 = { prc->left + mL - 10, yLbl, prc->left + mL + 30, yLbl + 14 };
        DrawTextA(hdc, szX0, -1, &rc0, DT_LEFT | DT_SINGLELINE);
        RECT rc1 = { prc->left + mL + gW - 20, yLbl, prc->left + mL + gW + 20, yLbl + 14 };
        DrawTextA(hdc, "now", -1, &rc1, DT_CENTER | DT_SINGLELINE);
    }

    SelectObject(hdc, hOldFont);

    COLORREF clrLine = (nAttrIdx < 0) ? s_LineColors[0] : s_LineColors[1 + nAttrIdx];
    HPEN hPenLine = CreatePen(PS_SOLID, 2, clrLine);
    HPEN hPenDot  = CreatePen(PS_SOLID, 1, clrLine);
    SelectObject(hdc, hPenLine);

    BOOL bFirst = TRUE;
    for (k = 0; k < n; k++) {
        int x = prc->left + mL + (int)((double)gW * k / (n - 1));
        double frac = (valMax > valMin) ?
                      (double)(aVals[k] - valMin) / (double)(valMax - valMin) : 0.5;
        if (frac < 0.0) frac = 0.0;
        if (frac > 1.0) frac = 1.0;
        int y = prc->top + mT + gH - (int)(frac * gH);

        if (bFirst) {
            MoveToEx(hdc, x, y, NULL);
            bFirst = FALSE;
        } else {
            LineTo(hdc, x, y);
        }

        SelectObject(hdc, hPenDot);
        Ellipse(hdc, x - 3, y - 3, x + 3, y + 3);
        SelectObject(hdc, hPenLine);
    }

    SelectObject(hdc, hPenOld);
    DeleteObject(hPenLine);
    DeleteObject(hPenDot);

    if (n > 0) {
        char szCur[48];
        if (nAttrIdx < 0)
            _snprintf(szCur, sizeof(szCur), "Current: %d%%", pHist->aSamples[(pHist->nWriteHead + HISTORY_MAX_SAMPLES - 1) % HISTORY_MAX_SAMPLES].nHealthPercent);
        else
            _snprintf(szCur, sizeof(szCur), "Current: %lu",
                      (unsigned long)pHist->aSamples[(pHist->nWriteHead + HISTORY_MAX_SAMPLES - 1) % HISTORY_MAX_SAMPLES].dwAttrRaw[nAttrIdx]);

        SelectObject(hdc, hFontSmall);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, clrLine);
        RECT rcCur = { prc->left + mL, prc->top + mT - 18,
                       prc->left + mL + gW, prc->top + mT };
        DrawTextA(hdc, szCur, -1, &rcCur, DT_RIGHT | DT_SINGLELINE);
        SelectObject(hdc, hOldFont);
    }
}

static LRESULT CALLBACK GraphWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT rc;
            GetClientRect(hWnd, &rc);

            int nTabH = 28;

            HBRUSH hbrBg = CreateSolidBrush(RGB(240, 242, 247));
            FillRect(hdc, &rc, hbrBg);
            DeleteObject(hbrBg);

            const char* szTabs[] = { "Health %", "Attribute" };
            int t;
            for (t = 0; t < 2; t++) {
                int tx = rc.left + t * (rc.right - rc.left) / 2;
                int tw = (rc.right - rc.left) / 2;
                RECT rcTab = { tx, rc.top, tx + tw, rc.top + nTabH };

                COLORREF clrTab = (t == g_nGraphTab) ?
                                  RGB(255,255,255) : RGB(220,223,235);
                HBRUSH hbrTab = CreateSolidBrush(clrTab);
                FillRect(hdc, &rcTab, hbrTab);
                DeleteObject(hbrTab);

                if (t != g_nGraphTab) {
                    HPEN hpen = CreatePen(PS_SOLID, 1, RGB(180,185,200));
                    HPEN hold = (HPEN)SelectObject(hdc, hpen);
                    MoveToEx(hdc, rcTab.left,     rcTab.bottom - 1, NULL);
                    LineTo  (hdc, rcTab.right - 1, rcTab.bottom - 1);
                    SelectObject(hdc, hold);
                    DeleteObject(hpen);
                }

                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, (t == g_nGraphTab) ? RGB(30,35,50) : RGB(100,105,120));
                SelectObject(hdc, g_hGraphFontNm);
                DrawTextA(hdc, szTabs[t], -1, &rcTab,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            {
                HPEN hpen = CreatePen(PS_SOLID, 1, RGB(180,185,200));
                HPEN hold = (HPEN)SelectObject(hdc, hpen);
                MoveToEx(hdc, rc.left,  rc.top + nTabH, NULL);
                LineTo  (hdc, rc.right, rc.top + nTabH);
                SelectObject(hdc, hold);
                DeleteObject(hpen);
            }

            int nDriveSlot = -1;
            if (g_nGraphDrive >= 0 && g_nGraphDrive < g_nDriveCount)
                nDriveSlot = History_FindSlot(g_Drives[g_nGraphDrive].szSerial);

            int nCtrlH = 0;
            if (g_nGraphTab == GRAPH_TAB_ATTR) {
                nCtrlH = 30;
            }

            RECT rcGraph = { rc.left + 8,
                             rc.top + nTabH + nCtrlH + 8,
                             rc.right - 8,
                             rc.bottom - 8 };

            if (nDriveSlot >= 0) {
                int nAttr = (g_nGraphTab == GRAPH_TAB_HEALTH) ? -1 : g_nGraphAttr;
                Graph_Paint(hdc, &rcGraph,
                            &g_DriveHistory[nDriveSlot],
                            nAttr,
                            g_hGraphFontSm, g_hGraphFontNm);
            } else {
                HBRUSH hbrG = CreateSolidBrush(RGB(245,247,252));
                FillRect(hdc, &rcGraph, hbrG);
                DeleteObject(hbrG);
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(150,155,170));
                SelectObject(hdc, g_hGraphFontNm);
                DrawTextA(hdc, "No history data available yet.\n"
                               "Data is recorded every 5 seconds.",
                          -1, &rcGraph, DT_CENTER | DT_VCENTER);
            }

            EndPaint(hWnd, &ps);
        }
        return 0;

    case WM_LBUTTONDOWN:
        {

            RECT rc;
            GetClientRect(hWnd, &rc);
            int nTabH = 28;
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);
            if (my < nTabH) {
                int newTab = (mx < (rc.right - rc.left) / 2) ?
                             GRAPH_TAB_HEALTH : GRAPH_TAB_ATTR;
                if (newTab != g_nGraphTab) {
                    g_nGraphTab = newTab;
                    InvalidateRect(hWnd, NULL, TRUE);

                    HWND hCombo = GetDlgItem(hWnd, IDC_GRAPH_COMBO_ATTR);
                    if (hCombo)
                        ShowWindow(hCombo, (g_nGraphTab == GRAPH_TAB_ATTR) ?
                                   SW_SHOW : SW_HIDE);
                }
            }
        }
        return 0;

    case WM_COMMAND:
        {
            int id  = LOWORD(wParam);
            int evt = HIWORD(wParam);

            if (id == IDC_GRAPH_COMBO_ATTR && evt == CBN_SELCHANGE) {
                HWND hCombo = GetDlgItem(hWnd, IDC_GRAPH_COMBO_ATTR);
                int sel = (int)SendMessageA(hCombo, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < HISTORY_TRACKED_ATTRS) {
                    g_nGraphAttr = sel;
                    InvalidateRect(hWnd, NULL, TRUE);
                }
            }
            if (id == IDC_GRAPH_BTN_CLEAR) {
                if (g_nGraphDrive >= 0 && g_nGraphDrive < g_nDriveCount) {
                    if (MessageBoxA(hWnd,
                        "Clear all recorded history for this drive?",
                        "HDDHealth Monitor", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                        History_Clear(g_Drives[g_nGraphDrive].szSerial);
                        InvalidateRect(hWnd, NULL, TRUE);
                    }
                }
            }
        }
        return 0;

    case WM_SIZE:
        {

            HWND hCombo = GetDlgItem(hWnd, IDC_GRAPH_COMBO_ATTR);
            HWND hClear = GetDlgItem(hWnd, IDC_GRAPH_BTN_CLEAR);
            int W2 = LOWORD(lParam);
            if (hCombo)
                SetWindowPos(hCombo, NULL, 8, 32, W2 - 100, 22, SWP_NOZORDER);
            if (hClear)
                SetWindowPos(hClear, NULL, W2 - 88, 32, 80, 22, SWP_NOZORDER);
        }
        return 0;

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        g_hGraphWnd = NULL;
        if (g_hGraphFontSm) { DeleteObject(g_hGraphFontSm); g_hGraphFontSm = NULL; }
        if (g_hGraphFontNm) { DeleteObject(g_hGraphFontNm); g_hGraphFontNm = NULL; }
        return 0;
    }
    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

void Graph_RegisterClass(HINSTANCE hInst)
{
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = GraphWndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = LLHD_GRAPH_CLASS;
    RegisterClassA(&wc);
}

void Graph_ShowWindow(HWND hParent, HINSTANCE hInst, int nDriveIdx)
{
    g_nGraphDrive = nDriveIdx;

    if (!g_hGraphWnd) {

        g_hGraphFontSm = CreateFontA(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                     "Segoe UI");
        g_hGraphFontNm = CreateFontA(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                     "Segoe UI");

        RECT rcMain;
        GetWindowRect(hParent, &rcMain);
        int gx = rcMain.right + 8;
        int gy = rcMain.top;
        int gw = 520, gh = 370;

        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        if (gx + gw > sw) gx = rcMain.left - gw - 8;
        if (gx < 0) gx = 80;
        if (gy + gh > sh) gy = sh - gh - 40;
        if (gy < 0) gy = 40;

        g_hGraphWnd = CreateWindowExA(
            WS_EX_TOOLWINDOW,
            LLHD_GRAPH_CLASS,
            "S.M.A.R.T. History",
            WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
            gx, gy, gw, gh,
            NULL, NULL, hInst, NULL
        );
        if (!g_hGraphWnd) return;

        HWND hCombo = CreateWindowExA(
            0, "COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            8, 32, 400, 200,
            g_hGraphWnd, (HMENU)IDC_GRAPH_COMBO_ATTR, hInst, NULL
        );
        if (hCombo) {
            SendMessageA(hCombo, WM_SETFONT, (WPARAM)g_hGraphFontSm, TRUE);
            int a;
            for (a = 0; a < HISTORY_TRACKED_ATTRS; a++)
                SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)g_HistoryAttrShortNames[a]);
            SendMessageA(hCombo, CB_SETCURSEL, g_nGraphAttr, 0);
            ShowWindow(hCombo, (g_nGraphTab == GRAPH_TAB_ATTR) ? SW_SHOW : SW_HIDE);
        }

        HWND hClear = CreateWindowExA(
            0, "BUTTON", "Clear History",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            412, 32, 90, 22,
            g_hGraphWnd, (HMENU)IDC_GRAPH_BTN_CLEAR, hInst, NULL
        );
        if (hClear)
            SendMessageA(hClear, WM_SETFONT, (WPARAM)g_hGraphFontSm, TRUE);

        if (nDriveIdx >= 0 && nDriveIdx < g_nDriveCount) {
            char szTitle[128];
            _snprintf(szTitle, sizeof(szTitle),
                      "S.M.A.R.T. History > %s", g_Drives[nDriveIdx].szModel);
            SetWindowTextA(g_hGraphWnd, szTitle);
        }
    } else {

        if (nDriveIdx >= 0 && nDriveIdx < g_nDriveCount) {
            char szTitle[128];
            _snprintf(szTitle, sizeof(szTitle),
                      "S.M.A.R.T. History > %s", g_Drives[nDriveIdx].szModel);
            SetWindowTextA(g_hGraphWnd, szTitle);
        }
        InvalidateRect(g_hGraphWnd, NULL, TRUE);
    }

    ShowWindow(g_hGraphWnd, SW_SHOWNORMAL);
    SetForegroundWindow(g_hGraphWnd);
}

void Graph_Repaint(void)
{
    if (g_hGraphWnd && IsWindowVisible(g_hGraphWnd))
        InvalidateRect(g_hGraphWnd, NULL, FALSE);
}

void Graph_DestroyAll(void)
{
    if (g_hGraphWnd) {
        DestroyWindow(g_hGraphWnd);
        g_hGraphWnd = NULL;
    }
}
