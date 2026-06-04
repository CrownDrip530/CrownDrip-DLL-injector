#include <windows.h>

// Worker thread routine to avoid blocking the target process execution context
DWORD WINAPI NotificationThread(LPVOID lpParam) {
    // Show a message box indicating successful injection inside the target process space
    MessageBoxA(
        NULL, 
        "Hello from CrownDrip!\n\nYour DLL Injection routine executed perfectly inside this process space.", 
        "CrownDrip Hook Active", 
        MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL
    );
    return 0;
}

// Native Win32 Dynamic Link Library Entry Point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            // Spawn a thread to isolate code execution cleanly away from the primary thread loop
            CreateThread(NULL, 0, NotificationThread, NULL, 0, NULL);
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
