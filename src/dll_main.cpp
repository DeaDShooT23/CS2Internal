#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdexcept>
#include <fstream>
#include <string>

#include "core/interfaces.hpp"
#include "core/hooks.hpp"
#include "core/menu.hpp"

// Globale Log-Funktion für die dll_main
void LogMain(const std::string& message) {
    std::ofstream log_file("cs2_cheat_log.txt", std::ios::app);
    if (log_file.is_open()) {
        log_file << message << std::endl;
        log_file.close();
    }
}

DWORD WINAPI cheat_thread(LPVOID instance) {
    LogMain("======================================");
    LogMain("[START] Cheat Thread wurde gestartet!");

    try {
        LogMain("[INFO] Starte interfaces::create()...");
        interfaces::create();
        LogMain("[OK] interfaces::create() erfolgreich!");

        LogMain("[INFO] Starte menu::create()...");
        menu::create();
        LogMain("[OK] menu::create() erfolgreich!");

        LogMain("[INFO] Starte hooks::create()...");
        hooks::create();
        LogMain("[OK] hooks::create() erfolgreich!");
    } catch (const std::exception& e) {
        LogMain(std::string("[FEHLER] Exception gefangen: ") + e.what());
        hooks::destroy();
        menu::destroy();
        interfaces::destroy();
        MessageBoxA(nullptr, e.what(), "Error", MB_ICONERROR);
        FreeLibraryAndExitThread(reinterpret_cast<HMODULE>(instance), 1);
    } catch (...) {
        // Fängt andere, nicht-standardmäßige Fehler ab
        LogMain("[FATALER FEHLER] Unbekannter Crash im Thread!");
    }

    while (!(GetAsyncKeyState(VK_END) & 1)) {
        Sleep(100);
    }

    LogMain("[INFO] Cheat wird beendet (END-Taste gedrückt).");

    hooks::destroy();
    menu::destroy();
    interfaces::destroy();

    MessageBoxA(nullptr, "Cheat wird beendet...", "Info", MB_ICONINFORMATION);
    FreeLibraryAndExitThread(reinterpret_cast<HMODULE>(instance), 0);
    return 0;
}

DWORD APIENTRY DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        HANDLE thread = CreateThread(nullptr, 0, cheat_thread, instance, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}