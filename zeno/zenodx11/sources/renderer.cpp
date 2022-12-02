#include <zeno.hpp>
#include <assert.h>

namespace zeno::dx11
{
renderer_t::renderer_t(HWND in_window_handle)
    : m_feature_level(D3D_FEATURE_LEVEL_11_1)
{
    //  create DXGI factory to handle DXGI
    m_result = CreateDXGIFactory2(0, IID_IDXGIFactory7, reinterpret_cast<void**>(&m_dxgi.factory));
    if (FAILED(m_result))
        std::cerr << "Failed on CreateDXGIFactory2!\t" << std::hex << m_result << std::endl;

    //  find the most powerful graphics card
    std::vector<IDXGIAdapter4*> adapters;
    get_all_available_adapters(adapters);
    m_dxgi.graphics_card = adapters [0];

    //  get the monitor
    m_result = m_dxgi.graphics_card->EnumOutputs(0, reinterpret_cast<IDXGIOutput**>(&m_dxgi.monitor));

    //  create the device
    if (create_device() == false)
    {
        //  TODO: handle error
    }

    //  take the best resolution available to user
    DXGI_MODE_DESC1 best_display_mode = get_best_display_mode();

    //  create the swapchain
    if (create_swapchain(&best_display_mode, true, in_window_handle) == false)
    {
        //  TODO: handle error
    }

    ID3D11Texture2D1* frame_buffer;
    m_result = m_swapchain->GetBuffer(0, IID_ID3D11Texture2D1, reinterpret_cast<void**>(&frame_buffer));
    if (FAILED(m_result))
    {
        std::cerr << "Couldn't get the swap chain buffer!" << std::endl;
    }

    ID3D11RenderTargetView1* render_target_view;
    m_result = m_device->CreateRenderTargetView1(frame_buffer, 0, &render_target_view);
    if (FAILED(m_result))
    {
        std::cerr << "Couldn't create Render Target view!" << std::endl;
    }

    ID3DBlob* vs_blob    = nullptr;
    ID3DBlob* ps_blob    = nullptr;
    ID3DBlob* error_blob = nullptr;

    m_result = D3DCompileFromFile(
        L"shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "vs_main", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &vs_blob, &error_blob
    );
    if (FAILED(m_result))
    {
        if (error_blob)
        {
            OutputDebugStringA(reinterpret_cast<char*>(error_blob->GetBufferPointer()));
            error_blob->Release();
        }
        if (vs_blob)
            vs_blob->Release();
        assert(false);
    }

    m_result = D3DCompileFromFile(
        L"shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "ps_main", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &ps_blob, &error_blob
    );
    if (FAILED(m_result))
    {
        if (error_blob)
        {
            OutputDebugStringA(reinterpret_cast<char*>(error_blob->GetBufferPointer()));
            error_blob->Release();
        }
        if (ps_blob)
            ps_blob->Release();
        assert(false);
    }

    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader*  ps = nullptr;

    m_result = m_device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &vs);
    assert(SUCCEEDED(m_result));

    m_result = m_device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &ps);
    assert(SUCCEEDED(m_result));

    //  m_device->CreateInputLayout();
    //  m_device_context->IASetInputLayout();
    //  m_device->CreateBuffer();
    //  m_device_context->IASetVertexBuffers();

    //  TODO: continue to make a renderer (follow some guides idk)
}

renderer_t::~renderer_t()
{
    //  clean up dxgi
    m_dxgi.factory->Release();
    m_dxgi.device->Release();
    m_dxgi.graphics_card->Release();
    m_dxgi.monitor->Release();
    m_swapchain->Release();
    delete [] m_dxgi.display_modes;

    //  clean up d3d11
    m_device->Release();
    m_device_context->Release();
}

void renderer_t::get_all_available_adapters(std::vector<IDXGIAdapter4*>& out_adapters)
{
    IDXGIAdapter4* adapter;  //  used to temporary store found adapter

    //  getting all the adapters
    bool           could_find;  //  to check wether there are adapters left
    for (uint8_t index = 0;; index++)
    {
        could_find =
            m_dxgi.factory->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_IDXGIAdapter4, reinterpret_cast<void**>(&adapter))
            != DXGI_ERROR_NOT_FOUND;  //  get the adapter in the order of "highest performance first"

        if (not could_find)  //  finish if couldn't find the next adapter
            break;

        out_adapters.push_back(adapter);  //  store the found adapter
    }
}

DXGI_MODE_DESC1 renderer_t::get_best_display_mode()
{
    //  get display modes and take the best one (highest resolution and refresh rate)
    uint32_t number_of_display_modes = 0;
    m_dxgi.monitor->GetDisplayModeList1(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_ENUM_MODES_SCALING, &number_of_display_modes, nullptr);
    m_dxgi.display_modes = new DXGI_MODE_DESC1 [number_of_display_modes];
    m_dxgi.monitor->GetDisplayModeList1(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_ENUM_MODES_SCALING, &number_of_display_modes, m_dxgi.display_modes);
    return m_dxgi.display_modes [number_of_display_modes - 1];
}

bool renderer_t::create_device()
{
    m_result = D3D11CreateDevice(
        m_dxgi.graphics_card,
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        D3D11_CREATE_DEVICE_SINGLETHREADED,
        &m_feature_level,
        1,
        D3D11_SDK_VERSION,
        reinterpret_cast<ID3D11Device**>(&m_device),
        nullptr,
        reinterpret_cast<ID3D11DeviceContext**>(&m_device_context)
    );  //  create the device and its context for our graphics card

    if (FAILED(m_result))
    {
        std::cerr << "Failed to create Direct3D device!\t" << std::hex << m_result << std::endl;
        return false;
    }

    //  after creating the direct3d device, obtain the corresponding dxgi device
    m_result = m_device->QueryInterface<IDXGIDevice4>(&m_dxgi.device);

    if (FAILED(m_result))
    {
        std::cerr << "Failed to obtain DXGI device!\t" << std::hex << m_result << std::endl;
        return false;
    }

    return true;
}

bool renderer_t::create_swapchain(DXGI_MODE_DESC1* in_fullscreen_display_mode, bool in_windowed, HWND in_window_handle)
{
    //  how can the swapchain be used
    DXGI_USAGE            buffer_usage    = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    uint32_t              swapchain_flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    //  info on the dxgi format topic:
    //      - https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/converting-data-color-space
    //      - https://developer.nvidia.com/gpugems/gpugems3/part-iv-image-effects/chapter-24-importance-being-linear
    DXGI_SWAP_CHAIN_DESC1 swapchain_descriptor {};
    swapchain_descriptor.Width       = in_windowed ? 0 : in_fullscreen_display_mode->Width;   //  set the swapchain size to the native resolution
    swapchain_descriptor.Height      = in_windowed ? 0 : in_fullscreen_display_mode->Height;  //  set the swapchain size to the native resolution
    swapchain_descriptor.Format      = DXGI_FORMAT_R16G16B16A16_FLOAT;                        //  use linear format that is supported by flip mode swap chain
    swapchain_descriptor.Stereo      = false;   //  true is for VR-like things (two screens rendering at the same time)
    swapchain_descriptor.SampleDesc  = {1, 0};  //  disable MSAA
    swapchain_descriptor.BufferUsage = buffer_usage;
    swapchain_descriptor.BufferCount = 2;                                 //  we want the front and the back buffer (sums up to 2)
    swapchain_descriptor.Scaling     = DXGI_SCALING_NONE;                 //  stretch preserving the original rendered aspect ratio
    swapchain_descriptor.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;  //  use flip model instead of bitblt model (more in MSDN)
    swapchain_descriptor.AlphaMode   = DXGI_ALPHA_MODE_UNSPECIFIED;       //  FIXME: IDK what is this
    swapchain_descriptor.Flags       = swapchain_flags;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC swapchain_fullscreen_descriptor {};
    swapchain_fullscreen_descriptor.RefreshRate      = in_fullscreen_display_mode->RefreshRate;       //  get the highest refresh rate from the monitor
    swapchain_fullscreen_descriptor.ScanlineOrdering = in_fullscreen_display_mode->ScanlineOrdering;  //  get scanline ordering from the monitor
    swapchain_fullscreen_descriptor.Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;                 //  never use scaling
    swapchain_fullscreen_descriptor.Windowed         = in_windowed;                                   //  start the application in specified mode

    m_result = m_dxgi.factory->CreateSwapChainForHwnd(
        m_device, in_window_handle, &swapchain_descriptor, &swapchain_fullscreen_descriptor, nullptr, reinterpret_cast<IDXGISwapChain1**>(&m_swapchain)
    );
    if (FAILED(m_result))
    {
        std::cout << "Couldn't create swapchain for the window:" << std::flush;
        return false;
    }

    return true;
}
}  //  namespace zeno::dx11