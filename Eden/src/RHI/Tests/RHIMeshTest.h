#pragma once

#include "RHIBaseTest.h"
#include "Scene/MeshSource.h"
#include "Core/Camera.h"

namespace Eden::Gfx::Tests
{
	class RHIMeshTest final : public RHIBaseTest
	{
		RenderPassRef m_MainRenderPass;
		PipelineRef   m_MainPipeline;
		BufferRef	  m_VertexBuffer;

		Camera m_Camera;

		SharedPtr<MeshSource> m_MeshSource;

	public:
		RHIMeshTest() = default;
		~RHIMeshTest();

		virtual void Init(Window* window) override;
		virtual void Update() override;

	};
}
