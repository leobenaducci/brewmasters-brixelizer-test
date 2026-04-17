#include "AppLayer.hpp"
#include <Core/Application.hpp>

static constexpr UINT  WIDTH  = 1280;
static constexpr UINT  HEIGHT = 720;
static constexpr float FWIDTH  = static_cast<float>(WIDTH);
static constexpr float FHEIGHT = static_cast<float>(HEIGHT);

AppLayer::AppLayer()
	: m_Camera({ 0.0f, 5.0f, -10.0f })
	, m_SceneBuilder(m_Camera)
	, m_Input()
	, m_PlayerController(m_Camera, m_Input)
{
	auto& app      = Core::Application::Get();
	auto* device   = app.GetDX().GetDevice();
	auto* cmdList  = app.GetDX().GetCommandList();
	auto* cmdQueue = app.GetDX().GetCommandQueue();
	auto* srvHeap  = app.GetDX().GetSrvHeap();

	m_BrixelizerContext = std::make_unique<Brixelizer::BrixelizerContext>(device, cmdQueue);

	m_LightingShader = std::make_unique<Shader>(device, L"D:\\projects\\brixelizer-test\\Engine\\Resources\\Shaders\\Lighting.hlsl");

	app.GetDX().BeginFrame();

	m_Model.Load(device, cmdList, srvHeap, "D:\\projects\\brixelizer-test\\Engine\\Resources\\Sponza\\sponza.obj");

	for (auto const& mesh : m_Model.GetMeshes()) {
		m_BrixelizerMeshInstances.push_back(m_BrixelizerContext->SubmitMeshInstance(*mesh));
	}

	app.GetDX().Present();
}

AppLayer::~AppLayer() {
	for (auto const& meshInstance : m_BrixelizerMeshInstances) {
		m_BrixelizerContext->UnloadMeshInstance(meshInstance);
	}
}

void AppLayer::OnUpdate(float deltaTime) {
	m_PlayerController.Update(deltaTime);
	m_Input.EndFrame();
}

void AppLayer::OnEvent(Core::Event& event) {
	m_Input.OnEvent(event);
}

void AppLayer::OnRender() {
	auto& app     = Core::Application::Get();
	auto* device  = app.GetDX().GetDevice();
	auto* cmdList = app.GetDX().GetCommandList();
	auto* srvHeap = app.GetDX().GetSrvHeap();

	D3D12_VIEWPORT viewport { 0, 0, FWIDTH, FHEIGHT, 0.0f, 1.0f };
	D3D12_RECT scissor { 0, 0, (LONG)WIDTH, (LONG)HEIGHT };

	auto rtv = app.GetDX().GetCurrentRTV();
	auto dsv = app.GetDX().GetDepthStencilView();
	auto renderTarget = app.GetDX().GetCurrentRenderTarget();

	m_BrixelizerContext->Update(device, app.GetDX().GetCurrentSwapChainFrameIndex(), m_Camera, cmdList, renderTarget.Get());

	constexpr float clearColor[] = { 0.7f, 0.1f, 0.9f, 1.0f };

	cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);

	cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissor);

	ID3D12DescriptorHeap* heaps[] = { srvHeap };
	cmdList->SetDescriptorHeaps(1, heaps);

	m_LightingShader->Bind(cmdList);

	SceneConstants scene = m_SceneBuilder.Build();
	cmdList->SetGraphicsRoot32BitConstants(0, sizeof(SceneConstants) / 4, &scene, 0);

	UINT increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	m_Model.Draw(cmdList, srvHeap, increment);
}