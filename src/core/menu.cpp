#include "menu.hpp"

#include "globals.hpp"
#include "hooks.hpp"
#include "interfaces.hpp"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui/imgui_impl_win32.h>

#include <stdexcept>

static WNDPROC original_wndproc = nullptr;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT __stdcall hook_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_KEYDOWN && wparam == VK_INSERT) {
        globals::menu_opened = !globals::menu_opened;

        // Die Maus wird für das Menü befreit oder wieder an CS2 übergeben
        hooks::original_set_relative_mouse_mode(
            interfaces::input_system, globals::menu_opened ? false : globals::relative_mouse_mode);
    }

    // WICHTIG: ImGui muss IMMER mitlesen dürfen, um "Taste losgelassen" Events nicht zu verpassen
    ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);

    // Wenn das Menü offen ist, fangen wir die Eingaben ab, damit der Spieler im Hintergrund nicht
    // schießt/läuft
    if (globals::menu_opened) {
        // Ein paar wichtige Nachrichten müssen trotzdem durch, damit das Spiel nicht einfriert
        if (msg == WM_KEYUP || msg == WM_SYSKEYUP) {
            return CallWindowProcA(original_wndproc, hwnd, msg, wparam, lparam);
        }
        return true;
    }

    // Wenn das Menü ZU ist, geht jeder Input 1:1 an CS2 -> Du kannst laufen und zielen!
    return CallWindowProcA(original_wndproc, hwnd, msg, wparam, lparam);
}

namespace menu {
    void create() {
        if (!interfaces::d3d11_device || !interfaces::d3d11_device_context || !interfaces::hwnd) {
            throw std::runtime_error("interfaces not initialized");
        }

        original_wndproc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(
            interfaces::hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hook_wndproc)));

        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

        ImGui::StyleColorsDark();

        if (!ImGui_ImplWin32_Init(interfaces::hwnd)) {
            throw std::runtime_error("Failed to initialize ImGui Win32 implementation");
        }

        if (!ImGui_ImplDX11_Init(interfaces::d3d11_device, interfaces::d3d11_device_context)) {
            throw std::runtime_error("Failed to initialize ImGui DX11 implementation");
        }
    }

    void destroy() {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (original_wndproc) {
            SetWindowLongPtrA(interfaces::hwnd, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(original_wndproc));
            original_wndproc = nullptr;
        }
    }

    void render() {
        if (!globals::menu_opened) {
            return;
        }

        // Setzt die Startgröße des Fensters (nur beim ersten Öffnen)
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

        // Erstellt das Hauptfenster
        if (ImGui::Begin("CS2 Internal Cheat - made with ImGui", &globals::menu_opened)) {
            // Startet die Tab-Leiste (Reiter-Menü)
            if (ImGui::BeginTabBar("CheatTabs")) {
                // --- REITER 1: AIMBOT ---
                if (ImGui::BeginTabItem("Aimbot")) {
                    ImGui::Text("Hier werden wir bald den Aimbot einbauen!");
                    ImGui::Separator();  // Zieht eine schöne optische Trennlinie

                    // 'static' sorgt dafür, dass ImGui sich den Zustand der Checkbox merkt
                    static bool aimbot_enabled = false;
                    ImGui::Checkbox("Aimbot aktivieren (Dummy)", &aimbot_enabled);
                    ImGui::EndTabItem();
                }

                // --- REITER 2: VISUALS (ESP) ---
                if (ImGui::BeginTabItem("Visuals")) {
                    ImGui::Text("Hier kommen die Wallhack/ESP Einstellungen hin!");
                    ImGui::Separator();

                    ImGui::Checkbox("Gegner-Boxen zeichnen (Dummy)", &globals::esp_enabled);
                    ImGui::EndTabItem();
                }

                // --- REITER 3: MISC ---
                if (ImGui::BeginTabItem("Misc")) {
                    ImGui::Text("Hier landen sonstige Funktionen.");
                    ImGui::Separator();

                    static bool bhop_enabled = false;
                    ImGui::Checkbox("Bunnyhop aktivieren (Dummy)", &bhop_enabled);
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();  // Beendet die Tab-Leiste
            }
        }
        ImGui::End();  // Beendet das ImGui Fenster
    }
}  // namespace menu