#include "EnginePCH.h"
#include "ForwardLightTiles.h"
#include "Render/RenderCommand.h"
#include "Render/RenderUtil.h"
#include "Render/Renderer.h"
#include "Render/SceneView.h"
#include "Render/Texture2D.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
	// 요청 개수를 담지 못할 때만 RW 구조화 버퍼를 다시 만든다.
	bool EnsureCapacity(TUniquePtr<FStructuredBuffer>& Buffer, uint32 Count, uint32 Stride)
	{
		if (Buffer && Count <= Buffer->GetCapacity()) return true;
		if (!Count || Count > UINT32_MAX / Stride) return false;
		TUniquePtr<FStructuredBuffer> NewBuffer = RenderCommand::CreateRWStructuredBuffer(Stride, Count);
		if (!NewBuffer || !NewBuffer->GetSRV() || !NewBuffer->GetUAV()) return false;
		Buffer = std::move(NewBuffer);
		return true;
	}
}

bool FForwardLightTileGrid::Init()
{
	CullingCB = RenderCommand::CreateConstantBuffer(sizeof(FGridConstants));
	LightingInfoCB = RenderCommand::CreateConstantBuffer(sizeof(FGridConstants));
	if (!CullingCB || !LightingInfoCB || !EnsureCapacity(Allocator, 2, sizeof(uint32))) return false;
	ComputeShader = RenderCommand::CreateComputeShader(
		RenderUtil::CompileShader("Resources/Shader/TileLightCullingCS.hlsli", "mainCS", EShaderType::Compute));
	ClusterComputeShader = RenderCommand::CreateComputeShader(
		RenderUtil::CompileShader("Resources/Shader/TileLightCullingCS.hlsli", "mainClusterCS", EShaderType::Compute));
	return ComputeShader && ComputeShader->IsValid() && ClusterComputeShader && ClusterComputeShader->IsValid();
}

bool FForwardLightTileGrid::PrepareBuild(const FSceneView& View, const FForwardLightingConstants& Lighting, 
	const FStructuredBuffer* SceneLights, const FTexture2D* Depth, ELightCullingMode InMode)
{
	bPixelTilesValid = false;
	Constants = {};
	Constants.GridView = View.ViewMatrix;
	Constants.GridSliceCount = 1;
	Constants.GridIsPerspective = View.bIsPerspective ? 1u : 0u;
	Constants.GridPointCount = Lighting.LightCounts[0];
	Constants.GridSpotCount = Lighting.LightCounts[1];
	std::memcpy(Constants.GridLightOffsets, Lighting.LightOffsets, sizeof(Constants.GridLightOffsets));

	if (!CullingCB || !LightingInfoCB || !Depth || !Depth->GetWidth() || !Depth->GetHeight()) return false;

	Constants.GridViewWidth = Depth->GetWidth();
	Constants.GridViewHeight = Depth->GetHeight();
	Constants.GridTileCountX = (Constants.GridViewWidth - 1) / LightTileSize + 1;
	Constants.GridTileCountY = (Constants.GridViewHeight - 1) / LightTileSize + 1;

	if (InMode == ELightCullingMode::Off || (!Constants.GridPointCount && !Constants.GridSpotCount))
		return true;

	const bool bClustered = InMode == ELightCullingMode::Clustered;
	FComputeShader* Shader = bClustered ? ClusterComputeShader.get() : ComputeShader.get();
	if (!Shader || !SceneLights || !SceneLights->GetSRV() || (!bClustered && !Depth->GetSRV()))
		return false;


	const FMatrix& Projection = View.ProjectionMatrix;
	if (!std::isfinite(Projection.M[1][0]) || std::abs(Projection.M[1][0]) < 1.0e-8f ||
		!std::isfinite(Projection.M[2][1]) || std::abs(Projection.M[2][1]) < 1.0e-8f ||
		!std::isfinite(Projection.M[0][2]) || !std::isfinite(Projection.M[3][2]) ||
		(View.bIsPerspective ? std::abs(Projection.M[3][2]) : std::abs(Projection.M[0][2])) < 1.0e-8f ||
		!std::isfinite(View.NearClip) || !std::isfinite(View.FarClip) ||
		View.NearClip <= 0.0f || View.FarClip <= View.NearClip)
		return false;
	Constants.GridProjectionParams = FVector4(1.0f / Projection.M[1][0], 1.0f / Projection.M[2][1],
		Projection.M[0][2], Projection.M[3][2]);
	Constants.GridSliceCount = bClustered ? ClusterDepthSlices : 1;

	// 원근은 log2(depth), 직교는 선형 depth에 scale과 bias를 적용한다.
	const float SliceCount = static_cast<float>(Constants.GridSliceCount);
	float DepthScale, DepthBias;
	if (View.bIsPerspective)
	{
		const float NearLog = std::log2(View.NearClip);
		DepthScale = SliceCount / (std::log2(View.FarClip) - NearLog);
		DepthBias = -NearLog * DepthScale;
	}
	else
	{
		DepthScale = SliceCount / (View.FarClip - View.NearClip);
		DepthBias = -View.NearClip * DepthScale;
	}
	Constants.GridDepthParams = FVector4(View.NearClip, View.FarClip, DepthScale, DepthBias);

	// 셀마다 메타데이터 하나를 만들고, 인덱스 풀은 평균 예산으로 확보한다.
	const uint64 CellCount = static_cast<uint64>(Constants.GridTileCountX) *
		Constants.GridTileCountY * Constants.GridSliceCount;
	const uint32 AverageLightsPerCell = bClustered ? ClusterAverageLightsPerCell : TiledAverageLightsPerCell;
	if (CellCount > UINT32_MAX / (AverageLightsPerCell * sizeof(uint32))) return false;

	const uint32 IndexBudget = static_cast<uint32>(CellCount) * AverageLightsPerCell;
	if (!EnsureCapacity(Ranges, static_cast<uint32>(CellCount), sizeof(uint32) * 4) ||
		!EnsureCapacity(PointIndices, std::max({ IndexBudget, Constants.GridPointCount, 1u }), sizeof(uint32)) ||
		!EnsureCapacity(SpotIndices, std::max({ IndexBudget, Constants.GridSpotCount, 1u }), sizeof(uint32)))
		return false;

	// 모든 준비가 끝났을 때만 요청한 모드를 활성화한다.
	Constants.GridPointIndexCapacity = PointIndices->GetCapacity();
	Constants.GridSpotIndexCapacity = SpotIndices->GetCapacity();
	Constants.GridMode = static_cast<uint32>(InMode);
	return true;
}

bool FForwardLightTileGrid::ExecuteBuild(FRenderer* Renderer, FTexture2D* Depth, const FStructuredBuffer* SceneLights)
{
	// Clustered는 깊이 텍스처 없이 고정 깊이 구간의 경계를 만든다.
	const bool bClustered = Constants.GridMode == static_cast<uint32>(ELightCullingMode::Clustered);
	FComputeShader* Shader = bClustered ? ClusterComputeShader.get() : ComputeShader.get();
	if (Constants.GridMode == static_cast<uint32>(ELightCullingMode::Off) ||
		!Renderer || !SceneLights || !SceneLights->GetSRV() || !Shader ||
		!Ranges || !PointIndices || !SpotIndices || !Allocator ||
		(!bClustered && (!Depth || !Depth->GetSRV())))
		return false;

	RenderCommand::UpdateBufferData(CullingCB.get(), &Constants, sizeof(Constants));
	RenderCommand::ClearUnorderedAccess(Allocator.get());

	// 기존 입력·출력 슬롯과 목록 버퍼를 유지한다.
	RenderCommand::BindComputeShader(Shader);
	RenderCommand::BindConstantBuffer(0, CullingCB.get(), EShaderBindFlagBits::Compute);
	RenderCommand::BindShaderResource(0, bClustered ? nullptr : Depth, EShaderBindFlagBits::Compute);
	RenderCommand::BindShaderResource(1, SceneLights, EShaderBindFlagBits::Compute);
	RenderCommand::BindUnorderedAccess(0, Ranges.get());
	RenderCommand::BindUnorderedAccess(1, PointIndices.get());
	RenderCommand::BindUnorderedAccess(2, SpotIndices.get());
	RenderCommand::BindUnorderedAccess(3, Allocator.get());

	const uint32 DispatchDepth = bClustered ? Constants.GridSliceCount : 1u;
	RenderCommand::Dispatch(Constants.GridTileCountX, Constants.GridTileCountY, DispatchDepth);

	RenderCommand::UnbindShaderResources(0, 2, EShaderBindFlagBits::Compute);
	RenderCommand::BindConstantBuffer(0, nullptr, EShaderBindFlagBits::Compute);
	RenderCommand::BindComputeShader(nullptr);
	return true;
}

void FForwardLightTileGrid::PreparePixelShader(FRenderer* Renderer, bool bBuildSucceeded)
{
	bPixelTilesValid = Constants.GridMode != static_cast<uint32>(ELightCullingMode::Off) &&
		bBuildSucceeded && Ranges && PointIndices && SpotIndices;
	if (!Renderer || !LightingInfoCB) return;

	FGridConstants PixelConstants = Constants;
	if (!bPixelTilesValid)
		PixelConstants.GridMode = static_cast<uint32>(ELightCullingMode::Off);
	RenderCommand::UpdateBufferData(LightingInfoCB.get(), &PixelConstants, sizeof(PixelConstants));

}

std::array<ID3D11Resource*, 4> FForwardLightTileGrid::GetWriteResources() const
{
	return { Ranges ? Ranges->GetBuffer() : nullptr, PointIndices ? PointIndices->GetBuffer() : nullptr,
		SpotIndices ? SpotIndices->GetBuffer() : nullptr, Allocator ? Allocator->GetBuffer() : nullptr };
}

std::array<FStructuredBuffer*, 3> FForwardLightTileGrid::GetPixelResources() const
{
	return bPixelTilesValid ? std::array<FStructuredBuffer*, 3>{ Ranges.get(), PointIndices.get(), SpotIndices.get() }
		: std::array<FStructuredBuffer*, 3>{};
}
