#include <windows.h>
#include <cmath>

// Visual loop routine that animates the target window's position coordinates
DWORD WINAPI ChaoticWindowThread(LPVOID lpParam) {
    // Brief rest period allowing the injection thread cycle to settle completely
    Sleep(600);

    // Grab the active window handle belonging to the injected host process space
    HWND hwndTarget = GetForegroundWindow();
    
    if (hwndTarget) {
        RECT rectOriginal;
        GetWindowRect(hwndTarget, &rectOriginal);

        int originalX = rectOriginal.left;
        int originalY = rectOriginal.top;
        int width     = rectOriginal.right - rectOriginal.left;
        int height    = rectOriginal.bottom - rectOriginal.top;

        // Visual loop cycle parameter variables
        double angle = 0.0;
        int loopCounter = 0;
        const int totalCycles = 150; // Runs for roughly 15 seconds before restoring stability
        const int intensity   = 40;  // Pixel throw distance modifier

        while (loopCounter < totalCycles) {
            // Apply trigonometry functions to compute chaotic circular path offsets
            int offsetX = static_cast<int>(sin(angle) * intensity);
            int offsetY = static_cast<int>(cos(angle * 1.5) * intensity);

            // Directly override target window layout positioning matrices
            MoveWindow(hwndTarget, originalX + offsetX, originalY + offsetY, width, height, TRUE);

            // Increment movement calculation frames
            angle += 0.4;
            loopCounter++;

            // Wait 100 milliseconds between frame refreshes to create a rhythmic shake effect
            Sleep(100);
        }

        // Smoothly snap the application window back to its exact initial location coordinate
        MoveWindow(hwndTarget, originalX, originalY, width, height, TRUE);
    }
    return 0;
}

// Native Win32 Dynamic Link Library Entry Point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            // Safely detach execution onto an independent thread vector
            CreateThread(NULL, 0, ChaoticWindowThread, NULL, 0, NULL);
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
