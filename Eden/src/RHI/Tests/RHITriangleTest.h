#pragma once

#include "RHIBaseTest.h"
#include "RHI/DynamicRHI.h"

namespace Eden::Gfx::Tests
{
	class RHITriangleTest final : public RHIBaseTest
	{
		RenderPassRef m_MainRenderPass;
		PipelineRef   m_MainPipeline;
		BufferRef	  m_VertexBuffer;

	public:
		RHITriangleTest() = default;
		~RHITriangleTest();

		virtual void Init(Window* window) override;
		virtual void Update() override;

	};
}