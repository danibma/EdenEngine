#pragma once

#include <cstdint>

namespace Eden
{
	constexpr uint32_t GFrameCount = 2;
	// TODO: Right now just use a huge number here, and refactor when bindless descriptors are added
	constexpr uint32_t GSRVDescriptorCount = 8129;
	constexpr uint32_t GRTVDescriptorCount = 256;
	constexpr uint32_t GDSVDescriptorCount = 256;

	enum class ShaderStage
	{
		kVS = 0,
		kPS,
		kCS,
		kCount // Don't use this
	};

	enum class ComparisonFunc
	{
		kNever,
		kLess,
		kEqual,
		kLessEqual,
		kGreater,
		kNotEqual,
		kGreaterEqual,
		kAlways
	};

	enum class CullMode
	{
		kNone,
		kFront,
		kBack
	};

	enum class Blend
	{
		kZero,
		kOne,
		kSrcColor,
		kInvSrcColor,
		kSrcAlpha,
		kInvSrcAlpha,
		kDestAlpha,
		kInvDestAlpha,
		kDestColor,
		kInvDestColor,
		kSrcAlphaSat,
		kBlendFactor,
		kInvBlendFactor,
		kSrc1Color,
		kInvSrc1Color,
		kSrc1Alpha,
		kInvSrc1Alpha,
	};
	enum class BlendOp
	{
		kAdd,
		kSubtract,
		kRevSubtract,
		kMin,
		kMax,
	};

	enum class Format
	{
		kUnknown,

		kR8_UNORM,
		kR8_UINT,
		kR16_UINT,
		kR32_UINT,
		kR32_FLOAT,
		kRG8_UNORM,
		kRG16_FLOAT,
		kRG32_FLOAT,
		kRGBA8_UNORM,
		kRGBA16_FLOAT,
		kRGBA32_FLOAT,

		kSRGB,

		kDepth32FloatStencil8X24_UINT,
		kDepth32_FLOAT,
		kDepth24Stencil8,

		kR32_FLOAT_X8X24_TYPELESS,
		kR32_TYPELESS,
		kR24_X8_TYPELESS,

		// Defaults
		Depth = kDepth24Stencil8,
	};

	enum class ResourceState
	{
		kCommon,
		kRenderTarget,
		kPresent,
		kUnorderedAccess,
		kPixelShader,
		kNonPixelShader,
		kRead,
		kCopyDest,
		kCopySrc,
		kDepthRead,
		kDepthWrite
	};

	enum PipelineType
	{
		kPipelineType_Graphics,
		kPipelineType_Compute // Not implemented
	};

	enum API
	{
		kApi_D3D12,
		kApi_Vulkan
	};

	enum TextureUsage
	{
		kReadOnly,
		kReadWrite // Used for UAV's
	};
	
	namespace Utils
	{
		inline const char* ShaderStageToString(const ShaderStage target)
		{
			switch (target)
			{
				case ShaderStage::kVS:
					return "Vertex";
				case ShaderStage::kPS:
					return "Pixel";
				default:
					return "Null";
			}
		}

		inline const char* APIToString(const API api)
		{
			switch (api)
			{
				case API::kApi_D3D12:
					return "D3D12";
				case API::kApi_Vulkan:
					return "Vulkan";
				default:
					return "";
			}
		}
	}
}