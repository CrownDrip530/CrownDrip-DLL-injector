#include <windows.h>
#include <commdlg.h>
#include <tlhelp32.h>
#include <string>
#include <vector>

#define ID_BTN_TARGET   101
#define ID_BTN_DLL      102
#define ID_BTN_INJECT   103
#define ID_TXT_CONSOLE  104
#define ID_BTN_CLEAR    105

HWND hTargetInput, hDllInput, hConsoleLog;
char targetExePath[MAX_PATH] = "";
char targetExeName[MAX_PATH] = "";
char dllPath[MAX_PATH] = "";

// === Theme Palette ===
COLORREF COL_BG         = RGB(12,  12,  14);   // Deepest background
COLORREF COL_PANEL      = RGB(20,  20,  24);   // Panel / section bg
COLORREF COL_FIELD      = RGB(28,  28,  34);   // Input field bg
COLORREF COL_BORDER     = RGB(45,  45,  55);   // Subtle border
COLORREF COL_GOLD       = RGB(212, 175, 55);   // Crown gold accent
COLORREF COL_GOLD_DIM   = RGB(140, 110, 30);   // Dimmed gold for labels
COLORREF COL_CONSOLE_BG = RGB(8,   8,   10);   // Console bg
COLORREF COL_GREEN      = RGB(80,  220, 120);  // Success green
COLORREF COL_RED        = RGB(220, 70,  70);   // Error red
COLORREF COL_WHITE      = RGB(230, 230, 240);  // Primary text

HBRUSH hBrBg, hBrPanel, hBrField, hBrConsoleBg, hBrBorder;
HFONT hFontTitle, hFontLabel, hFontMono, hFontBtnInject, hFontSub;

// === Forward Declarations ===
void LogToConsole(const std::string& message);

// -----------------------------------------------------------------------
// Logging
// -----------------------------------------------------------------------
void LogToConsole(const std::string& message) {
    std::string fmt = message + "\r\n";
    int idx = GetWindowTextLengthA(hConsoleLog);
    SendMessageA(hConsoleLog, EM_SETSEL, idx, idx);
    SendMessageA(hConsoleLog, EM_REPLACESEL, 0, (LPARAM)fmt.c_str());
}

// -----------------------------------------------------------------------
// Process helpers
// -----------------------------------------------------------------------
DWORD GetPidByProcessName(const char* procName) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(snap, &pe)) {
            do {
                if (_stricmp(pe.szExeFile, procName) == 0) {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
    }
    return pid;
}

bool ExecuteInjection(DWORD processId, const char* path) {
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProc) {
        LogToConsole("  [-] OpenProcess failed — run as Administrator.");
        return false;
    }
    LPVOID pMem = VirtualAllocEx(hProc, NULL, strlen(path) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pMem) {
        LogToConsole("  [-] VirtualAllocEx failed.");
        CloseHandle(hProc);
        return false;
    }
    if (!WriteProcessMemory(hProc, pMem, path, strlen(path) + 1, NULL)) {
        LogToConsole("  [-] WriteProcessMemory failed.");
        VirtualFreeEx(hProc, pMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }
    LPVOID pLL = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)pLL, pMem, 0, NULL);
    if (!hThread) {
        LogToConsole("  [-] CreateRemoteThread failed.");
        VirtualFreeEx(hProc, pMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }
    WaitForSingleObject(hThread, INFINITE);
    VirtualFreeEx(hProc, pMem, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProc);
    return true;
}

// -----------------------------------------------------------------------
// Custom draw helpers
// -----------------------------------------------------------------------
void PaintDivider(HDC hdc, int x, int y, int w) {
    HPEN pen = CreatePen(PS_SOLID, 1, COL_BORDER);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x, y, NULL);
    LineTo(hdc, x + w, y);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

void PaintGoldAccentBar(HDC hdc, int x, int y, int h) {
    HPEN pen = CreatePen(PS_SOLID, 2, COL_GOLD);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x, y, NULL);
    LineTo(hdc, x, y + h);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

// -----------------------------------------------------------------------
// Window Procedure
// -----------------------------------------------------------------------
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_CREATE: {
        // --- Brushes ---
        hBrBg        = CreateSolidBrush(COL_BG);
        hBrPanel     = CreateSolidBrush(COL_PANEL);
        hBrField     = CreateSolidBrush(COL_FIELD);
        hBrConsoleBg = CreateSolidBrush(COL_CONSOLE_BG);
        hBrBorder    = CreateSolidBrush(COL_BORDER);

        // --- Fonts ---
        hFontTitle    = CreateFontA(26, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
        hFontSub      = CreateFontA(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
        hFontLabel    = CreateFontA(13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
        hFontMono     = CreateFontA(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Consolas");
        hFontBtnInject= CreateFontA(15, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");

        // === TARGET EXE LABEL ===
        HWND hLbl1 = CreateWindowA("Static", "TARGET EXECUTABLE", WS_VISIBLE | WS_CHILD,
            56, 120, 200, 18, hwnd, NULL, NULL, NULL);
        SendMessage(hLbl1, WM_SETFONT, (WPARAM)hFontLabel, TRUE);

        // === TARGET INPUT ===
        hTargetInput = CreateWindowA("Edit", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY,
            56, 143, 330, 28, hwnd, NULL, NULL, NULL);
        SendMessage(hTargetInput, WM_SETFONT, (WPARAM)hFontMono, TRUE);

        // === TARGET BROWSE BTN ===
        HWND hBtn1 = CreateWindowA("Button", "BROWSE", WS_VISIBLE | WS_CHILD,
            396, 143, 80, 28, hwnd, (HMENU)ID_BTN_TARGET, NULL, NULL);
        SendMessage(hBtn1, WM_SETFONT, (WPARAM)hFontLabel, TRUE);

        // === DLL LABEL ===
        HWND hLbl2 = CreateWindowA("Static", "PAYLOAD DLL", WS_VISIBLE | WS_CHILD,
            56, 193, 200, 18, hwnd, NULL, NULL, NULL);
        SendMessage(hLbl2, WM_SETFONT, (WPARAM)hFontLabel, TRUE);

        // === DLL INPUT ===
        hDllInput = CreateWindowA("Edit", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY,
            56, 216, 330, 28, hwnd, NULL, NULL, NULL);
        SendMessage(hDllInput, WM_SETFONT, (WPARAM)hFontMono, TRUE);

        // === DLL BROWSE BTN ===
        HWND hBtn2 = CreateWindowA("Button", "BROWSE", WS_VISIBLE | WS_CHILD,
            396, 216, 80, 28, hwnd, (HMENU)ID_BTN_DLL, NULL, NULL);
        SendMessage(hBtn2, WM_SETFONT, (WPARAM)hFontLabel, TRUE);

        // === INJECT BTN ===
        HWND hBtnInject = CreateWindowA("Button", "EXECUTE INJECTION", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            56, 265, 420, 40, hwnd, (HMENU)ID_BTN_INJECT, NULL, NULL);
        SendMessage(hBtnInject, WM_SETFONT, (WPARAM)hFontBtnInject, TRUE);

        // === CONSOLE LABEL ===
        HWND hLblCon = CreateWindowA("Static", "OUTPUT LOG", WS_VISIBLE | WS_CHILD,
            56, 325, 200, 18, hwnd, NULL, NULL, NULL);
        SendMessage(hLblCon, WM_SETFONT, (WPARAM)hFontLabel, TRUE);

        // === CLEAR BTN ===
        HWND hBtnClear = CreateWindowA("Button", "CLEAR", WS_VISIBLE | WS_CHILD,
            406, 322, 70, 22, hwnd, (HMENU)ID_BTN_CLEAR, NULL, NULL);
        SendMessage(hBtnClear, WM_SETFONT, (WPARAM)hFontSub, TRUE);

        // === CONSOLE OUTPUT ===
        hConsoleLog = CreateWindowA("Edit", "",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
            56, 348, 420, 180, hwnd, (HMENU)ID_TXT_CONSOLE, NULL, NULL);
        SendMessage(hConsoleLog, WM_SETFONT, (WPARAM)hFontMono, TRUE);

        LogToConsole("  [*] CrownDrip Engine v2.0 — Operational.");
        LogToConsole("  [*] Select a target executable and payload DLL to begin.");
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        // Full background
        FillRect(hdc, &rc, hBrBg);

        // ── Header banner ──
        RECT hdrRect = { 0, 0, rc.right, 105 };
        FillRect(hdc, &hdrRect, hBrPanel);

        // Gold top accent bar
        HPEN penGold = CreatePen(PS_SOLID, 3, COL_GOLD);
        HPEN oldPen  = (HPEN)SelectObject(hdc, penGold);
        MoveToEx(hdc, 0, 0, NULL);
        LineTo(hdc, rc.right, 0);
        SelectObject(hdc, oldPen);
        DeleteObject(penGold);

        // Crown symbol (◆)
        SetBkMode(hdc, TRANSPARENT);
        HFONT hCrown = CreateFontA(38, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI Symbol");
        SelectObject(hdc, hCrown);
        SetTextColor(hdc, COL_GOLD);
        TextOutA(hdc, 22, 22, "\xE2\x97\x86", 3); // UTF-8 ◆, works as ANSI fallback too
        DeleteObject(hCrown);

        // Title text: CrownDrip
        SelectObject(hdc, hFontTitle);
        SetTextColor(hdc, COL_GOLD);
        TextOutA(hdc, 56, 18, "CrownDrip", 9);

        // Subtitle: DLL INJECTOR
        HFONT hSubTitle = CreateFontA(13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
        SelectObject(hdc, hSubTitle);
        SetTextColor(hdc, COL_GOLD_DIM);
        TextOutA(hdc, 58, 50, "DLL INJECTOR", 12);
        DeleteObject(hSubTitle);

        // Version tag
        SelectObject(hdc, hFontSub);
        SetTextColor(hdc, RGB(80, 80, 95));
        TextOutA(hdc, 58, 70, "v2.0  |  loadlibrary injection  |  x64/x86", 42);

        // Header bottom divider
        PaintDivider(hdc, 0, 104, rc.right);

        // ── Section: Target & DLL ──
        RECT secRect1 = { 36, 110, rc.right - 36, 258 };
        FillRect(hdc, &secRect1, hBrPanel);
        PaintGoldAccentBar(hdc, 36, 110, 148);

        // ── Section: Inject ──
        RECT secRect2 = { 36, 258, rc.right - 36, 315 };
        FillRect(hdc, &secRect2, hBrPanel);

        // ── Section: Console ──
        RECT secRect3 = { 36, 315, rc.right - 36, rc.bottom - 15 };
        FillRect(hdc, &secRect3, hBrPanel);
        PaintGoldAccentBar(hdc, 36, 315, rc.bottom - 15 - 315);

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COL_GOLD_DIM);
        return (LRESULT)hBrPanel;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wp;
        HWND hCtrl = (HWND)lp;
        if (hCtrl == hConsoleLog) {
            SetBkColor(hdc, COL_CONSOLE_BG);
            SetTextColor(hdc, COL_GREEN);
            return (LRESULT)hBrConsoleBg;
        }
        SetBkColor(hdc, COL_FIELD);
        SetTextColor(hdc, COL_WHITE);
        return (LRESULT)hBrField;
    }

    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, COL_PANEL);
        SetTextColor(hdc, COL_GOLD);
        return (LRESULT)hBrPanel;
    }

    case WM_COMMAND: {
        if (LOWORD(wp) == ID_BTN_CLEAR) {
            SetWindowTextA(hConsoleLog, "");
            LogToConsole("  [*] Log cleared.");
            break;
        }

        OPENFILENAMEA ofn;
        memset(&ofn, 0, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner   = hwnd;
        ofn.nMaxFile    = MAX_PATH;
        ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (LOWORD(wp) == ID_BTN_TARGET) {
            ofn.lpstrFile   = targetExePath;
            ofn.lpstrFilter = "Application (*.exe)\0*.exe\0All Files\0*.*\0";
            if (GetOpenFileNameA(&ofn)) {
                SetWindowTextA(hTargetInput, targetExePath);
                std::string full(targetExePath);
                size_t sl = full.find_last_of("\\/");
                std::string name = (sl == std::string::npos) ? full : full.substr(sl + 1);
                strcpy_s(targetExeName, name.c_str());
                LogToConsole("  [+] Target locked  ->  " + name);
            }
        }
        else if (LOWORD(wp) == ID_BTN_DLL) {
            ofn.lpstrFile   = dllPath;
            ofn.lpstrFilter = "Dynamic Link Library (*.dll)\0*.dll\0";
            if (GetOpenFileNameA(&ofn)) {
                SetWindowTextA(hDllInput, dllPath);
                LogToConsole("  [+] Payload mapped  ->  " + std::string(dllPath));
            }
        }
        else if (LOWORD(wp) == ID_BTN_INJECT) {
            if (strlen(targetExeName) == 0 || strlen(dllPath) == 0) {
                LogToConsole("  [-] Error: Target or payload not set.");
                break;
            }
            LogToConsole("  -----------------------------------------------");
            LogToConsole("  [*] Scanning for process: " + std::string(targetExeName));
            DWORD pid = GetPidByProcessName(targetExeName);
            if (pid == 0) {
                LogToConsole("  [-] Process not found — is the target running?");
            } else {
                LogToConsole("  [+] Process found  ->  PID " + std::to_string(pid));
                LogToConsole("  [*] Allocating remote memory and writing payload...");
                if (ExecuteInjection(pid, dllPath)) {
                    LogToConsole("  [+] INJECTION COMPLETE  ->  DLL loaded successfully.");
                }
            }
            LogToConsole("  -----------------------------------------------");
        }
        break;
    }

    case WM_DESTROY: {
        DeleteObject(hBrBg);
        DeleteObject(hBrPanel);
        DeleteObject(hBrField);
        DeleteObject(hBrConsoleBg);
        DeleteObject(hBrBorder);
        DeleteObject(hFontTitle);
        DeleteObject(hFontSub);
        DeleteObject(hFontLabel);
        DeleteObject(hFontMono);
        DeleteObject(hFontBtnInject);
        PostQuitMessage(0);
        break;
    }

    default:
        return DefWindowProcA(hwnd, msg, wp, lp);
    }
    return 0;
}

// -----------------------------------------------------------------------
// WinMain
// -----------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    WNDCLASSEXA wcx = {0};
    const char* cls = "CrownDripInjectorClass";

    hBrBg = CreateSolidBrush(COL_BG);

    wcx.cbSize        = sizeof(WNDCLASSEXA);
    wcx.lpfnWndProc   = WindowProcedure;
    wcx.hInstance     = hInst;
    wcx.lpszClassName = cls;
    wcx.hbrBackground = hBrBg;
    wcx.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcx.hIcon         = LoadIcon(hInst, MAKEINTRESOURCE(1));

    if (!RegisterClassExA(&wcx)) return 0;

    HWND hwnd = CreateWindowExA(
        0, cls, "CrownDrip DLL Injector",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 570,
        NULL, NULL, hInst, NULL
    );

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
