#pragma once

#include "Render/RenderPacket.h"
#include "Render/ObjectConstants.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Engine/LightSceneProxy.h"
#include "Render/LightRenderData.h"
#include "Render/SceneLightResources.h"
#include "Render/SceneRendering.h"
#include "Math/Matrix.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>
#include "Math/QuadTree.h"

struct FRenderStats
{
	uint32 TotalPrimitives = 0;
	uint32 VisiblePrimitives = 0;
	uint32 DrawCalls = 0;
	uint64 Triangles = 0;
	uint32 LODCounts[4] = {};
	uint64 LODTriangles[4] = {};

	uint32 DirectionalCount = 0;
	uint32 PointCount = 0;
	uint32 SpotCount = 0;
	uint32 RectCount = 0;
	float AtlasCapacity = 0.0f;
	float ShadowTiles = 0.0f;
	uint32 ShadowViewsRedrawn = 0; // 이번 프레임에 다시 그린 그림자 뷰(큐브 면, cascade) 수
	uint32 ShadowViewsCached = 0;  // 캐시를 재사용해 건너뛴 그림자 뷰 수
	uint32 ShadowLightsSkippedOutsideView = 0;
	uint32 ShadowLightsSkippedNoReceiver = 0;
	uint32 ShadowLightsSkippedNoCaster = 0;
	uint32 ShadowCastersSkippedSmall = 0;
	uint32 ShadowDepthDraws = 0;    // 언리얼 구조 ShadowDepths 패스에서 그린 그림자 맵 수

	uint32 CascadeViewsRedrawn = 0;
	uint32 CascadeViewsCached = 0;

	uint32 DecalCount = 0;      // 필터를 통과해 판정한 데칼
	uint32 DecalsRendered = 0;  // Receiver가 하나 이상 있었던 데칼
	uint32 DecalReceivers = 0;  // Narrow Phase까지 통과한 메시 수 (데칼별 합)
	uint32 DecalDrawCalls = 0;  // 데칼 패스 DrawIndexed 수

	void Reset() { *this = FRenderStats(); }
};

// 뷰 하나의 데이터를 프레임 사이에도 보관한다. SceneRenderer는 현재 프레임 동안 이를 빌려 쓴다.
struct FSceneRenderData
{
	static constexpr int32 PointLightSlotIndex = 0;
	static constexpr int32 SpotLightSlotIndex = 1;
	static constexpr int32 DirectionalLightSlotIndex = 2;
	static constexpr int32 RectLightSlotIndex = 3;

	static constexpr uint32 ShadowMapSize = 4096;
	static constexpr uint32 MaxCascadeShadowCount = 4;
	static constexpr float MaxCascadeDistance = 1000.f;

	static constexpr float CascadeBlendRatio = 0.1f;

	struct alignas(64) FGatherChunk
	{
		TArray<FRenderPacket> Packets;              // 스태틱 묶음에 못 들어가는 것 (반투명 섹션, Renderer 없는 호출)
		TArray<uint32> SlowPathIndices;
		std::vector<FStaticDrawGroup> Groups;       // 불투명 스태틱 메시: (머티리얼, 메시, LOD)별 묶음. 항목만 매 프레임 비운다
		uint32 LastGroup = 0;                       // 바로 전 물체가 들어간 묶음 (연속한 물체는 대개 같은 묶음)
		uint32 StaticDrawCount = 0;
		uint32 LODCounts[4] = {};
		uint64 LODTriangles[4] = {};
	};

	FRenderQueue RenderQueue;
	FRenderStats RenderStats;
	TArray<FPrimitiveSceneProxy*> VisibleProxies;
	TArray<FLODSelectionInput> LODInputs;
	TArray<uint8> SelectedLODs;
	TArray<FGatherChunk> GatherChunks;

	TArray<FPerObjectConstants> ObjectConstants;
	FSceneLightResources ViewLightResources;

	FAmbientLightRenderData AmbientLight{};
	FSkyLightRenderData SkyLight{};
	bool bHasSkyLight = false;

	TArray<FLightRenderParameters> VisibleLights; // 모든 종류의 광원. 업로드할 때 종류별 구간으로 정렬한다
	TArray<const FLightSceneProxy*> VisibleLightProxies; // VisibleLights와 같은 순서의 Proxy (그림자 채널 배정에 씀)
	uint32 LightParameterOffsets[4]; // 라이트 리스트 내 구간의 offset
	uint32 LightParameterCounts[4]; // 라이트 리스트 내 구간의 count

	// CPU 전용 그림자 뷰 목록. Details 미리보기, 캐스케이드 경계 선, stat이 읽는다. GPU에 올리지 않는다.
	// FSceneRenderer::FinalizeShadowLights가 ProjectedShadows에서 광원 순서대로 채운다.
	TArray<FLightShadowRenderData> LightShadows;
	// VisibleLights와 같은 순서. 이 광원의 첫 그림자 뷰가 LightShadows의 몇 번째인지, 없으면 InvalidShadowView.
	static constexpr uint32 InvalidShadowView = 0xFFFFFFFFu;
	TArray<uint32> VisibleLightShadowViews;

	float CascadeShadowDistances[MaxCascadeShadowCount]{};
	uint32 CascadeShadowCount = 0;
	// CPU-only preview state for this view. Never uploaded with the light parameters.
	const ULightComponent* PreviewLight = nullptr;
	int32 PreviewLightIndex = INDEX_NONE;
	FLightShadowRange PreviewShadowRange{};

	// ---------- 언리얼 구조 그림자 (FSceneRenderer::InitDynamicShadows) ----------
	// 이번 프레임의 그림자 맵 전부. VisibleLightInfos가 이 안을 가리킨다.
	TArray<TUniquePtr<FProjectedShadowInfo>> ProjectedShadows;
	// FLightSceneInfo::Id로 찾는다. 그림자가 없는 라이트는 빈 항목.
	TArray<FVisibleLightInfo> VisibleLightInfos;
	// AllocateShadowDepthTargets가 쓴 깊이 아틀라스 슬라이스 수
	uint32 ShadowAtlasSliceCount = 0;

	bool GetShadowPreview(const ULightComponent* Light, FTexture2DArray*& OutAtlas,  // Shadow range를 바탕으로 lightshadow 인덱스에서 선택한 조명이 몇번째 index에서 viewcount가 몇개인지 알 수 있음
		TArray<FLightShadowRenderData>& OutShadows) const
	{
		OutAtlas = nullptr;
		OutShadows.Reset();
		const uint32 Start = PreviewShadowRange.StartIndex;
		const uint32 Count = PreviewShadowRange.ViewCount;
		if (!Light || Light != PreviewLight || !Count || Start > uint32(LightShadows.Num()) ||
			Count > uint32(LightShadows.Num()) - Start)
			return false;
		FTexture2DArray* Atlas = ViewLightResources.GetShadowDepthAtlas();
		if (!Atlas || !Atlas->GetSRV()) return false;
		for (uint32 i = 0; i < Count; ++i)
		{
			const auto& Shadow = LightShadows[Start + i];
			if (Shadow.AtlasIndex >= Atlas->GetArraySize() || Shadow.AtlasRect.Width <= 0 || Shadow.AtlasRect.Height <= 0)
			{
				OutShadows.Reset();
				return false;
			}
			OutShadows.Add(Shadow);
		}
		OutAtlas = Atlas;
		return true;
	}

	void ResetScene()
	{
		ResetFrame();
		for (FGatherChunk& Chunk : GatherChunks)
		{
			Chunk.Groups.clear();
			Chunk.LastGroup = 0;
		}
	}

	void ResetFrame()
	{
		RenderQueue.Reset();
		RenderStats.Reset();
		VisibleProxies.Reset();
		LODInputs.Reset();
		SelectedLODs.Reset();
		ObjectConstants.Reset();
		AmbientLight = {};
		SkyLight = {};
		bHasSkyLight = false;
		VisibleLights.Reset();
		VisibleLightProxies.Reset();
		VisibleLightShadowViews.Reset();
		LightShadows.Reset();
		PreviewLightIndex = INDEX_NONE;
		PreviewShadowRange = {};
		VisibleLightInfos.Reset();
		ProjectedShadows.Reset();
		ShadowAtlasSliceCount = 0;
		CascadeShadowCount = 0;
		std::fill(std::begin(CascadeShadowDistances), std::end(CascadeShadowDistances), 0.0f);
		std::memset(LightParameterOffsets, 0, sizeof(LightParameterOffsets));
		std::memset(LightParameterCounts, 0, sizeof(LightParameterCounts));

		for (FGatherChunk& Chunk : GatherChunks)
		{
			Chunk.Packets.Reset();
			Chunk.SlowPathIndices.Reset();
			Chunk.StaticDrawCount = 0;
			std::fill(std::begin(Chunk.LODCounts), std::end(Chunk.LODCounts), 0u);
			std::fill(std::begin(Chunk.LODTriangles), std::end(Chunk.LODTriangles), 0ull);
			for (FStaticDrawGroup& Group : Chunk.Groups) Group.Items.clear();
		}
	}

	// 광원을 종류별 구간으로 정렬하고 GPU에 올린다.
	// 그림자 채널(ShadowMapChannel)은 InitDynamicShadows 뒤에 FSceneRenderer::FinalizeShadowLights가 채우고 다시 올린다.
	void Commit(bool& bLightUploadSucceeded)
	{
		RenderQueue.Sort();

		if (VisibleLights.Num())
		{
			constexpr uint32 TypeCount = 4;
			constexpr uint32 TypeToSlot[TypeCount] = {
				FSceneRenderData::DirectionalLightSlotIndex,
				FSceneRenderData::PointLightSlotIndex,
				FSceneRenderData::SpotLightSlotIndex,
				FSceneRenderData::RectLightSlotIndex,
			};

			// Sort indices so the selected light's identity survives sorting, including identical lights.
			TArray<int32> LightOrder;
			LightOrder.SetNum(VisibleLights.Num());
			for (int32 i = 0; i < VisibleLights.Num(); ++i) LightOrder[i] = i;
			// stable_sort: 같은 종류끼리의 순서가 프레임마다 같아야 채널 배정과 아틀라스 배치가 흔들리지 않는다.
			std::stable_sort(LightOrder.begin(), LightOrder.end(), [&](int32 A, int32 B) {
				const uint32 ASlot = TypeToSlot[VisibleLights[A].Type];
				const uint32 BSlot = TypeToSlot[VisibleLights[B].Type];
				return ASlot < BSlot;
				});
			TArray<FLightRenderParameters> SortedLights;
			TArray<const FLightSceneProxy*> SortedProxies;
			int32 SortedPreviewIndex = INDEX_NONE;
			for (int32 Index : LightOrder)
			{
				if (Index == PreviewLightIndex) SortedPreviewIndex = SortedLights.Num();
				SortedLights.Add(VisibleLights[Index]);
				SortedProxies.Add(Index < VisibleLightProxies.Num() ? VisibleLightProxies[Index] : nullptr);
			}
			VisibleLights = std::move(SortedLights);
			VisibleLightProxies = std::move(SortedProxies);
			PreviewLightIndex = SortedPreviewIndex;

			for (FLightRenderParameters& Light : VisibleLights) Light.ShadowMapChannel = FLightRenderParameters::InvalidShadowMapChannel;

			// Light 배열 정리 및 구간 별 Offset, Count 기록
			int32 Offset = 0;
			int32 Count = 1;
			for (int32 i = 1; i < VisibleLights.Num(); i++)
			{
				const FLightRenderParameters& LightParams = VisibleLights[i];
				const FLightRenderParameters& PrevLightParams = VisibleLights[i - 1];

				if (LightParams.Type != PrevLightParams.Type)
				{
					int32 Slot = TypeToSlot[PrevLightParams.Type];
					LightParameterOffsets[Slot] = Offset;
					LightParameterCounts[Slot] = Count;

					Offset += Count;
					Count = 1;
				}
				else
				{
					Count++;
				}
			}

			const FLightRenderParameters& LightParams = VisibleLights.Last();
			int32 Slot = TypeToSlot[LightParams.Type];
			LightParameterOffsets[Slot] = Offset;
			LightParameterCounts[Slot] = Count;
		}

		// NOTE: 라이트 데이터를 GPU에 올림.
		bLightUploadSucceeded = ViewLightResources.Init();
		if (bLightUploadSucceeded)
			bLightUploadSucceeded &= ViewLightResources.UploadViewLights(VisibleLights);

		RenderStats.DirectionalCount = GetDirectionalLightCount();
		RenderStats.PointCount = GetPointLightCount();
		RenderStats.SpotCount = GetSpotLightCount();
	}

	inline bool HasDirectionalLight() { return LightParameterCounts[DirectionalLightSlotIndex]; }
	inline uint32 GetDirectionalLightOffset() { return LightParameterOffsets[DirectionalLightSlotIndex]; }
	inline uint32 GetDirectionalLightCount() { return LightParameterCounts[DirectionalLightSlotIndex]; }

	inline bool HasPointLights() { return LightParameterCounts[PointLightSlotIndex]; }
	inline uint32 GetPointLightOffset() { return LightParameterOffsets[PointLightSlotIndex]; }
	inline uint32 GetPointLightCount() { return LightParameterCounts[PointLightSlotIndex]; }

	inline bool HasSpotLights() { return LightParameterCounts[SpotLightSlotIndex]; }
	inline uint32 GetSpotLightOffset() { return LightParameterOffsets[SpotLightSlotIndex]; }
	inline uint32 GetSpotLightCount() { return LightParameterCounts[SpotLightSlotIndex]; }

	inline bool HasRectLights() { return LightParameterCounts[RectLightSlotIndex]; }
	inline bool HasLights() { return HasDirectionalLight() || HasPointLights() || HasSpotLights() || HasRectLights(); }
};
