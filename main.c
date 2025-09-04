#include <windows.h>
#include <wtsapi32.h>
#include <shellapi.h>
#include <stdio.h>
#include "resource.h"

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_APP_ICON 1001
#define ID_TRAY_MENU_MONITOR 1002
#define ID_TRAY_MENU_EXIT 1003
#define ID_TRAY_MENU_TITLE 1004

const char *APP_NAME = "Numlock Monitor";

HINSTANCE hInst;
NOTIFYICONDATAA nid = {0};
HMENU hTrayMenu;
BOOL monitoring = TRUE;
UINT_PTR timerId = 1;
BOOL gCurrentThemeIsLight = TRUE;

void SetNumLock(BOOL bState)
{
    BYTE keyState[256];
    GetKeyboardState((LPBYTE)&keyState);

    if ((bState && !(keyState[VK_NUMLOCK] & 1)) ||
        (!bState && (keyState[VK_NUMLOCK] & 1)))
    {
        // Toggle NumLock
        keybd_event(VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY, 0);
        keybd_event(VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    }
}

void EnsureNumLockOn()
{
    SHORT state = GetKeyState(VK_NUMLOCK);
    if (!(state & 1))
    {
        SetNumLock(TRUE);
    }
}

BOOL isLightTheme()
{
    HKEY hKey;
    DWORD value = 1; // default to light
    DWORD size = sizeof(value);

    if (RegOpenKeyExA(HKEY_CURRENT_USER,
                      "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {

        RegQueryValueExA(hKey, "SystemUsesLightTheme", NULL, NULL,
                         (LPBYTE)&value, &size);
        RegCloseKey(hKey);
    }
    return value != 0;
}

HICON LoadTrayIcon()
{
    WORD iconId = gCurrentThemeIsLight ? IDI_ICON_BLACK : IDI_ICON_WHITE;
    HICON hIcon = LoadImageA(
        hInst,
        MAKEINTRESOURCEA(iconId),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), // typically 16
        GetSystemMetrics(SM_CYSMICON), // typically 16
        LR_DEFAULTCOLOR);

    return hIcon;
}

void InitTrayIcon(HWND hwnd)
{
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATAA);
    nid.hWnd = hwnd;
    nid.uID = ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    // nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    nid.hIcon = LoadTrayIcon();
    strcpy(nid.szTip, APP_NAME);
    Shell_NotifyIconA(NIM_ADD, &nid);
}

void UpdateIconColor(void)
{
    // Old icon doesn't exist so we might actually be too early
    if (NULL == nid.hIcon)
    {
        return;
    }

    BOOL isNowLight = isLightTheme();
    if (isNowLight != gCurrentThemeIsLight)
    {
        gCurrentThemeIsLight = isNowLight;

        // Reload icon
        HICON newIcon = LoadTrayIcon();
        if (newIcon)
        {
            DestroyIcon(nid.hIcon); // Clean up old icon
            nid.hIcon = newIcon;
            Shell_NotifyIcon(NIM_MODIFY, &nid); // Update tray icon
        }
    }
}

void ShowTrayMenu(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);

    SetForegroundWindow(hwnd);

    CheckMenuItem(hTrayMenu, ID_TRAY_MENU_MONITOR,
                  MF_BYCOMMAND | (monitoring ? MF_CHECKED : MF_UNCHECKED));

    TrackPopupMenu(hTrayMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN,
                   pt.x, pt.y, 0, hwnd, NULL);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        gCurrentThemeIsLight = isLightTheme();
        InitTrayIcon(hwnd);
        hTrayMenu = CreatePopupMenu();
        AppendMenuA(hTrayMenu, MF_STRING | MF_DISABLED, ID_TRAY_MENU_TITLE, APP_NAME);
        AppendMenuA(hTrayMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuA(hTrayMenu, MF_STRING, ID_TRAY_MENU_MONITOR, "Enabled");
        AppendMenuA(hTrayMenu, MF_STRING, ID_TRAY_MENU_EXIT, "Exit");
        SetTimer(hwnd, timerId, 1000, NULL);
        break;

    case WM_TIMER:
        if (monitoring)
        {
            EnsureNumLockOn();
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_TRAY_MENU_MONITOR:
            monitoring = !monitoring;
            break;
        case ID_TRAY_MENU_EXIT:
            Shell_NotifyIconA(NIM_DELETE, &nid);
            PostQuitMessage(0);
            break;
        }
        break;

    case WM_TRAYICON:
        if (lParam == WM_LBUTTONUP || lParam == WM_RBUTTONUP)
        {
            ShowTrayMenu(hwnd);
        }
        break;

    case WM_DESTROY:
        Shell_NotifyIconA(NIM_DELETE, &nid);
        if (nid.hIcon)
        {
            DestroyIcon(nid.hIcon);
            nid.hIcon = NULL;
        }
        PostQuitMessage(0);
        break;

    case WM_SETTINGCHANGE:
        if (0 == wParam && lParam && strcmp((const char *)lParam, "ImmersiveColorSet") == 0)
        {
            UpdateIconColor();
        }
        break;

    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // Check if running in the active console session (desktop)
    DWORD sessionId = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &sessionId))
    {
        // Could not get session ID, exit
        return 5; // ERROR_ACCESS_DENIED
    }
    DWORD activeSessionId = WTSGetActiveConsoleSessionId();
    if (sessionId != activeSessionId)
    {
        // Not running in the interactive session, exit
        return 10; // ERROR_BAD_ENVIRONMENT
    }

    HANDLE hMutex = CreateMutexA(NULL, TRUE, "Global\\NumLockMonitorMutex");
    if (NULL == hMutex || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        // Already running
        if (hMutex)
        {
            CloseHandle(hMutex);
        }
        return 32; // ERROR_SHARING_VIOLATION
    }

    hInst = hInstance;
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "NumlockMonitorClass";

    RegisterClass(&wc);

    // Create a hidden invisible window so we can get events
    (void)CreateWindowExA(0, "NumlockMonitorClass", APP_NAME,
                          0, 0, 0, 0, 0,
                          NULL, NULL, hInstance, NULL);
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return 0;
}
