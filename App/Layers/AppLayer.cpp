#include "AppLayer.hpp"

static constexpr UINT   WIDTH  = 1280;
static constexpr UINT   HEIGHT = 720;
static constexpr float  FWIDTH  = static_cast<float>(WIDTH);
static constexpr float  FHEIGHT = static_cast<float>(HEIGHT);

// ─────────────────────────────────────────────────────────────────────────────
AppLayer::AppLayer()
    : m_Camera({ 0.0f, 5.0f, -10.0f })
    , m_SceneBuilder(m_Camera)
    , m_Input()
    , m_PlayerController(m_Camera, m_Input)
{
    auto& app     = Core::Application::Get();
    auto* device  = app.GetDX().GetDevice();
    auto* cmdList = app.GetDX().GetCommandList();
    auto* srvHeap = app.GetDX().GetSrvHeap();

    m_GBufferShader  = std::make_unique<LightingShader>(device, L"D:\\archive\\dev\\dx12engine\\Engine\\Resources\\Shaders\\GBuffer.hlsl");
    m_LightingShader = std::make_unique<LightingShader>(device, L"D:\\archive\\dev\\dx12engine\\Engine\\Resources\\Shaders\\Lighting.hlsl");

    m_BrixelizerContext = std::make_unique<Brixelizer::BrixelizerContext>(
        device, app.GetDX().GetCommandQueue());

    app.GetDX().BeginFrame();

    m_Model.Load(device, cmdList, srvHeap,
        "D:\\archive\\dev\\dx12engine\\Engine\\Resources\\Sponza\\sponza.obj");

    for (auto const& mesh : m_Model.GetMeshes()) {
        m_MeshInstances.push_back(m_BrixelizerContext->SubmitMeshInstance(*mesh));
    }

    // G-Buffer.
    {
        D3D12_DESCRIPTOR_HEAP_DESC rtvDesc {
            .Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .NumDescriptors = 1,
            .Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        };

        ComPtr<ID3D12DescriptorHeap> gbufferRtvHeap;
        device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&gbufferRtvHeap));
        gbufferRtvHeap->SetName(L"GBuffer_RTV_Heap");

        D3D12_DESCRIPTOR_HEAP_DESC dsvDesc {
            .Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            .NumDescriptors = 1,
            .Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        };
        ComPtr<ID3D12DescriptorHeap> gbufferDsvHeap;
        device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&gbufferDsvHeap));
        gbufferDsvHeap->SetName(L"GBuffer_DSV_Heap");

        m_GBuffer = std::make_unique<Brixelizer::GBuffer>();
        m_GBuffer->Init(device, WIDTH, HEIGHT,
            gbufferRtvHeap.Get(), 0,
            gbufferDsvHeap.Get(), 0);
    }

    m_DiffuseGISrvSlot = static_cast<UINT>(m_Model.TextureCount());
    CreateGBufferSRVs(device);

    m_BrixelizerGIContext = std::make_unique<Brixelizer::BrixelizerGIContext>();
    m_BrixelizerGIContext->Init(
        device,
        cmdList,
        m_BrixelizerContext->GetFfxInterface(),
        WIDTH, HEIGHT);

    app.GetDX().Present();

    DirectX::XMStoreFloat4x4(&m_PrevView, m_Camera.GetViewMatrix());
    DirectX::XMStoreFloat4x4(&m_PrevProj, m_Camera.GetProjectionMatrix());
}

AppLayer::~AppLayer() {
    for (auto const& instance : m_MeshInstances)
        m_BrixelizerContext->UnloadMeshInstance(instance);

    m_BrixelizerGIContext->Destroy();
}

void AppLayer::OnUpdate(float deltaTime) {
    m_PlayerController.Update(deltaTime);
    m_Input.EndFrame();
}

void AppLayer::OnRender() {
    auto& app        = Core::Application::Get();
    auto* device     = app.GetDX().GetDevice();
    auto* cmdList    = app.GetDX().GetCommandList();
    auto* srvHeap    = app.GetDX().GetSrvHeap();
    uint32_t frameIdx = app.GetDX().GetCurrentBackBufferIndex();

    DirectX::XMFLOAT4X4 view, proj;
    DirectX::XMStoreFloat4x4(&view, m_Camera.GetViewMatrix());
    DirectX::XMStoreFloat4x4(&proj, m_Camera.GetProjectionMatrix());

    D3D12_VIEWPORT const viewport { 0, 0, FWIDTH, FHEIGHT, 0.0f, 1.0f };
    D3D12_RECT     const scissor  { 0, 0, WIDTH,  HEIGHT };

    // G-Buffer Pass.
    {
        m_GBuffer->BeginGBufferPass(cmdList);

        cmdList->RSSetViewports(1, &viewport);
        cmdList->RSSetScissorRects(1, &scissor);

        ID3D12DescriptorHeap* heaps[] = { srvHeap };
        cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
        m_GBufferShader->Bind(cmdList);

        SceneConstants scene = m_SceneBuilder.Build();
        cmdList->SetGraphicsRoot32BitConstants(0, sizeof(SceneConstants) / 4, &scene, 0);

        m_Model.Draw(cmdList, srvHeap,
            device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

        m_GBuffer->EndGBufferPass(cmdList);
    }

    m_BrixelizerContext->Update(device, frameIdx, m_Camera.GetPosition(), cmdList);

    // Brixelizer GI Dispatch.
    {
        auto* cascadeAABBTrees = m_BrixelizerContext->GetCascadeAABBTrees();
        auto* cascadeBrickMaps = m_BrixelizerContext->GetCascadeBrickMaps();

        uint32_t const startCascade = 2 * Brixelizer::BrixelizerContext::MAX_CASCADES;
        uint32_t const endCascade   = 3 * Brixelizer::BrixelizerContext::MAX_CASCADES - 1;

        m_BrixelizerGIContext->Dispatch(
            cmdList, frameIdx,
            view, proj,
            m_PrevView, m_PrevProj,
            m_Camera.GetPosition(),
            *m_GBuffer,
            m_BrixelizerContext->GetSdfAtlas(),
            m_BrixelizerContext->GetBrickAABBs(),
            cascadeAABBTrees,
            cascadeBrickMaps,
            m_BrixelizerContext->GetFfxBrixelizerContext(),
            startCascade, endCascade);
    }

    m_GBuffer->CopyToHistory(cmdList);

    // Light pass.
    UpdateDiffuseGISRV(device);
    {
        constexpr float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        auto rtv = app.GetDX().GetCurrentRTV();
        auto dsv = app.GetDX().GetDepthStencilView();

        cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
        cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        cmdList->RSSetViewports(1, &viewport);
        cmdList->RSSetScissorRects(1, &scissor);

        ID3D12DescriptorHeap* heaps[] = { srvHeap };
        cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
        m_LightingShader->Bind(cmdList);

        SceneConstants scene = m_SceneBuilder.Build();
        struct GIConstants { float giIntensity; float pad[3]; } gi { 1.0f };

        UINT const srvStep = device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // b0, b1
        LightingShader::SetSceneConstants(cmdList, &scene);
        LightingShader::SetGIConstants(cmdList, &gi);

        D3D12_GPU_DESCRIPTOR_HANDLE giHandle = srvHeap->GetGPUDescriptorHandleForHeapStart();
        giHandle.ptr += m_DiffuseGISrvSlot * srvStep;
        LightingShader::SetDiffuseGITable(cmdList, giHandle);

        m_Model.Draw(cmdList, srvHeap,
            device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    }

    m_PrevView = view;
    m_PrevProj = proj;
}

void AppLayer::OnEvent(Core::Event& e) {
    m_Input.OnEvent(e);
}

void AppLayer::CreateGBufferSRVs(ID3D12Device* device) {
    UpdateDiffuseGISRV(device);
}

void AppLayer::UpdateDiffuseGISRV(ID3D12Device* device) {
    auto* srvHeap = Core::Application::Get().GetDX().GetSrvHeap();
    UINT  step    = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = srvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += m_DiffuseGISrvSlot * step;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {
        .Format                  = DXGI_FORMAT_R16G16B16A16_FLOAT,
        .ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D               = { .MipLevels = 1 },
    };
    device->CreateShaderResourceView(
        m_BrixelizerGIContext->GetDiffuseGI(), &srvDesc, handle);
}