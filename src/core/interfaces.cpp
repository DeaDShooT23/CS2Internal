#include "interfaces.hpp"

#include <stdexcept>
#include <format>
#include <fstream>
#include <string>

namespace interfaces {

    // Einfache Log-Funktion für die Interfaces
    void LogInt(const std::string& message) {
        std::ofstream log_file("cs2_cheat_log.txt", std::ios::app);
        if (log_file.is_open()) {
            log_file << message << std::endl;
        }
    }

    template <class T>
    T* capture_interface(const char* module_name, const char* interface_name) {
        // Sicherstellen, dass die Pointer nicht null sind, um Warnung C6387 zu vermeiden
        std::string safe_mod_name = module_name ? module_name : "unbekannt";
        std::string safe_int_name = interface_name ? interface_name : "unbekannt";

        LogInt("[INFO] Suche Modul: " + safe_mod_name);
        const HMODULE module_handle = GetModuleHandleA(module_name);
        if (module_handle == nullptr) {
            LogInt("[FEHLER] Konnte Handle nicht bekommen für Modul: " + safe_mod_name);
            throw std::runtime_error(
                std::format("failed to get handle for module \"{}\"", safe_mod_name));
        }

        using create_interface_fn = T* (*)(const char*, int*);
        const auto create_interface =
            reinterpret_cast<create_interface_fn>(GetProcAddress(module_handle, "CreateInterface"));
        if (create_interface == nullptr) {
            LogInt("[FEHLER] CreateInterface nicht gefunden in: " + safe_mod_name);
            throw std::runtime_error(std::format(
                "failed to get CreateInterface address from module \"{}\"", safe_mod_name));
        }

        LogInt("[INFO] Capture Interface: " + safe_int_name);
        T* interface_ptr = create_interface(interface_name, nullptr);
        if (interface_ptr == nullptr) {
            LogInt("[FEHLER] Interface nicht gefunden: " + safe_int_name);
            throw std::runtime_error(std::format("failed to capture interface \"{}\" from \"{}\"",
                                                 safe_int_name, safe_mod_name));
        }

        LogInt("[OK] Interface erfolgreich: " + safe_int_name);
        return interface_ptr;
    }

    static void create_d3d11_resources() {
        LogInt("[INFO] Starte create_d3d11_resources()...");
        {
            LogInt("[INFO] Suche Pattern für swap_chain_dx11 in rendersystemdx11.dll...");
            std::uint8_t* address =
                sdk::find_pattern("rendersystemdx11.dll", "48 89 2D ? ? ? ? 66 0F 7F");

            if (!address) {
                LogInt("[FEHLER] Pattern für swap_chain_dx11 NICHT gefunden! Veraltet?");
                throw std::runtime_error("failed to find swap_chain_dx11 pattern");
            }
            LogInt("[INFO] Pattern swap_chain_dx11 gefunden. Löse RIP auf...");

            swap_chain_dx11 = **reinterpret_cast<sdk::interface_swap_chain_dx11***>(
                sdk::resolve_absolute_rip_address(address, 3, 7));

            if (swap_chain_dx11 == nullptr) {
                LogInt("[FEHLER] swap_chain_dx11 ist nach RIP-Auflösung nullptr!");
                throw std::runtime_error("failed to capture interface_swap_chain_dx11");
            }
            LogInt("[OK] swap_chain_dx11 erfolgreich gecaptured.");
        }

        LogInt("[INFO] Prüfe swap_chain Padding...");
        if (swap_chain_dx11->swap_chain == nullptr) {
            LogInt("[FEHLER] swap_chain_dx11->swap_chain ist nullptr. Padding veraltet!");
            throw std::runtime_error("swap_chain_dx11 padding is outdated.");
        }

        IDXGISwapChain* swap_chain = swap_chain_dx11->swap_chain;

        LogInt("[INFO] Hole D3D11Device...");
        if (FAILED(swap_chain->GetDevice(__uuidof(ID3D11Device),
                                         reinterpret_cast<void**>(&d3d11_device)))) {
            LogInt("[FEHLER] GetDevice fehlgeschlagen.");
            throw std::runtime_error("failed to get d3d11 device from swap chain");
        }

        if (d3d11_device == nullptr) {
            throw std::runtime_error("d3d11 device is null");
        }

        LogInt("[INFO] Hole ImmediateContext...");
        d3d11_device->GetImmediateContext(&d3d11_device_context);

        if (d3d11_device_context == nullptr) {
            throw std::runtime_error("d3d11 device context is null");
        }

        {
            LogInt("[INFO] Hole HWND aus SwapChain-Desc...");
            DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
            if (FAILED(swap_chain->GetDesc(&swap_chain_desc))) {
                throw std::runtime_error("failed to get swap chain description");
            }

            hwnd = swap_chain_desc.OutputWindow;
            if (hwnd == nullptr) {
                LogInt("[FEHLER] hwnd ist nullptr!");
                throw std::runtime_error("hwnd is null");
            }
        }

        LogInt("[INFO] Erstelle Render Target...");
        create_render_target();
        LogInt("[OK] create_d3d11_resources() abgeschlossen.");
    }

    void create() {
        LogInt("--------------------------------------");
        LogInt("[INFO] interfaces::create() gestartet.");
        {
            LogInt("[INFO] Suche Pattern für csgo_input in client.dll...");
            std::uint8_t* address =
                sdk::find_pattern("client.dll", "48 8B 0D ? ? ? ? 4C 8D 8F ? ? ? ? 45 33 F6");

            if (!address) {
                LogInt("[FATALER FEHLER] Pattern für csgo_input NICHT gefunden! Crash verhindert.");
                throw std::runtime_error("pattern for csgo_input not found");
            }

            LogInt("[INFO] Pattern für csgo_input gefunden. Löse RIP auf...");
            csgo_input = *reinterpret_cast<sdk::interface_csgo_input**>(
                sdk::resolve_absolute_rip_address(address, 3, 7));

            if (csgo_input == nullptr) {
                LogInt("[FEHLER] csgo_input ist nullptr!");
                throw std::runtime_error("failed to capture interface_csgo_input");
            }
            LogInt("[OK] csgo_input gefunden.");
        }

        LogInt("[INFO] Hole InputSystemVersion001...");
        input_system = capture_interface<sdk::interface_input_system>("inputsystem.dll",
                                                                      "InputSystemVersion001");

        create_d3d11_resources();
        LogInt("[OK] interfaces::create() erfolgreich!");
    }

    void destroy() {
        destroy_render_target();

        if (d3d11_device_context != nullptr) {
            d3d11_device_context->Release();
            d3d11_device_context = nullptr;
        }

        if (d3d11_device != nullptr) {
            d3d11_device->Release();
            d3d11_device = nullptr;
        }

        swap_chain_dx11 = nullptr;
        input_system = nullptr;
        csgo_input = nullptr;
        hwnd = nullptr;
    }

    void create_render_target() {
        if (!d3d11_device || !swap_chain_dx11 || !swap_chain_dx11->swap_chain) {
            throw std::runtime_error("d3d11 device or swap chain is null");
        }

        ID3D11Texture2D* back_buffer = nullptr;
        if (FAILED(swap_chain_dx11->swap_chain->GetBuffer(
                0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer)))) {
            throw std::runtime_error("failed to get back buffer from swap chain");
        }

        if (back_buffer == nullptr) {
            throw std::runtime_error("back buffer is null");
        }

        if (FAILED(d3d11_device->CreateRenderTargetView(back_buffer, nullptr,
                                                        &d3d11_render_target_view))) {
            back_buffer->Release();
            throw std::runtime_error("failed to create render target view from back buffer");
        }

        back_buffer->Release();

        if (d3d11_render_target_view == nullptr) {
            throw std::runtime_error("render target view is null");
        }
    }

    void destroy_render_target() {
        if (d3d11_render_target_view != nullptr) {
            d3d11_render_target_view->Release();
            d3d11_render_target_view = nullptr;
        }
    }

}  // namespace interfaces