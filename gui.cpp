// main.cpp
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <d3d11.h>
#include <tchar.h>
#include <fstream>
#include <string>

// Link libs
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// Forward declare Win32 stuff
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// D3D globals
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// ------------------------------------------------------------
// YOUR DATA STRUCTURES
// ------------------------------------------------------------

struct Cell {
    char type;
};

static const int MAP_W = 50;
static const int MAP_H = 50;
Cell mapGrid[MAP_H][MAP_W];

struct RoverState {
    int x = 0;
    int y = 0;
    int battery = 100;
    int collectedGreen = 0;
    int collectedYellow = 0;
    int collectedBlue = 0;
};

RoverState rover;

// ------------------------------------------------------------
// YOUR MAP LOADING
// ------------------------------------------------------------

bool LoadMap(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;

    for (int y = 0; y < MAP_H; y++) {
        std::string line;
        std::getline(file, line);
        if (line.size() < MAP_W) return false;

        for (int x = 0; x < MAP_W; x++) {
            mapGrid[y][x].type = line[x];
            if (line[x] == 'S') {
                rover.x = x;
                rover.y = y;
            }
        }
    }
    return true;
}

// ------------------------------------------------------------
// YOUR DRAW FUNCTIONS
// ------------------------------------------------------------

void DrawMap() {
    ImGui::Begin("Map");

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    float cellW = canvasSize.x / MAP_W;
    float cellH = canvasSize.y / MAP_H;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            char c = mapGrid[y][x].type;

            ImU32 color;
            switch (c) {
                case '#': color = IM_COL32(80, 80, 80, 255); break;
                case 'G': color = IM_COL32(0, 255, 0, 255); break;
                case 'Y': color = IM_COL32(255, 255, 0, 255); break;
                case 'B': color = IM_COL32(0, 128, 255, 255); break;
                default:  color = IM_COL32(40, 40, 40, 255); break;
            }

            if (x == rover.x && y == rover.y)
                color = IM_COL32(255, 0, 0, 255);

            ImVec2 p0 = ImVec2(canvasPos.x + x * cellW, canvasPos.y + y * cellH);
            ImVec2 p1 = ImVec2(canvasPos.x + (x+1)*cellW, canvasPos.y + (y+1)*cellH);

            drawList->AddRectFilled(p0, p1, color);
            drawList->AddRect(p0, p1, IM_COL32(20,20,20,255));
        }
    }

    ImGui::End();
}

void DrawRoverPanel() {
    ImGui::Begin("Rover Data");

    ImGui::Text("Position: (%d, %d)", rover.x, rover.y);
    ImGui::Text("Battery: %d%%", rover.battery);

    ImGui::Separator();
    ImGui::Text("Collected Diamonds:");
    ImGui::BulletText("Green:  %d", rover.collectedGreen);
    ImGui::BulletText("Yellow: %d", rover.collectedYellow);
    ImGui::BulletText("Blue:   %d", rover.collectedBlue);

    ImGui::End();
}

void RenderUI() {
    ImGui::Begin("MainWindow", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize);

    float totalWidth = ImGui::GetContentRegionAvail().x;
    float mapWidth   = totalWidth * 0.66f;
    float dataWidth  = totalWidth * 0.34f;

    ImGui::BeginChild("MapRegion", ImVec2(mapWidth, 0), true);
    DrawMap();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("DataRegion", ImVec2(dataWidth, 0), true);
    DrawRoverPanel();
    ImGui::EndChild();

    ImGui::End();
}

// ------------------------------------------------------------
// D3D / WIN32 HELPERS (from ImGui example)
// ------------------------------------------------------------

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[1] = { D3D_FEATURE_LEVEL_11_0 };
    if (D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 1, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
        &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
    return true;
}

void CleanupDeviceD3D() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
    if (g_pSwapChain)           { g_pSwapChain->Release();           g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext)    { g_pd3dDeviceContext->Release();    g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice)           { g_pd3dDevice->Release();           g_pd3dDevice = nullptr; }
}

// ------------------------------------------------------------
// WIN32 ENTRY
// ------------------------------------------------------------

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            ID3D11Texture2D* pBackBuffer;
            g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
            pBackBuffer->Release();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // Register class
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
                      GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                      _T("ImGuiExample"), NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, _T("Rover GUI"),
                             WS_OVERLAPPEDWINDOW, 100, 100, 1280, 720,
                             NULL, NULL, wc.hInstance, NULL);

    // Create D3D device
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    LoadMap("mars_map_50x50.csv");

    // Main loop
    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                done = true;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (done) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RenderUI();

        ImGui::Render();
        const float clear_color[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return 0;
}
