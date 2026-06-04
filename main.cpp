#include <windows.h>
#include <commdlg.h>
#include <tlhelp32.h>
#include <string>
#include <vector>

#define ID_BTN_TARGET   101
#define ID_BTN_DLL      102
#define ID_BTN_INJECT   103
#define ID_TXT_CONSOLE  104

HWND hTargetInput, hDllInput, hConsoleLog;
char targetExePath[MAX_PATH] = "";
char targetExeName[MAX_PATH] = "";
char dllPath[MAX_PATH] = "";

// Theme Colors
COLORREF COLOR_DARK_GREY  = RGB(30, 30, 30);    // Window Background
COLORREF COLOR_LIGHT_GREY = RGB(50, 50, 50);    // Path Input Fields
COLORREF COLOR_BLACK      = RGB(0, 0, 0);       // Console Output Field
COLORREF COLOR_GREEN      = RGB(0, 255, 64);    // Glowing Theme Font

HBRUSH hBrushDarkGrey;
HBRUSH hBrushLightGrey;
HBRUSH hBrushBlack;

void LogToConsole(const std::string& message) {
    std::string format = message + "\r\n";
    int index = GetWindowTextLengthA(hConsoleLog);
    SendMessageA(hConsoleLog, EM_SETSEL, index, index);
    SendMessageA(hConsoleLog, EM_REPLACESEL, 0, (LPARAM)format.c_str());
}

DWORD GetPidByProcessName(const char* procName) {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 processEntry;
        processEntry.dwSize = sizeof(processEntry);
        if (Process32First(snapshot, &processEntry)) {
            do {
                if (_stricmp(processEntry.szExeFile, procName) == 0) {
                    pid = processEntry.th32ProcessID;
                    break;
                }
            } while (Process32Next(snapshot, &processEntry));
        }
        CloseHandle(snapshot);
    }
    return pid;
}

bool ExecuteInjection(DWORD processId, const char* path) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProcess) {
        LogToConsole("[-] Error: OpenProcess failed. Run as Admin.");
        return false;
    }

    LPVOID pRemoteMemory = VirtualAllocEx(hProcess, NULL, strlen(path) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteMemory) {
        LogToConsole("[-] Error: Virtual memory allocation failed.");
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, pRemoteMemory, path, strlen(path) + 1, NULL)) {
        LogToConsole("[-] Error: WriteProcessMemory failed.");
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    LPVOID pLoadLibrary = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pRemoteMemory, 0, NULL);
    if (!hThread) {
        LogToConsole("[-] Error: CreateRemoteThread failed.");
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);
    return true;
}

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            hBrushDarkGrey  = CreateSolidBrush(COLOR_DARK_GREY);
            hBrushLightGrey = CreateSolidBrush(COLOR_LIGHT_GREY);
            hBrushBlack     = CreateSolidBrush(COLOR_BLACK);

            CreateWindowA("Static", "Target Exe:", WS_VISIBLE | WS_CHILD, 10, 15, 80, 20, hwnd, NULL, NULL, NULL);
            hTargetInput = CreateWindowA("Edit", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY, 90, 12, 260, 22, hwnd, NULL, NULL, NULL);
            CreateWindowA("Button", "Browse...", WS_VISIBLE | WS_CHILD, 360, 10, 90, 25, hwnd, (HMENU)ID_BTN_TARGET, NULL, NULL);

            CreateWindowA("Static", "Payload DLL:", WS_VISIBLE | WS_CHILD, 10, 50, 80, 20, hwnd, NULL, NULL, NULL);
            hDllInput = CreateWindowA("Edit", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY, 90, 47, 260, 22, hwnd, NULL, NULL, NULL);
            CreateWindowA("Button", "Browse...", WS_VISIBLE | WS_CHILD, 360, 45, 90, 25, hwnd, (HMENU)ID_BTN_DLL, NULL, NULL);

            CreateWindowA("Button", "EXECUTE INJECTION", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 85, 440, 35, hwnd, (HMENU)ID_BTN_INJECT, NULL, NULL);

            CreateWindowA("Static", "Console Output Log:", WS_VISIBLE | WS_CHILD, 10, 135, 250, 15, hwnd, NULL, NULL, NULL);
            hConsoleLog = CreateWindowA("Edit", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, 10, 155, 440, 180, hwnd, (HMENU)ID_TXT_CONSOLE, NULL, NULL);
            
            LogToConsole("[*] CrownDrip Engine Operational. Awaiting target elements.");
            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, COLOR_GREEN);
            SetBkColor(hdc, COLOR_DARK_GREY);
            return (LRESULT)hBrushDarkGrey;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wp;
            HWND hwndEdit = (HWND)lp;
            SetTextColor(hdc, COLOR_GREEN);

            if (hwndEdit == hConsoleLog) {
                SetBkColor(hdc, COLOR_BLACK);
                return (LRESULT)hBrushBlack;
            } else {
                SetBkColor(hdc, COLOR_LIGHT_GREY);
                return (LRESULT)hBrushLightGrey;
            }
        }

        case WM_COMMAND: {
            OPENFILENAMEA ofn;
            memset(&ofn, 0, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (LOWORD(wp) == ID_BTN_TARGET) {
                ofn.lpstrFile = targetExePath;
                ofn.lpstrFilter = "Application (*.exe)\0*.exe\0All Files\0*.*\0";
                if (GetOpenFileNameA(&ofn)) {
                    SetWindowTextA(hTargetInput, targetExePath);
                    std::string fullPath(targetExePath);
                    size_t lastSlash = fullPath.find_last_of("\\/");
                    std::string filename = (lastSlash == std::string::npos) ? fullPath : fullPath.substr(lastSlash + 1);
                    strcpy_s(targetExeName, filename.c_str());
                    LogToConsole("[*] Target locked: " + filename);
                }
            }
            else if (LOWORD(wp) == ID_BTN_DLL) {
                ofn.lpstrFile = dllPath;
                ofn.lpstrFilter = "Dynamic Link Library (*.dll)\0*.dll\0";
                if (GetOpenFileNameA(&ofn)) {
                    SetWindowTextA(hDllInput, dllPath);
                    LogToConsole("[*] Payload path mapped: " + std::string(dllPath));
                }
            }
            else if (LOWORD(wp) == ID_BTN_INJECT) {
                if (strlen(targetExeName) == 0 || strlen(dllPath) == 0) {
                    LogToConsole("[-] Error: Targets missing.");
                    break;
                }

                LogToConsole("[*] Scanning system handles for: " + std::string(targetExeName));
                DWORD pid = GetPidByProcessName(targetExeName);

                if (pid == 0) {
                    LogToConsole("[-] Error: Selected target is not active.");
                } else {
                    LogToConsole("[+] Target verified. PID: " + std::to_string(pid));
                    LogToConsole("[*] Launching memory runtime threads...");
                    if (ExecuteInjection(pid, dllPath)) {
                        LogToConsole("[+] [SUCCESS] Injection routing complete!");
                    }
                }
            }
            break;
        }

        case WM_DESTROY: {
            DeleteObject(hBrushDarkGrey);
            DeleteObject(hBrushLightGrey);
            DeleteObject(hBrushBlack);
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProcA(hwnd, msg, wp, lp);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR args, int ncmdshow) {
    WNDCLASSEXA Wcx = {0};
    const char* className = "CrownDripInjectorClass";
    
    hBrushDarkGrey = CreateSolidBrush(COLOR_DARK_GREY);

    Wcx.cbSize = sizeof(WNDCLASSEXA);
    Wcx.lpfnWndProc = WindowProcedure;
    Wcx.hInstance = hInst;
    Wcx.lpszClassName = className;
    Wcx.hbrBackground = hBrushDarkGrey;
    Wcx.hCursor = LoadCursor(NULL, IDC_ARROW);

    // Matches the integer ID 1 specified inside resource.rc
    Wcx.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(1));

    if (!RegisterClassExA(&Wcx)) return 0;

    HWND hwnd = CreateWindowExA(0, className, "CrownDrip DLL Injector", 
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE, 
                                CW_USEDEFAULT, CW_USEDEFAULT, 475, 385, NULL, NULL, hInst, NULL);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
