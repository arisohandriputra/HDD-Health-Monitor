/* ============================================================================
 *  HDDHealth Monitor 1.3 - Drive database implementation
 *
 *  Author  : Ari Sohandri Putra
 *  Sponsor : https://github.com/sponsors/arisohandriputra/
 *  License : MIT
 *
 *  Minimal JSON parser + lookup + display dialog.
 *  The parser is intentionally simple — it only handles our specific
 *  JSON structure (flat array of objects with string values).
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
#include <ctype.h>

#include "drive_db.h"
#include "mainwnd.h"   /* g_hInst, g_Drives, g_nSelectedDrive */
#include "smart.h"
#include "resource.h"  /* IDR_DRIVES_JSON */

/* ------------------------------------------------------------------ */
/*  Load the embedded JSON from the .exe's RT_RCDATA resource          */
/*  Returns malloc'd buffer (caller must free) or NULL on failure.      */
/*  The database ships inside the .exe — no external file needed.      */
/* ------------------------------------------------------------------ */
static char* LoadEmbeddedJson(int* pnSize)
{
    HRSRC hRes = FindResourceA(NULL, MAKEINTRESOURCEA(IDR_DRIVES_JSON), RT_RCDATA);
    if (!hRes) return NULL;

    HGLOBAL hMem = LoadResource(NULL, hRes);
    if (!hMem) return NULL;

    DWORD dwSize = SizeofResource(NULL, hRes);
    if (dwSize == 0) return NULL;

    void* pData = LockResource(hMem);
    if (!pData) return NULL;

    /* Allocate +1 for null terminator */
    char* pBuf = (char*)malloc(dwSize + 1);
    if (!pBuf) return NULL;

    memcpy(pBuf, pData, dwSize);
    pBuf[dwSize] = '\0';

    if (pnSize) *pnSize = (int)dwSize;
    return pBuf;
}

/* ------------------------------------------------------------------ */
/*  Minimal JSON parser                                                */
/*  Handles: { "key": "value", ... } and arrays of objects             */
/*  Does NOT handle: nested objects, escaped quotes in values,         */
/*  numbers, booleans, null. Values are always treated as strings.     */
/* ------------------------------------------------------------------ */

typedef struct _JSON_CTX {
    const char* p;       /* current parse position */
    const char* end;     /* end of buffer */
} JSON_CTX;

static void JSON_SkipWs(JSON_CTX* ctx)
{
    while (ctx->p < ctx->end) {
        char c = *ctx->p;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            ctx->p++;
        else
            break;
    }
}

/* Read a quoted string into szOut. Handles \" escape (needed for
   values like "2.5\"" which contains an escaped quote). */
static BOOL JSON_ReadString(JSON_CTX* ctx, char* szOut, int nOutLen)
{
    JSON_SkipWs(ctx);
    if (ctx->p >= ctx->end || *ctx->p != '"') return FALSE;
    ctx->p++; /* skip opening quote */
    int i = 0;
    while (ctx->p < ctx->end) {
        /* Handle escape sequences — only \" is common in our data */
        if (*ctx->p == '\\' && ctx->p + 1 < ctx->end) {
            ctx->p++; /* skip backslash */
            if (*ctx->p == '"') {
                /* Escaped quote — output literal " */
                if (i < nOutLen - 1) szOut[i++] = '"';
                ctx->p++;
                continue;
            } else if (*ctx->p == '\\') {
                if (i < nOutLen - 1) szOut[i++] = '\\';
                ctx->p++;
                continue;
            } else if (*ctx->p == '/') {
                if (i < nOutLen - 1) szOut[i++] = '/';
                ctx->p++;
                continue;
            }
            /* Other escapes: just copy the char after backslash */
            if (i < nOutLen - 1) szOut[i++] = *ctx->p;
            ctx->p++;
            continue;
        }
        /* Unescaped closing quote — end of string */
        if (*ctx->p == '"') break;
        if (i < nOutLen - 1) szOut[i++] = *ctx->p;
        ctx->p++;
    }
    szOut[i] = '\0';
    if (ctx->p < ctx->end && *ctx->p == '"') ctx->p++; /* skip closing quote */
    return TRUE;
}

/* Read a value as a string (strips quotes if present). */
static BOOL JSON_ReadValue(JSON_CTX* ctx, char* szOut, int nOutLen)
{
    JSON_SkipWs(ctx);
    if (ctx->p >= ctx->end) return FALSE;

    /* String value? */
    if (*ctx->p == '"') {
        return JSON_ReadString(ctx, szOut, nOutLen);
    }

    /* Otherwise read until , or } — captures numbers, true, false, null */
    int i = 0;
    while (ctx->p < ctx->end && *ctx->p != ',' && *ctx->p != '}' &&
           *ctx->p != '\r' && *ctx->p != '\n') {
        if (i < nOutLen - 1) szOut[i++] = *ctx->p;
        ctx->p++;
    }
    /* Trim trailing whitespace */
    while (i > 0 && (szOut[i-1] == ' ' || szOut[i-1] == '\t')) i--;
    szOut[i] = '\0';
    return i > 0;
}

/* Skip past a value (string, number, or object/array — we only care
 * about flat string values so this just skips to the next , or }) */
static void JSON_SkipValue(JSON_CTX* ctx)
{
    JSON_SkipWs(ctx);
    if (ctx->p >= ctx->end) return;
    if (*ctx->p == '"') {
        char szTmp[256];
        JSON_ReadString(ctx, szTmp, sizeof(szTmp));
    } else {
        char szTmp[256];
        JSON_ReadValue(ctx, szTmp, sizeof(szTmp));
    }
}

/* ------------------------------------------------------------------ */
/*  Field assignment by key name                                       */
/* ------------------------------------------------------------------ */

static void SetField(const char* szKey, const char* szVal, DRIVE_DB_ENTRY* pE)
{
    if      (!strcmp(szKey, "model"))                  lstrcpynA(pE->szModel, szVal, sizeof(pE->szModel));
    else if (!strcmp(szKey, "firmware"))               lstrcpynA(pE->szFirmware, szVal, sizeof(pE->szFirmware));
    else if (!strcmp(szKey, "trademark"))              lstrcpynA(pE->szTrademark, szVal, sizeof(pE->szTrademark));
    else if (!strcmp(szKey, "capacity"))               lstrcpynA(pE->szCapacity, szVal, sizeof(pE->szCapacity));
    else if (!strcmp(szKey, "form_factor"))            lstrcpynA(pE->szFormFactor, szVal, sizeof(pE->szFormFactor));
    else if (!strcmp(szKey, "type"))                   lstrcpynA(pE->szType, szVal, sizeof(pE->szType));
    else if (!strcmp(szKey, "interface"))              lstrcpynA(pE->szInterface, szVal, sizeof(pE->szInterface));
    else if (!strcmp(szKey, "sata_version"))           lstrcpynA(pE->szSataVersion, szVal, sizeof(pE->szSataVersion));
    else if (!strcmp(szKey, "controller"))             lstrcpynA(pE->szController, szVal, sizeof(pE->szController));
    else if (!strcmp(szKey, "alt_firmwares"))          lstrcpynA(pE->szAltFirmwares, szVal, sizeof(pE->szAltFirmwares));
    else if (!strcmp(szKey, "firmware_upgrade"))       lstrcpynA(pE->szFirmwareUpgrade, szVal, sizeof(pE->szFirmwareUpgrade));
    else if (!strcmp(szKey, "ata_version"))            lstrcpynA(pE->szAtaVersion, szVal, sizeof(pE->szAtaVersion));
    else if (!strcmp(szKey, "advanced_format"))        lstrcpynA(pE->szAdvancedFormat, szVal, sizeof(pE->szAdvancedFormat));
    else if (!strcmp(szKey, "units_tested"))            lstrcpynA(pE->szUnitsTested, szVal, sizeof(pE->szUnitsTested));
    else if (!strcmp(szKey, "times_tested"))            lstrcpynA(pE->szTimesTested, szVal, sizeof(pE->szTimesTested));
    else if (!strcmp(szKey, "max_interface_speed"))    lstrcpynA(pE->szMaxInterfaceSpeed, szVal, sizeof(pE->szMaxInterfaceSpeed));
    else if (!strcmp(szKey, "max_buffered_read"))      lstrcpynA(pE->szMaxBufferedRead, szVal, sizeof(pE->szMaxBufferedRead));
    else if (!strcmp(szKey, "max_read_speed"))         lstrcpynA(pE->szMaxReadSpeed, szVal, sizeof(pE->szMaxReadSpeed));
    else if (!strcmp(szKey, "min_access_time"))        lstrcpynA(pE->szMinAccessTime, szVal, sizeof(pE->szMinAccessTime));
    else if (!strcmp(szKey, "cache_buffer"))            lstrcpynA(pE->szCacheBuffer, szVal, sizeof(pE->szCacheBuffer));
    else if (!strcmp(szKey, "volatile_write_cache"))   lstrcpynA(pE->szVolatileWriteCache, szVal, sizeof(pE->szVolatileWriteCache));
    else if (!strcmp(szKey, "read_look_ahead"))        lstrcpynA(pE->szReadLookAhead, szVal, sizeof(pE->szReadLookAhead));
    else if (!strcmp(szKey, "ncq"))                     lstrcpynA(pE->szNcq, szVal, sizeof(pE->szNcq));
    else if (!strcmp(szKey, "apm"))                     lstrcpynA(pE->szApm, szVal, sizeof(pE->szApm));
    else if (!strcmp(szKey, "aam"))                     lstrcpynA(pE->szAam, szVal, sizeof(pE->szAam));
    else if (!strcmp(szKey, "trim"))                    lstrcpynA(pE->szTrim, szVal, sizeof(pE->szTrim));
    else if (!strcmp(szKey, "rotation_rate"))          lstrcpynA(pE->szRotationRate, szVal, sizeof(pE->szRotationRate));
    else if (!strcmp(szKey, "sensor_count"))            lstrcpynA(pE->szSensorCount, szVal, sizeof(pE->szSensorCount));
    else if (!strcmp(szKey, "free_fall_control"))      lstrcpynA(pE->szFreeFallControl, szVal, sizeof(pE->szFreeFallControl));
    else if (!strcmp(szKey, "sct"))                     lstrcpynA(pE->szSct, szVal, sizeof(pE->szSct));
    else if (!strcmp(szKey, "full_selftest_time"))     lstrcpynA(pE->szFullSelftestTime, szVal, sizeof(pE->szFullSelftestTime));
    else if (!strcmp(szKey, "crit_max_temp"))          lstrcpynA(pE->szCritMaxTemp, szVal, sizeof(pE->szCritMaxTemp));
    else if (!strcmp(szKey, "rec_max_temp"))           lstrcpynA(pE->szRecMaxTemp, szVal, sizeof(pE->szRecMaxTemp));
    else if (!strcmp(szKey, "max_temp"))                lstrcpynA(pE->szMaxTemp, szVal, sizeof(pE->szMaxTemp));
    else if (!strcmp(szKey, "avg_max_temp"))            lstrcpynA(pE->szAvgMaxTemp, szVal, sizeof(pE->szAvgMaxTemp));
    else if (!strcmp(szKey, "drive_access_restriction")) lstrcpynA(pE->szDriveAccessRestriction, szVal, sizeof(pE->szDriveAccessRestriction));
    else if (!strcmp(szKey, "enhanced_security_erase")) lstrcpynA(pE->szEnhancedSecurityErase, szVal, sizeof(pE->szEnhancedSecurityErase));
    else if (!strcmp(szKey, "data_destruction_time")) lstrcpynA(pE->szDataDestructionTime, szVal, sizeof(pE->szDataDestructionTime));
    else if (!strcmp(szKey, "instant_data_destruction")) lstrcpynA(pE->szInstantDataDestruction, szVal, sizeof(pE->szInstantDataDestruction));
    else if (!strcmp(szKey, "hpa"))                     lstrcpynA(pE->szHpa, szVal, sizeof(pE->szHpa));
    else if (!strcmp(szKey, "hpa_password"))            lstrcpynA(pE->szHpaPassword, szVal, sizeof(pE->szHpaPassword));
    else if (!strcmp(szKey, "trusted_computing"))      lstrcpynA(pE->szTrustedComputing, szVal, sizeof(pE->szTrustedComputing));
}

/* ------------------------------------------------------------------ */
/*  Lookup                                                             */
/* ------------------------------------------------------------------ */

BOOL DriveDB_Lookup(const char* szModel, const char* szFirmware,
                    DRIVE_DB_ENTRY* pEntry)
{
    if (!pEntry) return FALSE;
    ZeroMemory(pEntry, sizeof(*pEntry));
    pEntry->bFound = FALSE;

    if (!szModel || !szModel[0]) return FALSE;

    /* Load the embedded JSON from the .exe's RT_RCDATA resource */
    int nSize = 0;
    char* pBuf = LoadEmbeddedJson(&nSize);
    if (!pBuf || nSize == 0) {
        if (pBuf) free(pBuf);
        return FALSE;
    }

    /* Parse JSON — look for objects with matching "model" field */
    JSON_CTX ctx = { pBuf, pBuf + nSize };
    BOOL bMatch = FALSE;

    while (ctx.p < ctx.end) {
        JSON_SkipWs(&ctx);
        if (ctx.p >= ctx.end) break;

        /* Expect { to start an object */
        if (*ctx.p != '{') { ctx.p++; continue; }
        ctx.p++; /* skip { */

        /* Parse fields of this object */
        DRIVE_DB_ENTRY entry;
        ZeroMemory(&entry, sizeof(entry));

        while (ctx.p < ctx.end) {
            JSON_SkipWs(&ctx);
            if (ctx.p >= ctx.end) break;
            if (*ctx.p == '}') { ctx.p++; break; }
            if (*ctx.p == ',') { ctx.p++; continue; }

            /* Read key */
            char szKey[64];
            if (!JSON_ReadString(&ctx, szKey, sizeof(szKey))) break;

            JSON_SkipWs(&ctx);
            if (ctx.p >= ctx.end || *ctx.p != ':') break;
            ctx.p++; /* skip : */

            /* Read value — don't break on empty string, just skip it */
            char szVal[128];
            szVal[0] = '\0';
            JSON_ReadValue(&ctx, szVal, sizeof(szVal));

            SetField(szKey, szVal, &entry);
        }

        /* Check if this entry matches the requested model */
        if (entry.szModel[0]) {
            /* Case-insensitive substring match — the JSON model might be
             * a prefix (e.g. "ST500LT012" matches "ST500LT012-1DG142") */
            char szUpperModel[128], szUpperEntry[128];
            int i;
            for (i = 0; entry.szModel[i] && i < 127; i++)
                szUpperEntry[i] = (char)toupper((unsigned char)entry.szModel[i]);
            szUpperEntry[i] = '\0';
            for (i = 0; szModel[i] && i < 127; i++)
                szUpperModel[i] = (char)toupper((unsigned char)szModel[i]);
            szUpperModel[i] = '\0';

            if (strstr(szUpperModel, szUpperEntry)) {
                /* Model matches. If firmware specified in JSON, check it too. */
                if (entry.szFirmware[0] && szFirmware && szFirmware[0]) {
                    if (_stricmp(entry.szFirmware, szFirmware) == 0) {
                        *pEntry = entry;
                        pEntry->bFound = TRUE;
                        bMatch = TRUE;
                        break;
                    }
                    /* Firmware doesn't match — keep looking */
                } else {
                    /* No firmware constraint — match on model alone */
                    *pEntry = entry;
                    pEntry->bFound = TRUE;
                    bMatch = TRUE;
                    break;
                }
            }
        }
    }

    free(pBuf);
    return bMatch;
}

/* ------------------------------------------------------------------ */
/*  Display dialog                                                     */
/* ------------------------------------------------------------------ */

/* Row helper: creates a label (right-aligned) + value (left-aligned) */
static HWND AddRow(HWND hDlg, int x, int y, int lblW, int valW,
                   const char* szLabel, const char* szValue, HFONT hFont,
                   int nIdStart)
{
    HWND hLbl = CreateWindowExA(0, "STATIC", szLabel,
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        x, y, lblW, 18, hDlg, (HMENU)(nIdStart), g_hInst, NULL);
    SendMessageA(hLbl, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hVal = CreateWindowExA(0, "STATIC", szValue[0] ? szValue : "-",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x + lblW + 8, y, valW, 18, hDlg, (HMENU)(nIdStart + 1), g_hInst, NULL);
    SendMessageA(hVal, WM_SETFONT, (WPARAM)hFont, TRUE);

    return hVal;
}

/* Group box helper */
static HWND AddGroup(HWND hDlg, int x, int y, int w, int h,
                     const char* szTitle, HFONT hFont, int nId)
{
    HWND hGrp = CreateWindowExA(0, "BUTTON", szTitle,
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, y, w, h, hDlg, (HMENU)nId, g_hInst, NULL);
    SendMessageA(hGrp, WM_SETFONT, (WPARAM)hFont, TRUE);
    return hGrp;
}

static LRESULT CALLBACK DriveDBDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        {
            /* Look up the currently selected drive */
            DRIVE_DB_ENTRY entry;
            ZeroMemory(&entry, sizeof(entry));

            const char* szModel = "-";
            const char* szFirmware = "";
            if (g_nSelectedDrive >= 0 && g_nSelectedDrive < g_nDriveCount) {
                szModel = g_Drives[g_nSelectedDrive].szModel;
                szFirmware = g_Drives[g_nSelectedDrive].szFirmware;
            }

            BOOL bFound = DriveDB_Lookup(szModel, szFirmware, &entry);

            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            int cx = 500, cy = 420;

            if (!bFound) {
                HWND hMsg = CreateWindowExA(0, "STATIC",
                    "This drive is not in the database.\n\n"
                    "The database is maintained by the developer and embedded\n"
                    "in the .exe. To add this drive, the developer needs to\n"
                    "add an entry to drives.json and rebuild.",
                    WS_CHILD | WS_VISIBLE,
                    16, 50, cx - 32, 80, hDlg, NULL, g_hInst, NULL);
                SendMessageA(hMsg, WM_SETFONT, (WPARAM)hFont, TRUE);
            } else {
                int y = 16;
                int lblW = 160, valW = 280;
                int x = 16;
                int nId = 1000;

                /* --- General information ---
                 * 13 rows: trademark, model, capacity, form-factor, type,
                 * interface, sata version, controller, firmware, firmware
                 * upgrade, ata version, advanced format, rotation rate. */
                AddGroup(hDlg, x, y, cx - 32, 22 + 13 * 22 + 12, "General information", hFont, nId++);
                y += 22;
                AddRow(hDlg, x + 12, y, lblW, valW, "Trademark",       entry.szTrademark,      hFont, nId); nId += 2; y += 22;
                AddRow(hDlg, x + 12, y, lblW, valW, "Model",            entry.szModel,          hFont, nId); nId += 2; y += 22;
                AddRow(hDlg, x + 12, y, lblW, valW, "Capacity",        entry.szCapacity,       hFont, nId); nId += 2; y += 22;
                AddRow(hDlg, x + 12, y, lblW, valW, "Form-factor",     entry.szFormFactor,     hFont, nId); nId += 2; y += 22;
                AddRow(hDlg, x + 12, y, lblW, valW, "Type",            entry.szType,           hFont, nId); nId += 2; y += 22;
                AddRow(hDlg, x + 12, y, lblW, valW, "Interface",       entry.szInterface,      hFont, nId); nId += 2; y += 22;
                AddRow(hDlg, x + 12, y, lblW, valW, "SATA version",    entry.szSataVersion,    hFont, nId); nId += 2; y += 22;
                AddRow(hDlg, x + 12, y, lblW, valW, "Controller",      entry.szController,     hFont, nId); nId += 2; y += 22;
                AddRow(hDlg, x + 12, y, lblW, valW, "Firmware",        entry.szFirmware[0] ? entry.szFirmware : szFirmware, hFont, nId); nId += 2; y += 22;
                AddRow(hDlg, x + 12, y, lblW, valW, "Firmware upgrade",entry.szFirmwareUpgrade,hFont, nId); nId += 2; y += 22;
                AddRow(hDlg, x + 12, y, lblW, valW, "ATA version",     entry.szAtaVersion,     hFont, nId); nId += 2; y += 22;
                AddRow(hDlg, x + 12, y, lblW, valW, "Advanced Format", entry.szAdvancedFormat, hFont, nId); nId += 2; y += 22;
                AddRow(hDlg, x + 12, y, lblW, valW, "Rotation rate",   entry.szRotationRate,   hFont, nId); nId += 2; y += 22;
            }

            /* Close button */
            HWND hBtn = CreateWindowExA(0, "BUTTON", "Close",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_DEFPUSHBUTTON,
                (cx - 100) / 2, cy - 40, 100, 28,
                hDlg, (HMENU)9, g_hInst, NULL);
            SendMessageA(hBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
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

void DriveDB_ShowDialog(HWND hWnd)
{
    static BOOL bReg = FALSE;
    if (!bReg) {
        WNDCLASSEXA wc; ZeroMemory(&wc, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = DriveDBDlgProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "HDDHDriveDBDlg";
        RegisterClassExA(&wc);
        bReg = TRUE;
    }

    /* Dialog size — fits the 13-row General information section + Close button */
    HWND hDlg = CreateWindowExA(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "HDDHDriveDBDlg", "Drive Info",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        0, 0, 520, 440, hWnd, NULL, g_hInst, NULL);
    if (!hDlg) return;

    /* Center on parent */
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
