#include "RHITriangleTest.h"

namespace Eden::Gfx::Tests
{
	void RHITriangleTest::Init(Window* window)
	{
		RHICreate(window);

		RenderPassDesc renderPassDesc		= {};
		renderPassDesc.debugName			= "Triangle Test Render Pass";
		renderPassDesc.bIsSwapchainTarget	= true;
		renderPassDesc.attachmentsFormats	= { Format::kRGBA32_FLOAT, Format::Depth };
		renderPassDesc.width				= window->GetWidth();
		renderPassDesc.height				= window->GetHeight();
		m_MainRenderPass = RHICreateRenderPass(&renderPassDesc);

		PipelineDesc pipelineDesc		= {};
		pipelineDesc.debugName			= "Triangle Test Pipeline";
		pipelineDesc.programName		= "tests/TriangleTest";
		pipelineDesc.renderPass			= m_MainRenderPass;
		m_MainPipeline = RHICreatePipeline(&pipelineDesc);

		const float triangleVertices[] =
		{
			// position      color
			 0.0,  0.5, 0,	 0, 0, 1,
			 0.5, -0.5, 0,	 0, 1, 0,
			-0.5, -0.5, 0,	 1, 0, 0,
		};

		BufferDesc vbDesc	= {};
		vbDesc.usage		= BufferDesc::Vertex_Index;
		vbDesc.stride		= sizeof(float) * 6;
		vbDesc.elementCount = 3;
		vbDesc.debugName    = "Triangle Test VB";
		m_VertexBuffer = RHICreateBuffer(&vbDesc, triangleVertices);
	}

	void RHITriangleTest::Update()
	{
		RHIBeginRender();
		
		RHIBeginRenderPass(m_MainRenderPass);
		RHIBindPipeline(m_MainPipeline);
		RHIBindVertexBuffer(m_VertexBuffer);
		RHIDraw(3);
		RHIEndRenderPass(m_MainRenderPass);

		RHIEndRender();
		RHIRender();
	}

	RHITriangleTest::~RHITriangleTest()
	{
		m_MainRenderPass.Reset();
		m_MainPipeline.Reset();
		m_VertexBuffer.Reset();
		RHIShutdown();
	}

}