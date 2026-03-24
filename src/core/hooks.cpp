#include "hooks.hpp"

#include <minhook/MinHook.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui/imgui_impl_win32.h>

#include <stdexcept>
#include <fstream>
#include <string>

#include "menu.hpp"
#include "globals.hpp"
#include "../features/esp.hpp"

// Einfache Log-Funktion, die Text in eine Datei anhängt
void Log(const std::string& message) {
    std::ofstream log_file("cs2_cheat_log.txt", std::ios::app);
    if (log_file.is_open()) {
        log_file << message << std::endl;
        log_file.close();
    }
}

HRESULT __stdcall hook_present(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags) {
    if (!interfaces::d3d11_render_target_view) {
        interfaces::create_render_target();
    }

    if (interfaces::d3d11_device_context) {
        interfaces::d3d11_device_context->OMSetRenderTargets(
            1, &interfaces::d3d11_render_target_view, nullptr);
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    menu::render();
    esp::render();  // <-- HIER RUFEN WIR DEN ESP AUF!

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    return hooks::original_present(interfaces::swap_chain_dx11->swap_chain, sync_interval, flags);
}

HRESULT __stdcall hook_resize_buffers(IDXGISwapChain* swap_chain, UINT buffer_count, UINT width,
                                      UINT height, DXGI_FORMAT new_format, UINT swap_chain_flags) {
    const HRESULT result = hooks::original_resize_buffers(swap_chain, buffer_count, width, height,
                                                          new_format, swap_chain_flags);

    if (SUCCEEDED(result)) {
        interfaces::destroy_render_target();
        ImGui_ImplDX11_CreateDeviceObjects();
    }

    return result;
}

HRESULT __stdcall hook_create_swap_chain(IDXGIFactory* dxgi_factory, IUnknown* device,
                                         DXGI_SWAP_CHAIN_DESC* swap_chain_desc,
                                         IDXGISwapChain** swap_chain) {
    ImGui_ImplDX11_InvalidateDeviceObjects();
    interfaces::destroy_render_target();

    return hooks::original_create_swap_chain(dxgi_factory, device, swap_chain_desc, swap_chain);
}

bool __fastcall hook_mouse_input_enabled(sdk::interface_csgo_input* csgo_input) {
    return globals::menu_opened ? false : hooks::original_mouse_input_enabled(csgo_input);
}

void* __fastcall hook_set_relative_mouse_mode(sdk::interface_input_system* input_system,
                                              bool enabled) {
    globals::relative_mouse_mode = enabled;
    return hooks::original_set_relative_mouse_mode(input_system,
                                                   globals::menu_opened ? false : enabled);
}

namespace hooks {
    void create() {
        Log("======================================");
        Log("[INFO] Starte Initialisierung der Hooks...");

        if (MH_Initialize() != MH_OK) {
            Log("[FEHLER] failed to initialize minhook");
            throw std::runtime_error("failed to initialize minhook");
        }
        Log("[OK] MinHook initialisiert.");

        Log("[INFO] Hooke Present (Index 8)...");
        if (MH_CreateHook(
                sdk::virtual_function_get<void*, 8>(interfaces::swap_chain_dx11->swap_chain),
                &hook_present, reinterpret_cast<void**>(&original_present)) != MH_OK) {
            Log("[FEHLER] failed to create present hook");
            throw std::runtime_error("failed to create present hook");
        }
        Log("[OK] Present Hook erfolgreich.");

        Log("[INFO] Hooke ResizeBuffers (Index 13)...");
        if (MH_CreateHook(
                sdk::virtual_function_get<void*, 13>(interfaces::swap_chain_dx11->swap_chain),
                &hook_resize_buffers,
                reinterpret_cast<void**>(&original_resize_buffers)) != MH_OK) {
            Log("[FEHLER] failed to create resize buffers hook");
            throw std::runtime_error("failed to create resize buffers hook");
        }
        Log("[OK] ResizeBuffers Hook erfolgreich.");

        {
            Log("[INFO] Setup für DXGI Factory (CreateSwapChain Hook)...");
            IDXGIDevice* dxgi_device = nullptr;
            if (FAILED(interfaces::d3d11_device->QueryInterface(
                    __uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgi_device)))) {
                Log("[FEHLER] failed to get dxgi device");
                throw std::runtime_error("failed to get dxgi device from d3d11 device");
            }

            IDXGIAdapter* dxgi_adapter = nullptr;
            if (FAILED(dxgi_device->GetAdapter(&dxgi_adapter))) {
                dxgi_device->Release();
                Log("[FEHLER] failed to get dxgi adapter");
                throw std::runtime_error("failed to get dxgi adapter from dxgi device");
            }

            IDXGIFactory* dxgi_factory = nullptr;
            if (FAILED(dxgi_adapter->GetParent(__uuidof(IDXGIFactory),
                                               reinterpret_cast<void**>(&dxgi_factory)))) {
                dxgi_adapter->Release();
                dxgi_device->Release();
                Log("[FEHLER] failed to get dxgi factory");
                throw std::runtime_error("failed to get dxgi factory from dxgi adapter");
            }

            Log("[INFO] Hooke CreateSwapChain (Index 10)...");
            if (MH_CreateHook(sdk::virtual_function_get<void*, 10>(dxgi_factory),
                              &hook_create_swap_chain,
                              reinterpret_cast<void**>(&original_create_swap_chain)) != MH_OK) {
                dxgi_factory->Release();
                dxgi_adapter->Release();
                dxgi_device->Release();
                Log("[FEHLER] failed to create create swap chain hook");
                throw std::runtime_error("failed to create create swap chain hook");
            }

            dxgi_factory->Release();
            dxgi_adapter->Release();
            dxgi_device->Release();
            Log("[OK] CreateSwapChain Hook erfolgreich.");
        }

        Log("[INFO] Hooke Mouse Input (Index 15)...");
        if (MH_CreateHook(sdk::virtual_function_get<void*, 15>(interfaces::csgo_input),
                          &hook_mouse_input_enabled,
                          reinterpret_cast<void**>(&original_mouse_input_enabled)) != MH_OK) {
            Log("[FEHLER] failed to create mouse input enabled hook");
            throw std::runtime_error("failed to create mouse input enabled hook");
        }
        Log("[OK] Mouse Input Hook erfolgreich.");

        Log("[INFO] Hooke Set Relative Mouse Mode (Index 76)...");
        if (MH_CreateHook(sdk::virtual_function_get<void*, 76>(interfaces::input_system),
                          &hook_set_relative_mouse_mode,
                          reinterpret_cast<void**>(&original_set_relative_mouse_mode)) != MH_OK) {
            Log("[FEHLER] failed to create set relative mouse mode hook");
            throw std::runtime_error("failed to create set relative mouse mode hook");
        }
        Log("[OK] Set Relative Mouse Mode Hook erfolgreich.");

        Log("[INFO] Aktiviere alle Hooks...");
        if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
            Log("[FEHLER] failed to enable minhook hooks");
            throw std::runtime_error("failed to enable minhook hooks");
        }
        Log("[ERFOLG] Alle Hooks aktiviert! Initialisierung abgeschlossen.");
    }

    void destroy() {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_RemoveHook(MH_ALL_HOOKS);

        MH_Uninitialize();
    }
}  // namespace hooks