#include "EnginePCH.h"
#include "Render/SceneLightResources.h"
#include "Render/RenderCommand.h"
#include "Render/MeshPassProcessor.h"
#include "Render/Renderer.h"
#include "Components/LightComponentBase.h"
#include "SceneRenderData.h"
#include "Render/ShadowRendering.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
	static_assert(offsetof(FDeferredLightConstants, ViewportSize) == 96);
	static_assert(offsetof(FDeferredLightConstants, Ambient) == 112);
	static_assert(offsetof(FDeferredLightConstants, LightCounts) == 128);
	static_assert(offsetof(FDeferredLightConstants, LightOffsets) == 144);
	static_assert(offsetof(FDeferredLightConstants, SkyColorIntensity) == 160);
	static_assert(offsetof(FDeferredLightConstants, bSingleLightPass) == 176);
	static_assert(offsetof(FDeferredLightConstants, SingleLightIndex) == 180);
	static_assert(sizeof(FDeferredLightConstants) == 192);

	// 용량이 모자랄 때만 두 배로 키우고 Map 한 번으로 채운다.
	bool UploadStructured(TUniquePtr<FStructuredBuffer>& Buffer, const void* Data, uint32 Count, uint32 Stride)
	{
		if (!Count) return true;
		if (!Buffer || Buffer->GetCapacity() < Count)
		{
			const uint32 PreviousCapacity = Buffer ? Buffer->GetCapacity() : 0;
			const uint32 Maximum = UINT32_MAX / Stride;
			const uint32 Doubled = PreviousCapacity <= Maximum / 2
				? std::max(1u, PreviousCapacity * 2) : Maximum;
			auto NewBuffer = RenderCommand::CreateDynamicStructuredBuffer(std::max(Count, Doubled), Stride);
			if (!NewBuffer || !NewBuffer->GetBuffer() || !NewBuffer->GetSRV()) return false;
			Buffer = std::move(NewBuffer);
		}
		void* Destination = RenderCommand::MapWriteDiscard(Buffer.get());
		if (!Destination) return false;
		std::memcpy(Destination, Data, static_cast<size_t>(Count) * Stride);
		RenderCommand::Unmap(Buffer.get());
		return true;
	}
}

bool FSceneLightResources::Init()
{
	if (bInitialized) return true;

	ForwardLightCB = RenderCommand::CreateConstantBuffer(sizeof(FForwardLightingConstants));
	DeferredLightCB = RenderCommand::CreateConstantBuffer(sizeof(FDeferredLightConstants));
	ShadowDepthPassCB = RenderCommand::CreateConstantBuffer(sizeof(FShadowDepthPassConstants));
	ShadowProjectionCB = RenderCommand::CreateConstantBuffer(sizeof(FShadowProjectionConstants));

	if (!ForwardTileGrid.Init())
		HTR_LOG(Warning, "Forward tile culling compute shader unavailable");
	bInitialized = ForwardLightCB && DeferredLightCB;
	return bInitialized;
}

bool FSceneLightResources::UploadViewLights(const TArray<FLightRenderParameters>& Lights)
{
	return UploadStructured(SceneLightBuffer, Lights.GetData(), Lights.Num(), sizeof(FLightRenderParameters));
}

void FSceneLightResources::CreateShadowDepthAtlasIfNeed(uint32 SliceCount)
{
	SliceCount = std::max(SliceCount, 1u);
	if (ShadowDepthAtlas && ShadowDepthAtlas->GetArraySize() >= SliceCount) return;

	FTextureDesc Desc{};
	Desc.Width = ShadowAtlasSize;
	Desc.Height = ShadowAtlasSize;
	Desc.ArraySize = SliceCount;
	Desc.BindFlags = ETextureBindFlags::DepthStencil | ETextureBindFlags::ShaderResource;
	Desc.Format = ETextureFormat::R32_Typeless;   // DSV D32_FLOAT, SRV R32_FLOAT (Texture2D.cpp 변환표에 있음)
	ShadowDepthAtlas = RenderCommand::CreateTexture2DArray(Desc);
}

