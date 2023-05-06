#pragma once

#include <d3d12.h>
#include <d3d12shader.h>
#include <dxc/dxcapi.h>
#include <wrl/client.h>
#include "d3dx12.h"
#include <D3D12MemoryAllocator/D3D12MemAlloc.h>

#include "Core/Log.h"
#include "RHI/RHIDefinitions.h"

using namespace Microsoft::WRL;

// Macros
#define SAFE_RELEASE(x) if (x) x->Release();

#define DX_CHECK(call) \
	{ \
		HRESULT hr = (call); \
		ensureMsgf(hr == S_OK, "D3D12 call \"%s\" failed", #call); \
	}

// Globals
constexpr D3D_FEATURE_LEVEL GFeatureLevel = D3D_FEATURE_LEVEL_12_1;

// Debug message callback
inline void D3D12DebugMessageCallback(D3D12_MESSAGE_CATEGORY Category,
						  D3D12_MESSAGE_SEVERITY Severity,
						  D3D12_MESSAGE_ID ID,
						  LPCSTR pDescription,
						  void* pContext)
{
	switch (Severity)
	{
	case D3D12_MESSAGE_SEVERITY_CORRUPTION:
		ED_LOG_FATAL("{}", pDescription);
		break;
	case D3D12_MESSAGE_SEVERITY_ERROR:
		ED_LOG_ERROR("{}", pDescription);
		break;
	case D3D12_MESSAGE_SEVERITY_WARNING:
		ED_LOG_WARN("{}", pDescription);
		break;
	case D3D12_MESSAGE_SEVERITY_INFO:
		ED_LOG_INFO("{}", pDescription);
		break;
	case D3D12_MESSAGE_SEVERITY_MESSAGE:
		ED_LOG_TRACE("{}", pDescription);
		break;
	}
}

// Helpers
using namespace Eden;

constexpr DXGI_FORMAT ConvertToDXFormat(Format value)
{
	switch (value)
	{
	case Format::kUnknown:
		return DXGI_FORMAT_UNKNOWN;
	case Format::kR8_UNORM:
		return DXGI_FORMAT_R8_UNORM;
	case Format::kR8_UINT:
		return DXGI_FORMAT_R8_UINT;
	case Format::kR16_UINT:
		return DXGI_FORMAT_R16_UINT;
	case Format::kR32_UINT:
		return DXGI_FORMAT_R32_UINT;
	case Format::kR32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case Format::kRG8_UNORM:
		return DXGI_FORMAT_R8G8_UNORM;
	case Format::kRG16_FLOAT:
		return DXGI_FORMAT_R16G16_FLOAT;
	case Format::kRG32_FLOAT:
		return DXGI_FORMAT_R32G32_FLOAT;
	case Format::kRGBA8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case Format::kRGBA16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case Format::kRGBA32_FLOAT:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case Format::kSRGB:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

		// Depth Formats
	case Format::kDepth32FloatStencil8X24_UINT:
		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	case Format::kDepth32_FLOAT:
		return DXGI_FORMAT_D32_FLOAT;
	case Format::kDepth24Stencil8:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;

		// Typeless Formats
	case Format::kR32_FLOAT_X8X24_TYPELESS:
		return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
	case Format::kR32_TYPELESS:
		return DXGI_FORMAT_R32_TYPELESS;
	case Format::kR24_X8_TYPELESS:
		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

constexpr D3D12_BLEND ConvertToDXBlend(Blend blend)
{
	switch (blend)
	{
	case Blend::kZero:
		return D3D12_BLEND_ZERO;
	case Blend::kOne:
		return D3D12_BLEND_ONE;
	case Blend::kSrcColor:
		return D3D12_BLEND_SRC_COLOR;
	case Blend::kInvSrcColor:
		return D3D12_BLEND_INV_SRC_COLOR;
	case Blend::kSrcAlpha:
		return D3D12_BLEND_SRC_ALPHA;
	case Blend::kInvSrcAlpha:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case Blend::kDestAlpha:
		return D3D12_BLEND_DEST_ALPHA;
	case Blend::kInvDestAlpha:
		return D3D12_BLEND_INV_DEST_ALPHA;
	case Blend::kDestColor:
		return D3D12_BLEND_DEST_COLOR;
	case Blend::kInvDestColor:
		return D3D12_BLEND_INV_DEST_COLOR;
	case Blend::kSrcAlphaSat:
		return D3D12_BLEND_SRC_ALPHA_SAT;
	case Blend::kBlendFactor:
		return D3D12_BLEND_BLEND_FACTOR;
	case Blend::kInvBlendFactor:
		return D3D12_BLEND_INV_BLEND_FACTOR;
	case Blend::kSrc1Color:
		return D3D12_BLEND_SRC1_COLOR;
	case Blend::kInvSrc1Color:
		return D3D12_BLEND_INV_SRC1_COLOR;
	case Blend::kSrc1Alpha:
		return D3D12_BLEND_SRC1_ALPHA;
	case Blend::kInvSrc1Alpha:
		return D3D12_BLEND_INV_SRC1_ALPHA;
	default:
		return D3D12_BLEND_ZERO;
	}
}

constexpr D3D12_BLEND_OP ConvertToDXBlendOp(BlendOp blendOp)
{
	switch (blendOp)
	{
	case BlendOp::kAdd:
		return D3D12_BLEND_OP_ADD;
	case BlendOp::kSubtract:
		return D3D12_BLEND_OP_SUBTRACT;
	case BlendOp::kRevSubtract:
		return D3D12_BLEND_OP_REV_SUBTRACT;
	case BlendOp::kMin:
		return D3D12_BLEND_OP_MIN;
	case BlendOp::kMax:
		return D3D12_BLEND_OP_MAX;
	default:
		return D3D12_BLEND_OP_ADD;
	}
}

constexpr D3D12_COMPARISON_FUNC ConvertToDXComparisonFunc(ComparisonFunc compFunc)
{
	switch (compFunc)
	{
	case ComparisonFunc::kNever:
		return D3D12_COMPARISON_FUNC_NEVER;
	case ComparisonFunc::kLess:
		return D3D12_COMPARISON_FUNC_LESS;
	case ComparisonFunc::kEqual:
		return D3D12_COMPARISON_FUNC_EQUAL;
	case ComparisonFunc::kLessEqual:
		return D3D12_COMPARISON_FUNC_LESS_EQUAL;
	case ComparisonFunc::kGreater:
		return D3D12_COMPARISON_FUNC_GREATER;
	case ComparisonFunc::kNotEqual:
		return D3D12_COMPARISON_FUNC_NOT_EQUAL;
	case ComparisonFunc::kGreaterEqual:
		return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	case ComparisonFunc::kAlways:
		return D3D12_COMPARISON_FUNC_ALWAYS;
	default:
		return D3D12_COMPARISON_FUNC_NEVER;
	}
}

constexpr D3D12_CULL_MODE ConvertToDXCullMode(CullMode cull_mode)
{
	switch (cull_mode)
	{
	case CullMode::kNone:
		return D3D12_CULL_MODE_NONE;
	case Eden::CullMode::kFront:
		return D3D12_CULL_MODE_FRONT;
	case Eden::CullMode::kBack:
		return D3D12_CULL_MODE_BACK;
	default:
		return D3D12_CULL_MODE_NONE;
	}
}

constexpr D3D12_RESOURCE_STATES ConvertToDXResourceState(ResourceState state)
{
	switch (state)
	{
	case ResourceState::kCommon:
		return D3D12_RESOURCE_STATE_COMMON;
	case ResourceState::kRenderTarget:
		return D3D12_RESOURCE_STATE_RENDER_TARGET;
	case ResourceState::kPresent:
		return D3D12_RESOURCE_STATE_PRESENT;
	case ResourceState::kUnorderedAccess:
		return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	case ResourceState::kPixelShader:
		return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	case ResourceState::kNonPixelShader:
		return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	case ResourceState::kRead:
		return D3D12_RESOURCE_STATE_GENERIC_READ;
	case ResourceState::kCopyDest:
		return D3D12_RESOURCE_STATE_COPY_DEST;
	case ResourceState::kCopySrc:
		return D3D12_RESOURCE_STATE_COPY_SOURCE;
	case ResourceState::kDepthRead:
		return D3D12_RESOURCE_STATE_DEPTH_READ;
	case ResourceState::kDepthWrite:
		return D3D12_RESOURCE_STATE_DEPTH_WRITE;
	default:
		return D3D12_RESOURCE_STATE_COMMON;
	}
}
