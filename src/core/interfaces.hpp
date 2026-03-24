#pragma once
#include <d3d11.h>

#include "../sdk/interfaces/csgo_input.hpp"
#include "../sdk/interfaces/input_system.hpp"
#include "../sdk/interfaces/swap_chain_dx11.hpp"

namespace interfaces {
	void create();
	void destroy();

	inline sdk::interface_csgo_input* csgo_input = nullptr;
	inline sdk::interface_input_system* input_system = nullptr;
	inline sdk::interface_swap_chain_dx11* swap_chain_dx11 = nullptr;
	
	// Directx11 Specific.
	inline ID3D11Device* d3d11_device = nullptr;
	inline ID3D11DeviceContext* d3d11_device_context = nullptr;
	inline ID3D11RenderTargetView* d3d11_render_target_view = nullptr;

	void create_render_target();
	void destroy_render_target();

	// Windows Specific.
	inline HWND hwnd = nullptr;
}  // namespace interfaces
