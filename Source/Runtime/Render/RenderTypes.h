#pragma once

#include "Core/Types.h"

// RenderCommand가 그래픽스 API 타입 대신 받는 엔진 측 타입. D3D11 값으로의 변환은 D3D11TypeConversion.h가 맡는다.

enum class EPrimitiveTopology : uint8
{
	TriangleList,
	TriangleStrip,
	LineList,
	LineStrip,
	PointList,
};

enum class ETextureFormat : uint8
{
	Unknown,

	// 8비트 정규화
	R8_UNorm,
	R8G8_UNorm,
	R8G8B8A8_UNorm,
	R8G8B8A8_UNorm_SRGB,
	R8G8B8A8_SNorm,
	B8G8R8A8_UNorm,
	B8G8R8A8_UNorm_SRGB,
	A8_UNorm,

	// 16비트
	R16_Float,
	R16_UNorm,
	R16_UInt,
	R16G16_Float,
	R16G16_UNorm,
	R16G16B16A16_Float,
	R16G16B16A16_UNorm,

	// 32비트
	R32_Float,
	R32_UInt,
	R32_SInt,
	R32G32_Float,
	R32G32_UInt,
	R32G32B32_Float,
	R32G32B32A32_Float,
	R32G32B32A32_UInt,

	// 패킹
	R10G10B10A2_UNorm,
	R11G11B10_Float,

	// 깊이·스텐실
	D16_UNorm,
	D24_UNorm_S8_UInt,
	D32_Float,
	// 깊이로 쓰고 셰이더에서 읽을 수 있도록 만드는 typeless 리소스
	R16_Typeless,
	R24G8_Typeless,
	R32_Typeless,

	// 블록 압축
	BC1_UNorm,
	BC1_UNorm_SRGB,
	BC2_UNorm,
	BC2_UNorm_SRGB,
	BC3_UNorm,
	BC3_UNorm_SRGB,
	BC4_UNorm,
	BC4_SNorm,
	BC5_UNorm,
	BC5_SNorm,
	BC6H_UF16,
	BC6H_SF16,
	BC7_UNorm,
	BC7_UNorm_SRGB,
};

enum class ETextureBindFlags : uint32
{
	None = 0,
	ShaderResource = 1 << 0,
	RenderTarget = 1 << 1,
	DepthStencil = 1 << 2,
	UnorderedAccess = 1 << 3,
};

DEFINE_ENUM_OPERATORS(ETextureBindFlags);

enum class ETextureUsage : uint8
{
	Default,
	// 생성 시 초기 데이터로 채운 뒤 바꾸지 않는다.
	Immutable,
	// CPU가 매 프레임 덮어쓴다.
	Dynamic,
};

struct FTextureDesc
{
	uint32 Width = 0;
	uint32 Height = 0;
	// 0이면 전체 mip 체인을 만든다. bGenerateMips와 함께 쓴다.
	uint32 MipLevels = 1;
	uint32 ArraySize = 1;
	ETextureFormat Format = ETextureFormat::Unknown;
	ETextureBindFlags BindFlags = ETextureBindFlags::ShaderResource;
	ETextureUsage Usage = ETextureUsage::Default;
	// GPU에서 mip을 생성한다. RenderTarget 바인딩이 함께 필요하다.
	bool bGenerateMips = false;
};
