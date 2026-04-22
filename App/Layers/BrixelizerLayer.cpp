#include "BrixelizerLayer.hpp"
#include <Core/Application.hpp>

static constexpr UINT  WIDTH  = 1280;
static constexpr UINT  HEIGHT = 720;
static constexpr float FWIDTH  = static_cast<float>(WIDTH);
static constexpr float FHEIGHT = static_cast<float>(HEIGHT);

BrixelizerLayer::BrixelizerLayer() {
	auto& app      = Core::Application::Get();
	auto* device   = app.GetDX().GetDevice();
	auto* cmdList  = app.GetDX().GetCommandList();
	auto* cmdQueue = app.GetDX().GetCommandQueue();
	auto* srvHeap  = app.GetDX().GetSrvHeap();

	m_BrixelizerContext = std::make_unique<Brixelizer::BrixelizerContext>(device, cmdQueue);

	m_LightingShader = std::make_unique<BrixelizerShader>(
		device, L"..\\Engine\\Resources\\Shaders\\BrixelizerSDFVisualization.hlsl"
	);

	app.GetDX().BeginFrame();

	m_Model.Load(device, cmdList, srvHeap, "..\\Engine\\Resources\\Sponza\\sponza.obj");

	for (auto const& mesh : m_Model.GetMeshes()) {
		m_BrixelizerMeshInstances.push_back(m_BrixelizerContext->SubmitMeshInstance(*mesh));
	}

	app.GetDX().Present();
}

BrixelizerLayer::~BrixelizerLayer() {
	for (auto const& meshInstance : m_BrixelizerMeshInstances) {
		m_BrixelizerContext->UnloadMeshInstance(meshInstance);
	}
}

void BrixelizerLayer::OnUpdate(float deltaTime) {
	m_PlayerController.Update(deltaTime);
	m_Input.EndFrame();
}

void BrixelizerLayer::OnEvent(Core::Event& event) {
	m_Input.OnEvent(event);
}

void BrixelizerLayer::OnRender() {
	auto& app     = Core::Application::Get();
	auto* device  = app.GetDX().GetDevice();
	auto* cmdList = app.GetDX().GetCommandList();
	auto* srvHeap = app.GetDX().GetSrvHeap();

	auto renderTarget = app.GetDX().GetCurrentRenderTarget();
	m_BrixelizerContext->Update(device, app.GetDX().GetCurrentSwapChainFrameIndex(), m_Camera, cmdList, renderTarget.Get());

	constexpr float clearColor[] = { 0.7f, 0.1f, 0.9f, 1.0f };
	auto rtv = app.GetDX().GetCurrentRTVHandle();
	auto dsv = app.GetDX().GetDepthStencilView();

	cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);

	cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

	constexpr D3D12_VIEWPORT viewport { 0, 0, FWIDTH, FHEIGHT, 0.0f, 1.0f };
	constexpr D3D12_RECT scissor { 0, 0, (LONG)WIDTH, (LONG)HEIGHT };
	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissor);

	D3D12_GPU_VIRTUAL_ADDRESS sceneConstantGPUAddress = m_SceneBuilder.GetSceneConstantBufferGPUAddress();
	D3D12_GPU_VIRTUAL_ADDRESS cascadeStructGPUAddress = m_BrixelizerContext->GetConstantsGPUAddress();
	m_LightingShader->Bind(cmdList, sceneConstantGPUAddress, cascadeStructGPUAddress, srvHeap);

	SceneConstants scene = m_SceneBuilder.Build(m_Camera);

	UINT increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	m_Model.Draw(cmdList, srvHeap, increment);
}