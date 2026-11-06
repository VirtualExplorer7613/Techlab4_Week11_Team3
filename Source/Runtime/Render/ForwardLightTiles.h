#pragma once

#include "Core/Types.h"
#include "Render/Buffer.h"
#include "Render/Shader.h"
#include "Render/LightRenderData.h"
#include "Render/RenderEnums.h"
#include <array>

class FRenderer;
class FTexture2D;
struct FSceneView;

// 뷰별 타일 목록과 상수만 소유한다. GPU 명령은 FRenderer에 제출한다.
class FForwardLightTileGrid
{
public:
	bool Init();
	bool PrepareBuild(const FSceneView& View, const FForwardLightingConstants& Lighting,
		const FStructuredBuffer* SceneLights, const FTexture2D* Depth, ELightCullingMode InMode);
	bool ExecuteBuild(FRenderer* Renderer, FTexture2D* Depth, const FStructuredBuffer* SceneLights);
	void PreparePixelShader(FRenderer* Renderer, bool bBuildSucceeded);
	FConstantBuffer* GetPixelConstants() const { return LightingInfoCB.get(); }
	std::array<ID3D11Resource*, 4> GetWriteResources() const;
	std::array<FStructuredBuffer*, 3> GetPixelResources() const;
	void ResetPixelState() { bPixelTilesValid = false; }

private:
	static constexpr uint32 LightTileSize = 32;
	static constexpr uint32 ClusterDepthSlices = 24; // 클러스터에서 사용
	static constexpr uint32 TiledAverageLightsPerCell = 64;
	static constexpr uint32 ClusterAverageLightsPerCell = 16;

	struct FGridConstants
	{
		FMatrix GridView;
		FVector4 GridProjectionParams;
		uint32 GridViewWidth = 0, GridViewHeight = 0, GridTileCountX = 0, GridTileCountY = 0;
		uint32 GridMode = 0, GridSliceCount = 1, GridIsPerspective = 0, GridPadding = 0;
		uint32 GridPointCount = 0, GridSpotCount = 0, GridPointIndexCapacity = 0, GridSpotIndexCapacity = 0;
		uint32 GridLightOffsets[4]{};
		FVector4 GridDepthParams; // Near, Far, SliceScale, SliceBias
	};
	static_assert(sizeof(FGridConstants) == 160);

	TUniquePtr<FComputeShader> ComputeShader;
	TUniquePtr<FComputeShader> ClusterComputeShader;
	TUniquePtr<FConstantBuffer> CullingCB;
	TUniquePtr<FConstantBuffer> LightingInfoCB;
	TUniquePtr<FStructuredBuffer> Ranges, PointIndices, SpotIndices, Allocator;
	FGridConstants  Constants{};
	bool bPixelTilesValid = false;
};
