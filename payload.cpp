#include <windows.h>
#include <string>
#include <vector>

struct TargetWindowData {
    DWORD currentHostPid;
    HWND parentHwnd;
    HWND notepadEditHwnd;
};

// Callback to isolate the target windows within the process space
BOOL CALLBACK LocateNotepadWindows(HWND hwnd, LPARAM lParam) {
    TargetWindowData* data = reinterpret_cast<TargetWindowData*>(lParam);
    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);

    if (windowPid == data->currentHostPid && IsWindowVisible(hwnd)) {
        HWND editControl = FindWindowExA(hwnd, NULL, "Edit", NULL);
        if (editControl != NULL) {
            data->parentHwnd = hwnd;
            data->notepadEditHwnd = editControl;
            return FALSE; 
        }
    }
    return TRUE;
}

// Advanced chaos engine running entirely within Notepad's process memory
DWORD WINAPI ExtremeChaosRoutine(LPVOID lpParam) {
    Sleep(500);

    DWORD myPid = GetCurrentProcessId();
    TargetWindowData target = { myPid, NULL, NULL };
    EnumWindows(LocateNotepadWindows, reinterpret_cast<LPARAM>(&target));

    if (target.parentHwnd && target.notepadEditHwnd) {
        // Set Notepad to a Layered Window state so we can manipulate its transparency
        LONG style = GetWindowLongA(target.parentHwnd, GWL_EXSTYLE);
        SetWindowLongA(target.parentHwnd, GWL_EXSTYLE, style | WS_EX_LAYERED);

        // Pre-populate Notepad with a huge block of looping glitch lines
        std::string massiveText = "";
        for (int i = 0; i < 150; ++i) {
            massiveText += "[ SYSTEM MALFUNCTION ] :: OVERRIDING CORE REGISTERS... [ CROWNDRIP HOOKED ]\r\n";
        }
        SendMessageA(target.notepadEditHwnd, WM_SETTEXT, 0, (LPARAM)massiveText.c_str());

        int loopCounter = 0;
        bool invertState = false;

        // Visual Manipulation Loop (Runs for ~15 seconds)
        while (loopCounter < 75) {
            
            // 1. Chaotic Transparency Phase Fluctuation (Fades app out and back in)
            BYTE alphaValue = 50 + static_cast<BYTE>(180.0 * (0.5 + 0.5 * sin(loopCounter * 0.4)));
            SetLayeredWindowAttributes(target.parentHwnd, 0, alphaValue, LWA_ALPHA);

            // 2. Automate Erratic Line Scrolling
            // Alternate scrolling down 5 lines and up 2 lines rapidly
            int scrollLines = (loopCounter % 3 == 0) ? -2 : 5;
            SendMessageA(target.notepadEditHwnd, EM_LINESCROLL, 0, scrollLines);

            // 3. Screen Inversion Matrix (Inverts layout context via safe UI framework flash)
            HDC hdcNotepad = GetDC(target.parentHwnd);
            if (hdcNotepad) {
                RECT rect;
                GetClientRect(target.parentHwnd, &rect);
                // PatBlt with DSTINVERT flips all current pixels on screen to their opposite color map
                PatBlt(hdcNotepad, 0, 0, rect.right, rect.bottom, DSTINVERT);
                ReleaseDC(target.parentHwnd, hdcNotepad);
            }

            // 4. Subtle structural layout shifting
            RECT winRect;
            GetWindowRect(target.parentHwnd, &winRect);
            int shiftX = (loopCounter % 2 == 0) ? 8 : -8;
            int shiftY = (loopCounter % 4 == 0) ? 4 : -4;
            MoveWindow(target.parentHwnd, winRect.left + shiftX, winRect.top + shiftY, 
                       winRect.right - winRect.left, winRect.bottom - winRect.top, TRUE);

            loopCounter++;
            Sleep(200); // Frame delay time
        }

        // --- AUTOMATIC RESTORATION CLEANUP ---
        // Restore full solid opacity
        SetLayeredWindowAttributes(target.parentHwnd, 0, 255, LWA_ALPHA);
        
        // Final clear text notice layout
        std::string finalNotice = "========================================\n"
                                  "     CROWNDRIP EXTRA CHAOS RECOVERY     \n"
                                  "========================================\n\n"
                                  " [+] Status: Visual parameters restored.\n"
                                  " [+] Hook Thread Context: Terminated cleanly.\n";
        SendMessageA(target.notepadEditHwnd, WM_SETTEXT, 0, (LPARAM)finalNotice.c_str());
        
        // Force one final complete redraw so the pixel colors match perfectly again
        InvalidateRect(target.parentHwnd, NULL, TRUE);
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            CreateThread(NULL, 0, ExtremeChaosRoutine, NULL, 0, NULL);
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
