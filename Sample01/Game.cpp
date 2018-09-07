//
// Game.cpp
//

#include "pch.h"
#include "Game.h"


extern void ExitGame();

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Game::Game() noexcept(false)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,DXGI_FORMAT_D32_FLOAT,2,D3D_FEATURE_LEVEL_11_0, 0);
    m_deviceResources->RegisterDeviceNotify(this);


    m_intermediateRT = std::make_unique<DX::RenderTexture>(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
     XMVECTORF32 color;
     color.v = XMColorSRGBToRGB(Colors::CornflowerBlue);
     m_intermediateRT->SetClearColor(color);
}

Game::~Game()
{
    if (m_deviceResources)
    {
        m_deviceResources->WaitForGpu();
    }
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{
    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    // TODO: Change the timer settings if you want something other than the default variable timestep mode.
    // e.g. for 60 FPS fixed timestep update logic, call:

//     m_timer.SetFixedTimeStep(true);
//     m_timer.SetTargetElapsedSeconds(1.0 / 60);

}

#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float elapsedTime = float(timer.GetElapsedSeconds());

    // TODO: Add your game logic here.
    elapsedTime;


    if (m_screenPos.x>m_deviceResources->GetWidth())
    {
        m_screenPos.x = 0;
    }
    m_screenPos.x += 5;
    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    m_intermediateRT->BeginScene(commandList);

    Clear();

    // TODO: Add your rendering code here.
    {
        ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap() };
        commandList->SetDescriptorHeaps(_countof(heaps), heaps);

        m_spriteBatch->Begin(commandList);

        m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle(Descriptors::Background),
            GetTextureSize(m_background.Get()),
            m_fullscreenRect);

        m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle(Descriptors::Cat),
            GetTextureSize(m_texture.Get()),
            m_screenPos, nullptr, Colors::White, 0.f, m_origin);

        m_spriteBatch->End();
    }
    PIXEndEvent(commandList);

    m_intermediateRT->EndScene(commandList);

    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, nullptr);
    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);
    m_postProcess->Process(commandList);

    // Show the new frame.
    PIXBeginEvent(m_deviceResources->GetCommandQueue(), PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    PIXEndEvent(m_deviceResources->GetCommandQueue());
    m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    //auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    auto rtvDescriptor = m_renderDescriptors->GetCpuHandle(RTDescriptors::IntermediateRT);
    auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, Colors::CornflowerBlue, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set the viewport and scissor rect.
    auto viewport = m_deviceResources->GetScreenViewport();
    auto scissorRect = m_deviceResources->GetScissorRect();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    PIXEndEvent(commandList);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
    // TODO: Game is becoming active window.
}

void Game::OnDeactivated()
{
    // TODO: Game is becoming background window.
}

void Game::OnSuspending()
{
    // TODO: Game is being power-suspended (or minimized).
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

    // TODO: Game is being power-resumed (or returning from minimize).
}

void Game::OnWindowMoved()
{
    auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Game::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();

    // TODO: Game window is being resized.
}

// Properties
void Game::GetDefaultSize(int& width, int& height) const
{
    // TODO: Change to desired default window size (note minimum size is 320x200).
    width = 1280;
    height = 720;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    // TODO: Initialize device dependent objects here (independent of window size).
    m_graphicsMemory = std::make_unique<GraphicsMemory>(device);

    //create resource heap
    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        Descriptors::Count);

    //create rt heap
    m_renderDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        RTDescriptors::RTCount);

    m_intermediateRT->SetDevice(device,
        m_resourceDescriptors->GetCpuHandle(Descriptors::SceneTex),
        m_renderDescriptors->GetCpuHandle(RTDescriptors::IntermediateRT));

    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(),
        DXGI_FORMAT_UNKNOWN);
    // Set tone-mapper as 'pass-through' for now...
    m_postProcess = std::make_unique<BasicPostProcess>(device,
        rtState,
        BasicPostProcess::Sepia);



    //resource 
    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    {
        //load&create cat
        DX::ThrowIfFailed(
            CreateWICTextureFromFile(device, resourceUpload, L"cat.png",
                m_texture.ReleaseAndGetAddressOf()));

        CreateShaderResourceView(device, m_texture.Get(),
            m_resourceDescriptors->GetCpuHandle(Descriptors::Cat));

        //load&create background
        DX::ThrowIfFailed(
            CreateWICTextureFromFile(device, resourceUpload, L"sunset.jpg",
                m_background.ReleaseAndGetAddressOf()));

        CreateShaderResourceView(device, m_background.Get(),
            m_resourceDescriptors->GetCpuHandle(Descriptors::Background));
    }

    {
        RenderTargetState rtState(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_FORMAT_D32_FLOAT);

        SpriteBatchPipelineStateDescription pd(rtState);
        m_spriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, pd);

        XMUINT2 catSize = GetTextureSize(m_texture.Get());

        m_origin.x = float(catSize.x / 2);
        m_origin.y = float(catSize.y / 2);
    }

   

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

    uploadResourcesFinished.wait();


    //init cat position
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f,
        static_cast<float>(m_deviceResources->GetWidth()), static_cast<float>(m_deviceResources->GetHeight()),
        D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
    m_spriteBatch->SetViewport(viewport);

    m_screenPos.x = m_deviceResources->GetWidth() / 2.f;
    m_screenPos.y = m_deviceResources->GetHeight() / 2.f;

    //init background
    m_fullscreenRect.left = 0;
    m_fullscreenRect.top = 0;
    m_fullscreenRect.right = m_deviceResources->GetWidth();
    m_fullscreenRect.bottom = m_deviceResources->GetHeight();


//     {
// 
//         std::unique_ptr<BasicPostProcess> postProcess;
// 
//         RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(),
//             m_deviceResources->GetDepthBufferFormat());
// 
//         postProcess = std::make_unique<BasicPostProcess>(device, rtState, BasicPostProcess::Sepia);
//     }
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    // TODO: Initialize windows-size dependent objects here.

    auto size = m_deviceResources->GetOutputSize();
    m_intermediateRT->SetWindow(size);
    auto intermediateTex = m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex);
    m_postProcess->SetSourceTexture(intermediateTex, m_intermediateRT->GetResource());
}

void Game::OnDeviceLost()
{
    // TODO: Add Direct3D resource cleanup here.
    m_graphicsMemory.reset();

    m_intermediateRT->ReleaseDevice();
    m_postProcess.reset();
    m_resourceDescriptors.reset();
    m_renderDescriptors.reset();


    m_texture.Reset();
    m_resourceDescriptors.reset();
    m_spriteBatch.reset();
    m_background.Reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
