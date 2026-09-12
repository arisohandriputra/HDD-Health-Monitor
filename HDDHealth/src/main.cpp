/* ============================================================================
 *  HDDHealth Monitor 1.3 - WinMain entry point
 *
 *  Author  : Ari Sohandri Putra
 *  Sponsor : https://github.com/sponsors/arisohandriputra/
 *  License : MIT
 *
 *  Does the usual WinMain stuff: single-instance guard, common controls
 *  init, GDI objects, window class, message loop.
 * ============================================================================
 */
/* MSVC-only pragmas for comctl32.lib and Common-Controls v6 manifest.
   MinGW ignores these (wrapped in _MSC_VER to keep it clean). */
#ifdef _MSC_VER
#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>

#include "mainwnd.h"
#include "smart.h"
#include "resource.h"
#include "donate.h"    

/* A globally-named mutex prevents the user from accidentally launching
   multiple instances of the monitor.  The "Global\" prefix makes the
   mutex visible across all terminal-server sessions. */
#define MUTEX_NAME  "Global\\HDDHealth_SingleInstance_v1"

/* ------------------------------------------------------------------ */
/*  Single-instance helpers                                           */
/* ------------------------------------------------------------------ */

/* Global mutex with a NULL DACL so any user session can touch it.
   We need this because the app runs elevated but we still want
   one instance per machine. */
static HANDLE CreateWorldMutex(void)
{
    SECURITY_DESCRIPTOR sd;
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);

    SECURITY_ATTRIBUTES sa;
    sa.nLength              = sizeof(sa);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle       = FALSE;

    return CreateMutexA(&sa, TRUE, MUTEX_NAME);
}

/* If another instance is already running, this helper locates its
   main window and brings it to the foreground instead of starting
   a duplicate process. */
static void BringExistingWindowToFront(void)
{
    HWND hExist = FindWindowA("LLHDMonitorMainWnd", NULL);
    if (!hExist) return;

    if (!IsWindowVisible(hExist))
        ShowWindow(hExist, SW_SHOW);

    if (IsIconic(hExist))
        ShowWindow(hExist, SW_RESTORE);

    DWORD dwPid = 0;
    GetWindowThreadProcessId(hExist, &dwPid);
    AllowSetForegroundWindow(dwPid);
    SetForegroundWindow(hExist);
    BringWindowToTop(hExist);
}

/* ------------------------------------------------------------------ */
/*  WinMain - program entry point                                     */
/* ------------------------------------------------------------------ */

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    /* ---- Single-instance guard ------------------------------------ */
    HANDLE hMutex = CreateWorldMutex();
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        BringExistingWindowToFront();
        CloseHandle(hMutex);
        return 0;
    }

    g_hInst = hInstance;

    /* ---- Common controls init (ListViews, etc.) ------------------- */
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC  = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    /* ---- GDI objects (brushes, fonts) ----------------------------- */
    CreateGDIObjects();

   
    Donate_Startup(NULL);

    /* ---- Register the main window class --------------------------- */
    WNDCLASSEXA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "LLHDMonitorMainWnd";
    wc.hIcon         = LoadIconA(hInstance, MAKEINTRESOURCEA(IDI_APPICON));
    wc.hIconSm       = (HICON)LoadImageA(hInstance, MAKEINTRESOURCEA(IDI_APPICON),
                                          IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "RegisterClassEx failed!", "Error", MB_ICONERROR);
        if (hMutex) { ReleaseMutex(hMutex); CloseHandle(hMutex); }
        return 1;
    }

    /* ---- Center the main window on the primary monitor ------------ */
    int nScrW = GetSystemMetrics(SM_CXSCREEN);
    int nScrH = GetSystemMetrics(SM_CYSCREEN);
    int nX    = (nScrW - WINDOW_W) / 2;
    int nY    = (nScrH - WINDOW_H) / 2;

    HMENU hMenuBar  = CreateMenu();
    HMENU hMenuFile = CreatePopupMenu();
    HMENU hMenuHelp = CreatePopupMenu();
    AppendMenuA(hMenuFile, MF_STRING,    IDM_SHOW_WINDOW, "&Show Window");
    AppendMenuA(hMenuFile, MF_SEPARATOR, 0,               NULL);
    AppendMenuA(hMenuFile, MF_STRING,    IDM_EXIT,        "E&xit");
    AppendMenuA(hMenuHelp, MF_STRING,    IDM_DONATE,      "&Donate...");
    AppendMenuA(hMenuHelp, MF_SEPARATOR, 0,               NULL);
    AppendMenuA(hMenuHelp, MF_STRING,    IDM_ABOUT,       "&About HDDHealth...");
    AppendMenuA(hMenuBar,  MF_POPUP, (UINT_PTR)hMenuFile, "&File");
    AppendMenuA(hMenuBar,  MF_POPUP, (UINT_PTR)hMenuHelp, "&Help");

    HWND hWnd = CreateWindowExA(
        0,
        "LLHDMonitorMainWnd",
        "HDDHealth Monitor 1.3",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        nX, nY, WINDOW_W, WINDOW_H,
        NULL, hMenuBar, hInstance, NULL
    );

    if (!hWnd) {
        MessageBoxA(NULL, "CreateWindow failed!", "Error", MB_ICONERROR);
        if (hMutex) { ReleaseMutex(hMutex); CloseHandle(hMutex); }
        return 1;
    }

    /* Allow silent startup (e.g. launched from the autostart folder)
       via the /minimized command-line switch. */
    ShowWindow(hWnd, (lpCmdLine && strstr(lpCmdLine, "/minimized")) ? SW_HIDE : nCmdShow);
    UpdateWindow(hWnd);

    /* ---- Standard Win32 message loop ------------------------------ */
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
    return (int)msg.wParam;
}
