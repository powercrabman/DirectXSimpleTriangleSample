#include "EntryPoint.h"
#include "framework.h"

#include <chrono>
#include <string>

#include <comdef.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <d3dcompiler.inl>
#include <directxtk/SimpleMath.h>     // vcpkg install directxtk
#include <directxtk/SimpleMath.inl>   // vcpkg install directxtk
#include <sstream>
#include <wrl/client.h>
using namespace DirectX;
using namespace DirectX::SimpleMath;
using Microsoft::WRL::ComPtr;

#include <imgui.h>              // vcpkg install imgui
#include <imgui_impl_dx11.h>    // vcpkg install imgui[directx11-binding]
#include <imgui_impl_win32.h>   // vcpkg install imgui[win32-binding]

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct WindowContext
{
    HINSTANCE hInst = NULL;
    HWND      hWnd  = NULL;

    std::wstring title       = L"Test Window";
    std::wstring windowClass = L"Test Window";

    Vector2 windowResolution = { 1280.f, 720.f };

    // timer
    uint32_t prevCount = 0;
    float    deltaTime = 0.f;

    bool isRunning = false;

    bool isKeyDown[256] = {};
};

struct Vertex
{
    Vector2 posL;
    Vector3 color;
};

struct __declspec(align(16)) ConstantBuffer
{
    Matrix world;   // 64 bytes
};

struct D3DRenderer
{
    ComPtr<ID3D11Device>        device;      // factory
    ComPtr<ID3D11DeviceContext> context;     // draw
    ComPtr<IDXGISwapChain>      swapChain;   // back buffer

    ComPtr<ID3D11RenderTargetView> renderTargetView;   // back buffer
    D3D11_VIEWPORT                 viewport;           // draw

    ComPtr<ID3D11VertexShader> vertexShader;   // vertex shader
    ComPtr<ID3D11PixelShader>  pixelShader;    // pixel shader
    ComPtr<ID3D11InputLayout>  inputLayout;    // input layout

    ComPtr<ID3D11Buffer> vertexBuffer;
    uint32_t             vertexStride;
    uint32_t             vertexOffset;

    ComPtr<ID3D11Buffer> indexBuffer;
    uint32_t             indexCount;

    ComPtr<ID3D11Buffer> constantBuffer;
    ConstantBuffer       cpuConstantData;

    Vector2 triPosition = { 0.f, 0.f };
    Vector2 triScale    = { 1.f, 1.f };
    float   triRotation = 0.f;
};

Vertex g_triangleVertices[] = {
    Vertex { Vector2 { -0.5f, 0.f }, Vector3 { 1.f, 0.f, 0.f } },
    Vertex { Vector2 { 0.f, 0.5f }, Vector3 { 0.f, 0.f, 1.f } },
    Vertex { Vector2 { 0.5f, 0.f }, Vector3 { 0.f, 1.f, 0.f } }
};

uint32_t g_triangleIndices[] = { 0, 1, 2 };

WindowContext g_windowContext = {};
D3DRenderer   g_renderer      = {};

bool             Init();
bool             InitD3D();
bool             InitImgui();
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
bool             D3DCheckFail(HRESULT hr, const wchar_t* msg);
bool             UpdateConstantBuffer(void* data, size_t size, ComPtr<ID3D11Buffer>& buffer);
void             RenderImgui();

int APIENTRY wWinMain(_In_ HINSTANCE     hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR        lpCmdLine,
                      _In_ int           nCmdShow)
{
    if (!Init())
    {
        OutputDebugStringA("Init failed\n");
        return -1;
    }

    if (!InitD3D())
    {
        OutputDebugStringA("InitD3D failed\n");
        return -1;
    }

    if (!InitImgui())
    {
        OutputDebugStringA("InitImgui failed\n");
        return -1;
    }

    MSG msg = {};

    while (true)
    {
        if (!g_windowContext.isRunning)
            break;

        if (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;

            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
        else
        {
            uint32_t currentCount = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::high_resolution_clock::now().time_since_epoch())
                                        .count();
            g_windowContext.deltaTime = (currentCount - g_windowContext.prevCount) / 1000.f;
            g_windowContext.prevCount = currentCount;

            // Update
            {
                auto& r = g_renderer;

                // update constantBuffer
                r.cpuConstantData.world =
                    Matrix::CreateScale(r.triScale.x, r.triScale.y, 1.f) *
                    Matrix::CreateRotationZ(r.triRotation) *
                    Matrix::CreateTranslation(r.triPosition.x, r.triPosition.y, 0.f);

                UpdateConstantBuffer(
                    &r.cpuConstantData,
                    sizeof(r.cpuConstantData),
                    r.constantBuffer);

                // keyboard input
                if (g_windowContext.isKeyDown['W'])
                    r.triPosition.y += 1.f * g_windowContext.deltaTime;

                if (g_windowContext.isKeyDown['S'])
                    r.triPosition.y -= 1.f * g_windowContext.deltaTime;

                if (g_windowContext.isKeyDown['A'])
                    r.triPosition.x -= 1.f * g_windowContext.deltaTime;

                if (g_windowContext.isKeyDown['D'])
                    r.triPosition.x += 1.f * g_windowContext.deltaTime;

                if (g_windowContext.isKeyDown[VK_UP])
                    r.triScale.y += 1.f * g_windowContext.deltaTime;

                if (g_windowContext.isKeyDown[VK_DOWN])
                    r.triScale.y -= 1.f * g_windowContext.deltaTime;

                if (g_windowContext.isKeyDown[VK_LEFT])
                    r.triScale.x -= 1.f * g_windowContext.deltaTime;

                if (g_windowContext.isKeyDown[VK_RIGHT])
                    r.triScale.x += 1.f * g_windowContext.deltaTime;

                if (g_windowContext.isKeyDown[VK_SPACE])
                    r.triRotation -= 1.f * g_windowContext.deltaTime;

                if (g_windowContext.isKeyDown[VK_CONTROL])
                    r.triRotation += 1.f * g_windowContext.deltaTime;

                if (g_windowContext.isKeyDown[VK_ESCAPE])
                    g_windowContext.isRunning = false;
            }

            // Rendering
            {
                // Input Assembler
                auto& r = g_renderer;
                auto  c = r.context;

                c->IASetInputLayout(r.inputLayout.Get());
                c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                c->IASetVertexBuffers(
                    0,
                    1,
                    r.vertexBuffer.GetAddressOf(),
                    &r.vertexStride,
                    &r.vertexOffset);

                c->IASetIndexBuffer(
                    r.indexBuffer.Get(),
                    DXGI_FORMAT_R32_UINT,
                    0);

                // Vertex Shader
                c->VSSetShader(r.vertexShader.Get(), nullptr, 0);
                c->VSSetConstantBuffers(0, 1, r.constantBuffer.GetAddressOf());

                // Rasterizer
                c->RSSetViewports(1, &r.viewport);

                // Pixel Shader
                c->PSSetShader(g_renderer.pixelShader.Get(), nullptr, 0);

                // Output Merger
                c->OMSetRenderTargets(1, g_renderer.renderTargetView.GetAddressOf(), nullptr);
                FLOAT clearColor[] = { 0.f, 0.f, 0.f, 1.f };
                c->ClearRenderTargetView(g_renderer.renderTargetView.Get(), clearColor);
                c->DrawIndexed(_countof(g_triangleIndices), 0, 0);
            }

            // Render Imgui
            RenderImgui();

            // Present
            g_renderer.swapChain->Present(1, 0);
        }
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();

    OutputDebugStringA("Quit");
    return static_cast<int>(msg.wParam);
}

//
//  함수: MyRegisterClass()
//
//  용도: 창 클래스를 등록합니다.
//
bool Init()
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = WndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = g_windowContext.hInst;
    wcex.hIcon         = nullptr;
    wcex.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1));
    wcex.lpszMenuName  = nullptr;
    wcex.lpszClassName = g_windowContext.windowClass.c_str();
    wcex.hIconSm       = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    if (RegisterClassExW(&wcex) == false)
    {
        OutputDebugStringA("RegisterClassExW failed\n");
        return false;
    }

    HWND hWnd = CreateWindowW(g_windowContext.windowClass.c_str(),
                              g_windowContext.title.c_str(),
                              WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT,
                              0,
                              CW_USEDEFAULT,
                              0,
                              nullptr,
                              nullptr,
                              g_windowContext.hInst,
                              nullptr);

    if (!hWnd)
    {
        OutputDebugStringA("CreateWindowW failed\n");
        return false;
    }

    g_windowContext.hWnd = hWnd;

    RECT windowRect   = {};
    windowRect.right  = g_windowContext.windowResolution.x;
    windowRect.bottom = g_windowContext.windowResolution.y;

    if (!AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE))
    {
        OutputDebugStringA("AdjustWindowRect failed\n");
        return false;
    }

    if (!SetWindowPos(hWnd,
                      nullptr,
                      0,
                      0,
                      windowRect.right - windowRect.left,
                      windowRect.bottom - windowRect.top,
                      SWP_NOMOVE | SWP_SHOWWINDOW))
    {
        OutputDebugStringA("SetWindowPos failed\n");
        return false;
    }

    g_windowContext.isRunning = true;
    std::fill_n(g_windowContext.isKeyDown, _countof(g_windowContext.isKeyDown), false);

    return TRUE;
}

bool InitD3D()
{
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0
    };

    if (D3DCheckFail(
            D3D11CreateDevice(
                nullptr,
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                createDeviceFlags,
                featureLevels,
                _countof(featureLevels),
                D3D11_SDK_VERSION,
                &g_renderer.device,
                nullptr,
                &g_renderer.context),
            L"D3D11CreateDeviceAndSwapChain Fail"))
    {
        return false;
    }

    // Swap Chain

    UINT sampleQuality = 0;
    if (D3DCheckFail(
            g_renderer.device->CheckMultisampleQualityLevels(
                DXGI_FORMAT_R8G8B8A8_UNORM,
                4,
                &sampleQuality),
            L"CheckMultisampleQualityLevels Fail"))
    {
        return false;
    }

    auto [width, height] = g_windowContext.windowResolution;

    DXGI_SWAP_CHAIN_DESC sd               = {};
    sd.BufferCount                        = 1;
    sd.BufferDesc.Width                   = width;
    sd.BufferDesc.Height                  = height;
    sd.Windowed                           = TRUE;
    sd.SampleDesc.Count                   = 4;
    sd.SampleDesc.Quality                 = sampleQuality - 1;
    sd.OutputWindow                       = g_windowContext.hWnd;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags                              = 0;

    ComPtr<IDXGIDevice> dxgiDevice;
    if (D3DCheckFail(
            g_renderer.device.As(&dxgiDevice),
            L"IDXGIDevice Fail"))
    {
        return false;
    }

    ComPtr<IDXGIAdapter> dxgiAdapter;
    if (D3DCheckFail(
            dxgiDevice->GetAdapter(&dxgiAdapter),
            L"IDXGIDevice::GetAdapter Fail"))
    {
        return false;
    }

    ComPtr<IDXGIFactory> dxgiFactory;
    if (D3DCheckFail(
            dxgiAdapter->GetParent(__uuidof(IDXGIFactory), &dxgiFactory),
            L"IDXGIAdapter::GetParent Fail"))
    {
        return false;
    }

    if (D3DCheckFail(
            dxgiFactory->CreateSwapChain(g_renderer.device.Get(), &sd, &g_renderer.swapChain),
            L"CreateSwapChain Fail"))
    {
        return false;
    }

    // Render Target
    ComPtr<ID3D11Texture2D> backBuffer;
    if (D3DCheckFail(
            g_renderer.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer),
            L"GetBuffer Fail"))
    {
        return false;
    }

    g_renderer.viewport = D3D11_VIEWPORT {
        0.f,
        0.f,
        width,
        height,
        0.f,
        1.f
    };

    if (D3DCheckFail(
            g_renderer.device->CreateRenderTargetView(
                backBuffer.Get(),
                nullptr,
                g_renderer.renderTargetView.GetAddressOf()),
            L"CreateRenderTargetView Fail"))
    {
        return false;
    }

    // Vertex Buffer
    {
        D3D11_BUFFER_DESC desc = {};
        ZeroMemory(&desc, sizeof(D3D11_BUFFER_DESC));
        desc.BindFlags           = D3D11_BIND_VERTEX_BUFFER;
        desc.ByteWidth           = sizeof(Vertex) * _countof(g_triangleVertices);
        desc.Usage               = D3D11_USAGE_IMMUTABLE;
        desc.CPUAccessFlags      = 0;
        desc.MiscFlags           = 0;
        desc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem                = g_triangleVertices;

        if (D3DCheckFail(
                g_renderer.device->CreateBuffer(&desc, &initData, g_renderer.vertexBuffer.GetAddressOf()),
                L"CreateBuffer Fail"))
        {
            return false;
        }

        g_renderer.vertexStride = sizeof(Vertex);
        g_renderer.vertexOffset = 0;
    }

    // Index Buffer
    {
        D3D11_BUFFER_DESC desc = {};
        ZeroMemory(&desc, sizeof(D3D11_BUFFER_DESC));
        desc.BindFlags           = D3D11_BIND_INDEX_BUFFER;
        desc.ByteWidth           = sizeof(UINT) * _countof(g_triangleIndices);
        desc.Usage               = D3D11_USAGE_IMMUTABLE;
        desc.CPUAccessFlags      = 0;
        desc.MiscFlags           = 0;
        desc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem                = g_triangleIndices;

        if (D3DCheckFail(
                g_renderer.device->CreateBuffer(&desc, &initData, g_renderer.indexBuffer.GetAddressOf()),
                L"CreateBuffer Fail"))
        {
            return false;
        }

        g_renderer.indexCount = _countof(g_triangleIndices);
    }

    // Constant Buffer
    {
        D3D11_BUFFER_DESC desc = {};
        ZeroMemory(&desc, sizeof(D3D11_BUFFER_DESC));

        desc.BindFlags           = D3D11_BIND_CONSTANT_BUFFER;
        desc.ByteWidth           = sizeof(ConstantBuffer);
        desc.Usage               = D3D11_USAGE_DYNAMIC;
        desc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags           = 0;
        desc.StructureByteStride = 0;

        if (D3DCheckFail(
                g_renderer.device->CreateBuffer(&desc, nullptr, g_renderer.constantBuffer.GetAddressOf()),
                L"CreateBuffer Fail"))
        {
            return false;
        }
    }

    const char* shaderCode = R"(
        cbuffer cb : register(b0)
        {
            row_major matrix world;
        }

        struct VS_INPUT
        {
            float2 posL : POSITION;
            float3 color : COLOR;
        };

        struct PS_INPUT
        {
            float4 posH : SV_POSITION;
            float3 color : COLOR;
        };

        PS_INPUT VSmain(VS_INPUT input)
        {
            PS_INPUT output;
            output.posH = mul(float4(input.posL, 0.f, 1.f), world);
            output.color = input.color;
            return output;
        }

        float4 PSmain(PS_INPUT input) : SV_TARGET
        {
            return float4(input.color, 1.f);
        }
    )";

    // Vertex Shader
    ComPtr<ID3DBlob> shaderBlob;
    if (D3DCheckFail(
            D3DCompile(shaderCode,
                       strlen(shaderCode),
                       nullptr,
                       nullptr,
                       nullptr,
                       "VSmain",
                       "vs_5_0",
                       0,
                       0,
                       &shaderBlob,
                       nullptr),
            L"D3DCompile Fail"))
    {
        return false;
    }

    if (D3DCheckFail(
            g_renderer.device->CreateVertexShader(
                shaderBlob->GetBufferPointer(),
                shaderBlob->GetBufferSize(),
                nullptr,
                g_renderer.vertexShader.GetAddressOf()),
            L"CreateVertexShader Fail"))
    {
        return false;
    }

    // Input Layout
    D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    if (D3DCheckFail(
            g_renderer.device->CreateInputLayout(
                inputDesc,
                _countof(inputDesc),
                shaderBlob->GetBufferPointer(),
                shaderBlob->GetBufferSize(),
                g_renderer.inputLayout.GetAddressOf()),
            L"CreateInputLayout Fail"))
    {
        return false;
    }

    // Pixel Shader
    if (D3DCheckFail(
            D3DCompile(
                shaderCode,
                strlen(shaderCode),
                nullptr,
                nullptr,
                nullptr,
                "PSmain",
                "ps_5_0",
                0,
                0,
                &shaderBlob,
                nullptr),
            L"D3DCompile Fail"))
    {
        return false;
    }

    if (D3DCheckFail(
            g_renderer.device->CreatePixelShader(
                shaderBlob->GetBufferPointer(),
                shaderBlob->GetBufferSize(),
                nullptr,
                g_renderer.pixelShader.GetAddressOf()),
            L"CreatePixelShader Fail"))
    {
        return false;
    }

    return true;
}

bool InitImgui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(g_windowContext.hWnd);
    ImGui_ImplDX11_Init(g_renderer.device.Get(), g_renderer.context.Get());

    return true;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam);

    switch (message)
    {
        case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);

            switch (wmId)
            {
                case IDM_ABOUT:
                    DialogBox(g_windowContext.hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                    break;
                case IDM_EXIT:
                    DestroyWindow(hWnd);
                    break;
                default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);

        case WM_KEYDOWN:
            g_windowContext.isKeyDown[wParam] = true;
            break;

        case WM_KEYUP:
            g_windowContext.isKeyDown[wParam] = false;
            break;
    }
    return 0;
}

// 정보 대화 상자의 메시지 처리기입니다.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
        case WM_INITDIALOG:
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
            {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}

bool D3DCheckFail(HRESULT hr, const wchar_t* msg)
{
    if (FAILED(hr))
    {
        _com_error         err(hr);
        LPCTSTR            errMsg = err.ErrorMessage();
        std::wstringstream wss;
        wss << L"Error: " << errMsg << L"\n"
            << msg << L"\n";
        OutputDebugString(wss.str().c_str());
        return true;
    }
    return false;
}

bool UpdateConstantBuffer(void* data, size_t size, ComPtr<ID3D11Buffer>& buffer)
{
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    if (D3DCheckFail(
            g_renderer.context->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource),
            L"Map Fail"))
    {
        return false;
    }

    memcpy(mappedResource.pData, data, size);

    g_renderer.context->Unmap(buffer.Get(), 0);

    return true;
}

void RenderImgui()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    {
        ImGui::Begin("Triangle");

        ImGui::SliderFloat2("Position", &g_renderer.triPosition.x, -1.f, 1.f);
        ImGui::SliderFloat2("Scale", &g_renderer.triScale.x, 0.f, 1.f);
        ImGui::SliderAngle("Rotation", &g_renderer.triRotation);

        ImGui::Separator();
        ImGui::Text("Delta time: %.3f sec", g_windowContext.deltaTime);
        ImGui::Text("FPS: %.2f", 1 / g_windowContext.deltaTime);

        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}