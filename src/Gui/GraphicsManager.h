#pragma once

#include <Windows.h>
#include <d3d11.h>
#include "Extern/ImGui/imgui.h"
#include "GUI.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

class GraphicsManager {
public:
	inline static GraphicsManager& getGraphicsManager() {
		static GraphicsManager graphicsManager;
		return graphicsManager;
	}

	void init();
	void destroy();
	void render();
	bool proccessWndMsg();

	inline HWND getHWND() { return _overlayHWND; };

private:
	GraphicsManager() = default;

	static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void createOverlayWindow();
	void destroyOverlayWindow();
	void initD3D();
	void destroyD3D();
	void initImGui();
	void destroyImGui();
	void setupStyles();

	// Overlay Window vars
	const wchar_t* _overlayWindowName = L"Emulator";
	HWND _overlayHWND = NULL;
	WNDCLASSEXW _wc = {};
	bool _isTransparent = true;

	// Directx vars
	IDXGISwapChain* _pSwapChain = nullptr;
	ID3D11Device* _pd3dDevice = nullptr;
	ID3D11RenderTargetView* _mainRenderTargetView = nullptr;
	ID3D11DeviceContext* _pd3dDeviceContext = nullptr;

	// ImGui vars
	ImGuiIO* io = nullptr;
};