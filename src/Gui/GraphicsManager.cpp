#include "GraphicsManager.h"
#include <dwmapi.h>
#include "Extern/ImGui/imgui_impl_win32.h"
#include "Extern/ImGui/imgui_impl_dx11.h"
#include "Extern/ImGui/imgui_internal.h"

void GraphicsManager::init() {
	createOverlayWindow();
	initD3D();
	initImGui();
}

void GraphicsManager::destroy() {
	destroyImGui();
	destroyD3D();
	destroyOverlayWindow();
}

void GraphicsManager::render() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    GUI::getGUI().render();
    ImGui::Render();

    ImVec4 clearColor = ImVec4(0.f, 0.f, 0.f, 0.f);
    const float clearColorWithAlpha[4] = { clearColor.x * clearColor.w, clearColor.y * clearColor.w, clearColor.z * clearColor.w, clearColor.w };
    _pd3dDeviceContext->OMSetRenderTargets(1, &_mainRenderTargetView, nullptr);
    _pd3dDeviceContext->ClearRenderTargetView(_mainRenderTargetView, clearColorWithAlpha);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    if (io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
    _pSwapChain->Present(1, 0);
}

bool GraphicsManager::proccessWndMsg() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.message == WM_QUIT;
}

void GraphicsManager::createOverlayWindow() {
    HICON hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(101));
	_wc = { sizeof(_wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), hIcon, nullptr, nullptr, nullptr, _overlayWindowName, nullptr};
	RegisterClassExW(&_wc);
	_overlayHWND = CreateWindowExW(WS_EX_LAYERED,_wc.lpszClassName, _overlayWindowName, WS_POPUP, 0, 0, 1, 1,
		nullptr, nullptr, _wc.hInstance, nullptr);

    SetWindowLongW(_overlayHWND, GWL_EXSTYLE, GetWindowLong(_overlayHWND, GWL_EXSTYLE) & ~WS_EX_APPWINDOW | WS_EX_NOACTIVATE);
	ShowWindow(_overlayHWND, SW_SHOW);
	UpdateWindow(_overlayHWND);
    SetLayeredWindowAttributes(_overlayHWND, 0, BYTE(255), LWA_ALPHA);
    RECT clientArea{};
    GetClientRect(_overlayHWND, &clientArea);
    RECT windowArea{};
    GetWindowRect(_overlayHWND, &windowArea);
    POINT diff{};
    ClientToScreen(_overlayHWND, &diff);
    const MARGINS margins{ windowArea.left + (diff.x - windowArea.left), windowArea.top + (diff.y - windowArea.top),windowArea.right, windowArea.bottom };
    DwmExtendFrameIntoClientArea(_overlayHWND, &margins);
    SetWindowLongW(_overlayHWND, GWL_EXSTYLE, GetWindowLong(_overlayHWND, GWL_EXSTYLE) | WS_EX_TRANSPARENT | WS_EX_LAYERED);
}

void GraphicsManager::destroyOverlayWindow() {
	DestroyWindow(_overlayHWND);
	UnregisterClassW(_wc.lpszClassName, _wc.hInstance);
}

void GraphicsManager::initD3D() {
    DXGI_SWAP_CHAIN_DESC sd = { 0 };
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = _overlayHWND;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, 
        D3D11_SDK_VERSION, &sd, &_pSwapChain, &_pd3dDevice, &featureLevel, &_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) {
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, 
            D3D11_SDK_VERSION, &sd, &_pSwapChain, &_pd3dDevice, &featureLevel, &_pd3dDeviceContext);
    }
    if (res != S_OK) {
        MessageBox(_overlayHWND, L"Ошибка инициализации DirectX (Не удалось создать SwapChain)", L"Emulator init error", MB_OK | MB_ICONERROR);
        exit(1);
    }

    ID3D11Texture2D* pBackBuffer = nullptr;
    _pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        _pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &_mainRenderTargetView);
        pBackBuffer->Release();
    }
    else {
        MessageBox(_overlayHWND, L"Ошибка инициализации DirectX (Задний буфер отсутствует)", L"Emulator init error", MB_OK | MB_ICONERROR);
        destroy();
        exit(1);
    }
}

void GraphicsManager::destroyD3D() {
    if (_mainRenderTargetView) { _mainRenderTargetView->Release(); _mainRenderTargetView = nullptr; }
    if (_pSwapChain) { _pSwapChain->Release(); _pSwapChain = nullptr; }
    if (_pd3dDeviceContext) { _pd3dDeviceContext->Release(); _pd3dDeviceContext = nullptr; }
    if (_pd3dDevice) { _pd3dDevice->Release(); _pd3dDevice = nullptr; }
}

void GraphicsManager::initImGui() {
    ImGui::CreateContext();
    io = &ImGui::GetIO();
    io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable | ImGuiConfigFlags_DockingEnable;
    ImGui_ImplWin32_Init(_overlayHWND);
    ImGui_ImplDX11_Init(_pd3dDevice, _pd3dDeviceContext);
    setupStyles();
    //setupFonts();
}

void GraphicsManager::destroyImGui() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    io = nullptr;
}

void GraphicsManager::setupStyles() {
    ImGui::StyleColorsDark();
    ImGuiStyle& styles = ImGui::GetStyle();


    auto& colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_WindowBg] = ImVec4{ 0.1f, 0.1f, 0.13f, 1.0f };
    colors[ImGuiCol_MenuBarBg] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };

    // Border
    colors[ImGuiCol_Border] = ImVec4{ 0.44f, 0.37f, 0.61f, 0.29f };
    colors[ImGuiCol_BorderShadow] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.24f };

    // Text
    colors[ImGuiCol_Text] = ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f };
    colors[ImGuiCol_TextDisabled] = ImVec4{ 0.5f, 0.5f, 0.5f, 1.0f };

    // Headers
    colors[ImGuiCol_Header] = ImVec4{ 0.13f, 0.13f, 0.17, 1.0f };
    colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.19f, 0.2f, 0.25f, 1.0f };
    colors[ImGuiCol_HeaderActive] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };

    // Buttons
    colors[ImGuiCol_Button] = ImVec4{ 0.13f, 0.13f, 0.17, 1.0f };
    colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.19f, 0.2f, 0.25f, 1.0f };
    colors[ImGuiCol_ButtonActive] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
    colors[ImGuiCol_CheckMark] = ImVec4{ 0.74f, 0.58f, 0.98f, 1.0f };

    // Popups
    colors[ImGuiCol_PopupBg] = ImVec4{ 0.1f, 0.1f, 0.13f, 0.92f };

    // Slider
    colors[ImGuiCol_SliderGrab] = ImVec4{ 0.44f, 0.37f, 0.61f, 0.54f };
    colors[ImGuiCol_SliderGrabActive] = ImVec4{ 0.74f, 0.58f, 0.98f, 0.54f };

    // Frame BG
    colors[ImGuiCol_FrameBg] = ImVec4{ 0.13f, 0.13, 0.17, 1.0f };
    colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.19f, 0.2f, 0.25f, 1.0f };
    colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
    colors[ImGuiCol_TabHovered] = ImVec4{ 0.24, 0.24f, 0.32f, 1.0f };
    colors[ImGuiCol_TabActive] = ImVec4{ 0.2f, 0.22f, 0.27f, 1.0f };
    colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };

    // Title
    colors[ImGuiCol_TitleBg] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
    colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = ImVec4{ 0.1f, 0.1f, 0.13f, 1.0f };
    colors[ImGuiCol_ScrollbarGrab] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4{ 0.19f, 0.2f, 0.25f, 1.0f };
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4{ 0.24f, 0.24f, 0.32f, 1.0f };

    // Seperator
    colors[ImGuiCol_Separator] = ImVec4{ 0.44f, 0.37f, 0.61f, 1.0f };
    colors[ImGuiCol_SeparatorHovered] = ImVec4{ 0.74f, 0.58f, 0.98f, 1.0f };
    colors[ImGuiCol_SeparatorActive] = ImVec4{ 0.84f, 0.58f, 1.0f, 1.0f };

    // Resize Grip
    colors[ImGuiCol_ResizeGrip] = ImVec4{ 0.44f, 0.37f, 0.61f, 0.29f };
    colors[ImGuiCol_ResizeGripHovered] = ImVec4{ 0.74f, 0.58f, 0.98f, 0.29f };
    colors[ImGuiCol_ResizeGripActive] = ImVec4{ 0.84f, 0.58f, 1.0f, 0.29f };

    // Docking
    colors[ImGuiCol_DockingPreview] = ImVec4{ 0.44f, 0.37f, 0.61f, 1.0f };

    auto& style = ImGui::GetStyle();
    style.TabRounding = 4;
    style.ScrollbarRounding = 9;
    style.WindowRounding = 7;
    style.GrabRounding = 3;
    style.FrameRounding = 3;
    style.PopupRounding = 4;
    style.ChildRounding = 4;


    io->Fonts->AddFontFromFileTTF("C:/Windows/Fonts/arial.ttf", 15.f,nullptr, io->Fonts->GetGlyphRangesCyrillic());

    styles.FrameRounding = 3.f;
    styles.WindowRounding = 10.f;
    styles.GrabRounding = 12.f;
    styles.PopupRounding = 10.f;
    styles.ChildRounding = 0.f;
    styles.TabRounding = 8.f;
    styles.WindowTitleAlign.x = 0.5f;
    styles.SeparatorTextAlign.x = 0.5f;
    //styles.Colors[ImGuiCol_WindowBg] = { 93,188,255,200 };
}

LRESULT WINAPI GraphicsManager::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WM_LBUTTONDOWN;
    extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        //windowData::g_ResizeWidth = (UINT)LOWORD(lParam);
        //windowData::g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}