#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>

#include "RHIDefinitions.h"
#include "Core/Memory/SharedPtr.h"

namespace Eden
{
	//
	// Buffer
	//
	struct BufferDesc
	{
		enum BufferUsage
		{
			Vertex_Index,
			Uniform, // ConstantBuffer in D3D12
			Storage, // Structured, UAV, RWBuffer in D3D12
			Readback
		};

		uint32_t		stride;
		uint32_t		elementCount;
		BufferUsage		usage = Uniform;
		std::string		debugName;
	};

	struct Buffer
	{
		bool			bIsInitialized = false;
		ResourceState	currentState;
		void*			mappedData;
		uint32_t		size;
		BufferDesc		desc;
	};
	typedef SharedPtr<Buffer> BufferRef;

	//
	// Texture
	//
	struct TextureDesc
	{
		enum TextureType
		{
			Texture2D,
			Texture3D,  // Not implemented
			TextureCube	// Not implemented
		};

		void*		data;
		uint32_t	width;
		uint32_t	height;
		bool		bIsSrgb = false;
		bool		bIsStorage = false;
		bool		bGenerateMips = true;
		TextureType type = Texture2D;
		std::string debugName;
	};

	struct Texture
	{
		bool			bIsInitialized = false;
		ResourceState	currentState;
		Format			imageFormat;
		uint32_t		mipCount;
		TextureDesc		desc;
	};
	typedef SharedPtr<Texture> TextureRef;

	//
	// Render Pass
	//
	struct RenderPassDesc
	{
		std::string			debugName;
		bool				bIsImguiPass = false;
		bool				bIsSwapchainTarget = false;
		std::vector<Format> attachmentsFormats;
		uint32_t			width = 0, height = 0;
		glm::vec4			clearColor = glm::vec4(0, 0, 0, 1);
	};

	struct RenderPass
	{
		std::vector<TextureRef> colorAttachments;
		TextureRef				depthStencil;
		RenderPassDesc			desc;
	};
	typedef SharedPtr<RenderPass> RenderPassRef;

	//
	// Pipeline
	//
	struct PipelineDesc
	{
		std::string		debugName;
		std::string		programName;
		bool			bEnableBlending = false;
		bool			bIsFrontCounterClockwise = false;
		float			minDepth = 0.0f;
		CullMode		cull_mode = CullMode::kBack;
		ComparisonFunc	depthFunc = ComparisonFunc::kLess;
		PipelineType	type = kPipelineType_Graphics;
		RenderPassRef		renderPass;
	};

	struct Pipeline
	{
		PipelineDesc desc;

		// Shader Reflection data
		// name, rootParameterIndex
		std::unordered_map<std::string, uint32_t> rootParameterIndices;
	};
	typedef SharedPtr<Pipeline> PipelineRef;

	//
	// GPU Timer
	//
	struct GPUTimer
	{
		double elapsedTime = 0.0f;

		BufferRef readbackBuffer;
	};
	typedef SharedPtr<GPUTimer> GPUTimerRef;
}
