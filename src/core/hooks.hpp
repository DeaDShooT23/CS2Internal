#pragma once
#include "interfaces.hpp"

namespace hooks {
	void create();
	void destroy();

	using function_present = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
	inline function_present original_present = nullptr;

	using function_resize_buffers = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

	inline function_resize_buffers original_resize_buffers = nullptr;

	using function_create_swap_chain = HRESULT(__stdcall*)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);

	inline function_create_swap_chain original_create_swap_chain = nullptr;

	using function_mouse_input_enabled = bool(__thiscall*)(sdk::interface_csgo_input*);
	inline function_mouse_input_enabled original_mouse_input_enabled = nullptr;

	using function_set_relative_mouse_mode = void*(__thiscall*)(sdk::interface_input_system*, bool);
	inline function_set_relative_mouse_mode original_set_relative_mouse_mode = nullptr;

}  // namespace hooks

