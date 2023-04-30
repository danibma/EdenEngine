#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>

#include "RHIDefinitions.h"
#include "Core/Memory/SharedPtr.h"

namespace Eden
{
	struct ResourceInternal
	{
		virtual ~ResourceInternal()
		{
			// empty destructor so that the derived class destructor is called
		}
	};

	struct GraphicsChild
	{
		std::string debugName;
		SharedPtr<ResourceInternal> internal_state;
		bool IsValid() { return internal_state != nullptr; }
		operator bool()
		{
			return IsValid();
		}
	};

	struct Resource : GraphicsChild
	{
		bool bIsInitialized = false;
		ResourceState currentState;
	};

	struct BufferDesc
	{
		enum BufferUsage
		{
			Vertex_Index,
			Uniform, // ConstantBuffer in D3D12
			Storage, // Structured, UAV, RWBuffer in D3D12
			Readback
		};

		uint32_t stride;
		uint32_t elementCount;
		BufferUsage usage = Uniform;
	};

	struct Buffer : public Resource
	{
		void* mappedData;
		uint32_t size;
		BufferDesc desc;
	};

	struct TextureDesc
	{
		enum TextureType
		{
			Texture2D,
			Texture3D,  // Not implemented
			TextureCube	// Not implemented
		};

		void* data;
		uint32_t width;
		uint32_t height;
		bool bIsSrgb = false;
		bool bIsStorage = false;
		bool bGenerateMips = true;
		TextureType type = Texture2D;
	};

	struct Texture : public Resource
	{
		Format imageFormat;
		uint32_t mipCount;
		TextureDesc desc;
	};

	struct RenderPassDesc
	{
		std::string debugName;
		bool bIsImguiPass = false;
		bool bIsSwapchainTarget = false;
		std::vector<Format> attachmentsFormats;
		uint32_t width = 0, height = 0;
		glm::vec4 clearColor = glm::vec4(0, 0, 0, 1);
	};

	struct RenderPass : GraphicsChild
	{
		std::vector<Texture> colorAttachments;
		Texture depthStencil;
		RenderPassDesc desc;
	};

	struct PipelineDesc
	{
		std::string programName;
		bool bEnableBlending = false;
		bool bIsFrontCounterClockwise = false;
		float minDepth = 0.0f;
		CullMode cull_mode = CullMode::kBack;
		ComparisonFunc depthFunc = ComparisonFunc::kLess;
		PipelineType type = kPipelineType_Graphics;
		RenderPass* renderPass;
	};

	struct Pipeline : GraphicsChild
	{
		PipelineDesc desc;

		// Shader Reflection data
		// name, rootParameterIndex
		std::unordered_map<std::string, uint32_t> rootParameterIndices;
	};

	struct GPUTimer : GraphicsChild
	{
		double elapsedTime = 0.0f;

		Buffer readbackBuffer;
	};
}
