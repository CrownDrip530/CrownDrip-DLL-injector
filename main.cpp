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

// 介面配色定義
COLORREF COLOR_DARK_GREY  = RGB(30, 30, 30);    // 主視窗背景：深灰色
COLORREF COLOR_LIGHT_GREY = RGB(50, 50, 50);    // 輸入框背景：中灰色
COLORREF COLOR_BLACK      = RGB(0, 0, 0);       // 主控台背景：純黑色
COLORREF COLOR_GREEN      = RGB(0, 255, 64);    // 文字顏色：螢光綠

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
        LogToConsole("[-] 錯誤: OpenProcess 失敗。請嘗試以管理員身份執行。");
        return false;
    }

    LPVOID pRemoteMemory = VirtualAllocEx(hProcess, NULL, strlen(path) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteMemory) {
        LogToConsole("[-] 錯誤: 無法在目標程序中分配記憶體。");
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, pRemoteMemory, path, strlen(path) + 1, NULL)) {
        LogToConsole("[-] 錯誤: WriteProcessMemory 失敗。");
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    LPVOID pLoadLibrary = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pRemoteMemory, 0, NULL);
    if (!hThread) {
        LogToConsole("[-] 錯誤: 建立遠端執行緒失敗。");
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

            CreateWindowA("Static", "目標主程式:", WS_VISIBLE | WS_CHILD, 10, 15, 80, 20, hwnd, NULL, NULL, NULL);
            hTargetInput = CreateWindowA("Edit", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY, 90, 12, 260, 22, hwnd, NULL, NULL, NULL);
            CreateWindowA("Button", "瀏覽...", WS_VISIBLE | WS_CHILD, 360, 10, 90, 25, hwnd, (HMENU)ID_BTN_TARGET, NULL, NULL);

            CreateWindowA("Static", "注入 DLL:", WS_VISIBLE | WS_CHILD, 10, 50, 80, 20, hwnd, NULL, NULL, NULL);
            hDllInput = CreateWindowA("Edit", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY, 90, 47, 260, 22, hwnd, NULL, NULL, NULL);
            CreateWindowA("Button", "瀏覽...", WS_VISIBLE | WS_CHILD, 360, 45, 90, 25, hwnd, (HMENU)ID_BTN_DLL, NULL, NULL);

            CreateWindowA("Button", "執行 DLL 注入", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 85, 440, 35, hwnd, (HMENU)ID_BTN_INJECT, NULL, NULL);

            CreateWindowA("Static", "主控台輸出紀錄 (Console Log):", WS_VISIBLE | WS_CHILD, 10, 135, 250, 15, hwnd, NULL, NULL, NULL);
            hConsoleLog = CreateWindowA("Edit", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, 10, 155, 440, 180, hwnd, (HMENU)ID_TXT_CONSOLE, NULL, NULL);
            
            LogToConsole("[*] 系統就緒。請選擇目標 EXE 與要注入的 DLL 檔案。");
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
                ofn.lpstrFilter = "執行檔 (*.exe)\0*.exe\0所有檔案\0*.*\0";
                if (GetOpenFileNameA(&ofn)) {
                    SetWindowTextA(hTargetInput, targetExePath);
                    std::string fullPath(targetExePath);
                    size_t lastSlash = fullPath.find_last_of("\\/");
                    std::string filename = (lastSlash == std::string::npos) ? fullPath : fullPath.substr(lastSlash + 1);
                    strcpy_s(targetExeName, filename.c_str());
                    LogToConsole("[*] 已鎖定目標名稱: " + filename);
                }
            }
            else if (LOWORD(wp) == ID_BTN_DLL) {
                ofn.lpstrFile = dllPath;
                ofn.lpstrFilter = "動態連結庫 (*.dll)\0*.dll\0";
                if (GetOpenFileNameA(&ofn)) {
                    SetWindowTextA(hDllInput, dllPath);
                    LogToConsole("[*] 已載入 DLL 檔案: " + std::string(dllPath));
                }
            }
            else if (LOWORD(wp) == ID_BTN_INJECT) {
                if (strlen(targetExeName) == 0 || strlen(dllPath) == 0) {
                    LogToConsole("[-] 錯誤: 請確認目標 EXE 與 DLL 皆已選擇。");
                    break;
                }

                LogToConsole("[*] 正在尋找運作中的進程: " + std::string(targetExeName));
                DWORD pid = GetPidByProcessName(targetExeName);

                if (pid == 0) {
                    LogToConsole("[-] 失敗: 找不到該目標。請確認目標 EXE 目前「正在執行中」。");
                } else {
                    LogToConsole("[+] 成功尋找到目標！PID 進程編號: " + std::to_string(pid));
                    LogToConsole("[*] 啟動遠端記憶體配置與注入程序...");
                    if (ExecuteInjection(pid, dllPath)) {
                        LogToConsole("[+] 【完美成功】DLL 已順利寫入該程序中！");
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
    const char* className = "InjectorUiClass";
    
    hBrushDarkGrey = CreateSolidBrush(COLOR_DARK_GREY);

    Wcx.cbSize = sizeof(WNDCLASSEXA);
    Wcx.lpfnWndProc = WindowProcedure;
    Wcx.hInstance = hInst;
    Wcx.lpszClassName = className;
    hBrushDarkGrey = CreateSolidBrush(COLOR_DARK_GREY);
    Wcx.hbrBackground = hBrushDarkGrey;
    Wcx.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassExA(&Wcx)) return 0;

    HWND hwnd = CreateWindowExA(0, className, "圖形化 DLL 注入器", 
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE, 
                                CW_USEDEFAULT, CW_USEDEFAULT, 475, 385, NULL, NULL, hInst, NULL);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
