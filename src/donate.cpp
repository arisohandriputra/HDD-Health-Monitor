/* ============================================================================
 *  HDDHealth Monitor - Donate module implementation
 *  ---------------------------------------------------------------------------
 *  100% Free and Open Source Software (FOSS).
 *
 *  Author  : Ari Soandri Putra
 *  Company : ARImetic Inc.
 *  Sponsor : https://github.com/sponsors/arisohandriputra/
 *  License : MIT
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>

#include "donate.h"
#include "mainwnd.h"   /* for g_hInst, g_hFontSmall, g_hFontNormal */

/* ------------------------------------------------------------------ */
/*  Public: open GitHub Sponsors page                                 */
/* ------------------------------------------------------------------ */

BOOL Donate_OpenSponsorsPage(HWND hParent)
{
    /* ShellExecuteA with verb "open" launches the URL in the user's
       default browser.  SW_SHOWNORMAL is the most portable choice. */
    HINSTANCE hResult = ShellExecuteA(
        hParent,
        "open",
        DONATE_URL,
        NULL, NULL,
        SW_SHOWNORMAL);

    /* A result handle <= 32 indicates failure (per Win32 docs). */
    if ((INT_PTR)hResult <= 32) {
        MessageBoxA(hParent,
            "Unable to open the donations page in your default browser.\n"
            "Please visit the following URL manually:\n\n"
            DONATE_URL,
            "Open Browser Failed",
            MB_ICONWARNING | MB_OK);
        return FALSE;
    }
    return TRUE;
}

/* ------------------------------------------------------------------ */
/*  Donate dialog window procedure                                    */
/* ------------------------------------------------------------------ */

static LRESULT CALLBACK DonateDlgProc(HWND hDlg, UINT uMsg,
                                       WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        {
            int cx = DONATE_DLG_W;

            /* Bold title font used only inside this dialog. */
            HFONT hFontBold = CreateFontA(-15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

            /* ---- Header ------------------------------------------------- */
            HWND hTitle = CreateWindowExA(0, "STATIC",
                "Support HDDHealth Monitor",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                20, 16, cx - 40, 22, hDlg, NULL, g_hInst, NULL);
            SendMessageA(hTitle, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            CreateWindowExA(0, "STATIC", "",
                WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
                20, 46, cx - 40, 2, hDlg, NULL, g_hInst, NULL);

            /* ---- Explanation text -------------------------------------- */
            HWND hExplain = CreateWindowExA(0, "STATIC",
                "HDDHealth Monitor is 100% Free and Open Source Software.\n"
                "There is no license key, no trial, and no activation.\n"
                "All features are available to every user, free of charge.\n\n"
                "If you find this tool useful and would like to support\n"
                "ongoing development, please consider becoming a sponsor.",
                WS_CHILD | WS_VISIBLE,
                20, 56, cx - 40, 96, hDlg, NULL, g_hInst, NULL);
            SendMessageA(hExplain, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

            CreateWindowExA(0, "STATIC", "",
                WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
                20, 158, cx - 40, 2, hDlg, NULL, g_hInst, NULL);

            /* ---- Author / sponsor attribution -------------------------- */
            char authorLine[128];
            _snprintf(authorLine, sizeof(authorLine),
                "Author : %s (%s)", DONATE_AUTHOR, DONATE_COMPANY);
            HWND hAuthor = CreateWindowExA(0, "STATIC", authorLine,
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                20, 168, cx - 40, 18, hDlg, NULL, g_hInst, NULL);
            SendMessageA(hAuthor, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

            /* The link control displays the sponsor URL itself so the user
               can copy it even if they choose not to click. */
            HWND hLink = CreateWindowExA(0, "STATIC", DONATE_URL,
                WS_CHILD | WS_VISIBLE | SS_CENTER | SS_NOTIFY,
                20, 190, cx - 40, 20, hDlg,
                (HMENU)IDC_DONATE_LINK_STATIC, g_hInst, NULL);

            /* Underlined "link-style" font for the URL static. */
            HFONT hFontLink = CreateFontA(-12, 0, 0, 0, FW_NORMAL,
                FALSE, TRUE, FALSE,
                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            SendMessageA(hLink, WM_SETFONT, (WPARAM)hFontLink, TRUE);

            CreateWindowExA(0, "STATIC", "",
                WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
                20, 220, cx - 40, 2, hDlg, NULL, g_hInst, NULL);

            /* ---- Buttons ------------------------------------------------ */
            HWND hDonate = CreateWindowExA(0, "BUTTON", "Donate",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                (cx - 240) / 2, 236, 110, 30, hDlg,
                (HMENU)IDC_DONATE_OPEN_BTN, g_hInst, NULL);
            SendMessageA(hDonate, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            HWND hClose = CreateWindowExA(0, "BUTTON", "Close",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                (cx - 240) / 2 + 130, 236, 110, 30, hDlg,
                (HMENU)IDC_DONATE_CLOSE_BTN, g_hInst, NULL);
            SendMessageA(hClose, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        }
        return 0;

    case WM_COMMAND:
        {
            int nCtrl = LOWORD(wParam);

            if (nCtrl == IDC_DONATE_OPEN_BTN) {
                /* User clicked the Donate button - launch the sponsors page. */
                Donate_OpenSponsorsPage(hDlg);
                return 0;
            }

            if (nCtrl == IDC_DONATE_CLOSE_BTN || nCtrl == IDCANCEL) {
                DestroyWindow(hDlg);
                return 0;
            }

            /* Clicking the URL static also opens the browser - convenient
               for users who cannot tell a static can be clicked. */
            if (nCtrl == IDC_DONATE_LINK_STATIC &&
                HIWORD(wParam) == STN_CLICKED) {
                Donate_OpenSponsorsPage(hDlg);
                return 0;
            }
        }
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hDlg);
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hDlg);
        return 0;
    }

    return DefWindowProcA(hDlg, uMsg, wParam, lParam);
}

/* ------------------------------------------------------------------ */
/*  Public: show the Donate dialog                                    */
/* ------------------------------------------------------------------ */

void Donate_ShowDialog(HWND hParent)
{
    /* Register the dialog window class exactly once per process. */
    static BOOL bRegistered = FALSE;
    if (!bRegistered) {
        WNDCLASSEXA wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = DonateDlgProc;
        wc.hInstance     = g_hInst;
        wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = "LLHDDonateDlg";
        RegisterClassExA(&wc);
        bRegistered = TRUE;
    }

    /* If a donate dialog is already open, just bring it forward. */
    HWND hExist = FindWindowA("LLHDDonateDlg", NULL);
    if (hExist) {
        SetForegroundWindow(hExist);
        return;
    }

    /* Center on the parent window (or on screen if no parent). */
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    int x, y;
    if (hParent) {
        RECT rcP;
        GetWindowRect(hParent, &rcP);
        x = rcP.left + ((rcP.right  - rcP.left) - DONATE_DLG_W) / 2;
        y = rcP.top  + ((rcP.bottom - rcP.top ) - DONATE_DLG_H) / 2;
    } else {
        x = (scrW - DONATE_DLG_W) / 2;
        y = (scrH - DONATE_DLG_H) / 2;
    }

    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME,
        "LLHDDonateDlg",
        "HDDHealth Monitor - Donate",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        x, y, DONATE_DLG_W, DONATE_DLG_H,
        hParent, NULL, g_hInst, NULL);

    if (!hDlg) return;

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
    SetFocus(hDlg);

    /* Modal-style message loop - runs until the dialog is closed. */
    MSG msg;
    while (IsWindow(hDlg) && GetMessageA(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageA(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Public: no-op startup hook                                        */
/* ------------------------------------------------------------------ */

BOOL Donate_Startup(HWND hParent)
{
    /* Intentionally empty.  The program is unconditionally free, so
       there is no registration, no trial timer, and no install-date
       tracking to perform.  Kept only so the existing main.cpp call
       site does not require a separate patch. */
    (void)hParent;
    return TRUE;
}
