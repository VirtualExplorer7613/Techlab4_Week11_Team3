#pragma once

#include "Render/RenderTypes.h"
#include <d3d11.h>

// 엔진 렌더 타입과 D3D11 타입 사이의 변환. D3D11 백엔드 구현 파일에서만 include한다.
namespace D3D11Type
{
	inline D3D11_PRIMITIVE_TOPOLOGY ToD3D11(EPrimitiveTopology Topology)
	{
		switch (Topology)
		{
		case EPrimitiveTopology::TriangleList:	return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case EPrimitiveTopology::TriangleStrip:	return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		case EPrimitiveTopology::LineList:		return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
		case EPrimitiveTopology::LineStrip:		return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
		case EPrimitiveTopology::PointList:		return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
		}
		return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	}

	struct FTextureFormatMapping
	{
		ETextureFormat Format;
		DXGI_FORMAT DXGIFormat;
	};

	// 양방향 변환이 같은 표를 쓰므로 포맷을 추가할 때는 여기에만 넣는다.
	inline constexpr FTextureFormatMapping TextureFormatMappings[] =
	{
		{ ETextureFormat::R8_UNorm,				DXGI_FORMAT_R8_UNORM },
		{ ETextureFormat::R8G8_UNorm,			DXGI_FORMAT_R8G8_UNORM },
		{ ETextureFormat::R8G8B8A8_UNorm,		DXGI_FORMAT_R8G8B8A8_UNORM },
		{ ETextureFormat::R8G8B8A8_UNorm_SRGB,	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },
		{ ETextureFormat::R8G8B8A8_SNorm,		DXGI_FORMAT_R8G8B8A8_SNORM },
		{ ETextureFormat::B8G8R8A8_UNorm,		DXGI_FORMAT_B8G8R8A8_UNORM },
		{ ETextureFormat::B8G8R8A8_UNorm_SRGB,	DXGI_FORMAT_B8G8R8A8_UNORM_SRGB },
		{ ETextureFormat::A8_UNorm,				DXGI_FORMAT_A8_UNORM },

		{ ETextureFormat::R16_Float,			DXGI_FORMAT_R16_FLOAT },
		{ ETextureFormat::R16_UNorm,			DXGI_FORMAT_R16_UNORM },
		{ ETextureFormat::R16_UInt,				DXGI_FORMAT_R16_UINT },
		{ ETextureFormat::R16G16_Float,			DXGI_FORMAT_R16G16_FLOAT },
		{ ETextureFormat::R16G16_UNorm,			DXGI_FORMAT_R16G16_UNORM },
		{ ETextureFormat::R16G16B16A16_Float,	DXGI_FORMAT_R16G16B16A16_FLOAT },
		{ ETextureFormat::R16G16B16A16_UNorm,	DXGI_FORMAT_R16G16B16A16_UNORM },

		{ ETextureFormat::R32_Float,			DXGI_FORMAT_R32_FLOAT },
		{ ETextureFormat::R32_UInt,				DXGI_FORMAT_R32_UINT },
		{ ETextureFormat::R32_SInt,				DXGI_FORMAT_R32_SINT },
		{ ETextureFormat::R32G32_Float,			DXGI_FORMAT_R32G32_FLOAT },
		{ ETextureFormat::R32G32_UInt,			DXGI_FORMAT_R32G32_UINT },
		{ ETextureFormat::R32G32B32_Float,		DXGI_FORMAT_R32G32B32_FLOAT },
		{ ETextureFormat::R32G32B32A32_Float,	DXGI_FORMAT_R32G32B32A32_FLOAT },
		{ ETextureFormat::R32G32B32A32_UInt,	DXGI_FORMAT_R32G32B32A32_UINT },

		{ ETextureFormat::R10G10B10A2_UNorm,	DXGI_FORMAT_R10G10B10A2_UNORM },
		{ ETextureFormat::R11G11B10_Float,		DXGI_FORMAT_R11G11B10_FLOAT },

		{ ETextureFormat::D16_UNorm,			DXGI_FORMAT_D16_UNORM },
		{ ETextureFormat::D24_UNorm_S8_UInt,	DXGI_FORMAT_D24_UNORM_S8_UINT },
		{ ETextureFormat::D32_Float,			DXGI_FORMAT_D32_FLOAT },
		{ ETextureFormat::R16_Typeless,			DXGI_FORMAT_R16_TYPELESS },
		{ ETextureFormat::R24G8_Typeless,		DXGI_FORMAT_R24G8_TYPELESS },
		{ ETextureFormat::R32_Typeless,			DXGI_FORMAT_R32_TYPELESS },

		{ ETextureFormat::BC1_UNorm,			DXGI_FORMAT_BC1_UNORM },
		{ ETextureFormat::BC1_UNorm_SRGB,		DXGI_FORMAT_BC1_UNORM_SRGB },
		{ ETextureFormat::BC2_UNorm,			DXGI_FORMAT_BC2_UNORM },
		{ ETextureFormat::BC2_UNorm_SRGB,		DXGI_FORMAT_BC2_UNORM_SRGB },
		{ ETextureFormat::BC3_UNorm,			DXGI_FORMAT_BC3_UNORM },
		{ ETextureFormat::BC3_UNorm_SRGB,		DXGI_FORMAT_BC3_UNORM_SRGB },
		{ ETextureFormat::BC4_UNorm,			DXGI_FORMAT_BC4_UNORM },
		{ ETextureFormat::BC4_SNorm,			DXGI_FORMAT_BC4_SNORM },
		{ ETextureFormat::BC5_UNorm,			DXGI_FORMAT_BC5_UNORM },
		{ ETextureFormat::BC5_SNorm,			DXGI_FORMAT_BC5_SNORM },
		{ ETextureFormat::BC6H_UF16,			DXGI_FORMAT_BC6H_UF16 },
		{ ETextureFormat::BC6H_SF16,			DXGI_FORMAT_BC6H_SF16 },
		{ ETextureFormat::BC7_UNorm,			DXGI_FORMAT_BC7_UNORM },
		{ ETextureFormat::BC7_UNorm_SRGB,		DXGI_FORMAT_BC7_UNORM_SRGB },
	};

	inline DXGI_FORMAT ToDXGI(ETextureFormat Format)
	{
		for (const FTextureFormatMapping& Mapping : TextureFormatMappings)
			if (Mapping.Format == Format) return Mapping.DXGIFormat;
		return DXGI_FORMAT_UNKNOWN;
	}

	// 표에 없는 DXGI 포맷은 Unknown이 된다.
	inline ETextureFormat FromDXGI(DXGI_FORMAT Format)
	{
		for (const FTextureFormatMapping& Mapping : TextureFormatMappings)
			if (Mapping.DXGIFormat == Format) return Mapping.Format;
		return ETextureFormat::Unknown;
	}

	inline UINT ToD3D11(ETextureBindFlags Flags)
	{
		UINT Result = 0;
		if (HasFlag(Flags, ETextureBindFlags::ShaderResource))	Result |= D3D11_BIND_SHADER_RESOURCE;
		if (HasFlag(Flags, ETextureBindFlags::RenderTarget))	Result |= D3D11_BIND_RENDER_TARGET;
		if (HasFlag(Flags, ETextureBindFlags::DepthStencil))	Result |= D3D11_BIND_DEPTH_STENCIL;
		if (HasFlag(Flags, ETextureBindFlags::UnorderedAccess))	Result |= D3D11_BIND_UNORDERED_ACCESS;
		return Result;
	}

	inline D3D11_TEXTURE2D_DESC ToD3D11(const FTextureDesc& Desc)
	{
		D3D11_TEXTURE2D_DESC Result{};
		Result.Width = Desc.Width;
		Result.Height = Desc.Height;
		Result.MipLevels = Desc.MipLevels;
		Result.ArraySize = Desc.ArraySize;
		Result.Format = ToDXGI(Desc.Format);
		Result.SampleDesc.Count = 1;
		Result.BindFlags = ToD3D11(Desc.BindFlags);
		switch (Desc.Usage)
		{
		case ETextureUsage::Default:	Result.Usage = D3D11_USAGE_DEFAULT; break;
		case ETextureUsage::Immutable:	Result.Usage = D3D11_USAGE_IMMUTABLE; break;
		case ETextureUsage::Dynamic:
			Result.Usage = D3D11_USAGE_DYNAMIC;
			Result.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			break;
		}
		if (Desc.bGenerateMips) Result.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
		return Result;
	}
}
