#pragma once

#include "Math/Matrix.h"
#include "Math/Vector4.h"

constexpr uint32 ObjectSlotBytes = 256;

struct FPerObjectConstants
{
	FMatrix World;
	FMatrix NormalMatrix;
	FVector4 ShadowFlags; // x: bReceiveShadow (GBuffer가 NormalRoughness.w 부호로 넘긴다)
};

inline FPerObjectConstants MakePerObjectConstants(const FMatrix& World, bool bReceiveShadow = true)
{
	FPerObjectConstants Result{};
	Result.World = World;
	Result.NormalMatrix = World.Inverse().GetTransposed();
	Result.ShadowFlags.X = bReceiveShadow ? 1.0f : 0.0f;
	return Result;
}

static_assert(sizeof(FPerObjectConstants) == sizeof(FMatrix) * 2 + sizeof(FVector4));
static_assert(sizeof(FPerObjectConstants) <= ObjectSlotBytes);

// UberLit.hlsl의 PerObject(b0)와 MaterialParams(b2)에 대응한다.
struct FForwardObjectConstants
{
	FMatrix World;
	FMatrix View;
	FMatrix Projection;
	FMatrix NormalMatrix;
	FVector4 ShadowFlags;
};

struct FForwardMaterialConstants
{
	FVector4 BaseColor;
	FVector4 Emissive;
	FVector4 SurfaceParams;
	FVector4 UVTransform;
	FVector4 SpecularColorShininess;
};

static_assert(sizeof(FForwardObjectConstants) == 272);
static_assert(sizeof(FForwardMaterialConstants) == 80);
static_assert(offsetof(FForwardMaterialConstants, SpecularColorShininess) == 64);

