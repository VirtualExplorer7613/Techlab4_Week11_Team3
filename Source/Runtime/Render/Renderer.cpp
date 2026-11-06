#include "EnginePCH.h"
#include "Render/Renderer.h"
#include "Render/RenderCommand.h"
#include <algorithm>
#include <chrono>
#include <unordered_map>

#include "Render/SkyboxRenderer.h"
#include "Render/PostProcess/HeightFogRenderer.h"
#include "Render/PostProcess/FXAARenderer.h"
#include "Render/PostProcess/ToneMapData.h"
#include "Render/PostProcess/CameraOverlayRenderer.h"
#include "Text/TextRenderer.h"
#include "DecalData.h"

namespace
{
	constexpr uint32 PerObjectSlotConstants = 16;
	constexpr uint32 MinPerObjectSlots = 1024;
}

FRenderer::FRenderer() = default;
FRenderer::~FRenderer() = default;

bool FRenderer::Init()
{
	bUsePerObjectSlots = RenderCommand::SupportsConstantBufferOffsets();
	ObjectCB = RenderCommand::CreateConstantBuffer(sizeof(FForwardObjectConstants));
	ViewCB = RenderCommand::CreateConstantBuffer(sizeof(FMatrix));
	MaterialCB = RenderCommand::CreateConstantBuffer(sizeof(FForwardMaterialConstants));
	if (!ObjectCB || !ViewCB || !MaterialCB) return false;
	GPUOcclusion.Init();
	return true;
}

void FRenderer::UploadViewMatrix(const FMatrix& ViewProjection)
{
	RenderCommand::UpdateBufferData(ViewCB.get(), &ViewProjection, sizeof(ViewProjection));
}

void FRenderer::BindBatch(const FMeshDrawBatch& Batch)
{
	RenderCommand::BindPipelineState(Batch.Pipeline);
	if (Batch.bForceWireframe)
		RenderCommand::SetRasterizerState(ERasterizerState::Wireframe);
	
	RenderCommand::BindVertexBuffer(Batch.VertexBuffer);
	RenderCommand::BindIndexBuffer(Batch.IndexBuffer);
	
	if (Batch.bBindViewConstants)
		RenderCommand::BindConstantBuffer(0, ViewCB.get(), EShaderBindFlagBits::Vertex);
	for (uint32 Index = 0; Index < Batch.Textures.Num(); ++Index)
		RenderCommand::BindShaderResource(Index, Batch.Textures[Index].Texture,
			Batch.Textures[Index].Stages);
	
	RenderCommand::BindSamplerState(0, Batch.Sampler, Batch.SamplerStages);
	
	if (Batch.MaterialDataSize)
	{
		RenderCommand::UpdateBufferData(MaterialCB.get(), Batch.MaterialData.data(), Batch.MaterialDataSize);
		RenderCommand::BindConstantBuffer(Batch.MaterialSlot, MaterialCB.get(), Batch.MaterialStages);
	}
}

void FRenderer::BindObject(const FMeshDrawBatch& Batch, const FMeshDrawBatch::FItem& Item)
{
	if (Batch.bUseObjectSlots && bUsePerObjectSlots && PerObjectSlotCB &&
		Item.ObjectSlot != InvalidObjectSlot)
	{
		RenderCommand::BindConstantBufferRange(Batch.ObjectBindingSlot, PerObjectSlotCB.get(),
			Item.ObjectSlot * PerObjectSlotConstants, PerObjectSlotConstants, Batch.ObjectStages);
	}
	else
	{
		RenderCommand::UpdateBufferData(ObjectCB.get(), Item.ObjectData.data(), Item.ObjectDataSize);
		RenderCommand::BindConstantBuffer(Batch.ObjectBindingSlot, ObjectCB.get(), Batch.ObjectStages);
	}
}

void FRenderer::DrawMeshes(std::span<const FMeshDrawBatch> Batches)
{
	uint32 PreviousTextureCount = 0;
	for (const FMeshDrawBatch& Batch : Batches)
	{
		BindBatch(Batch);
		if (PreviousTextureCount > Batch.Textures.Num())
			RenderCommand::UnbindShaderResources(static_cast<uint32>(Batch.Textures.Num()),
				PreviousTextureCount - static_cast<uint32>(Batch.Textures.Num()),
				EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
		
		PreviousTextureCount = static_cast<uint32>(Batch.Textures.Num());
		for (const FMeshDrawBatch::FItem& Item : Batch.Items)
		{
			BindObject(Batch, Item);
			RenderCommand::DrawIndexed(Item.IndexCount, Item.StartIndex);
		}
	}
}

void FRenderer::EnsurePerObjectSlotCapacity(uint32 SlotCount)
{
	if (SlotCount <= PerObjectSlotCapacity) return;
	uint32 NewCapacity = std::max(PerObjectSlotCapacity * 2, MinPerObjectSlots);
	while (NewCapacity < SlotCount) NewCapacity *= 2;
	PerObjectSlotCB = RenderCommand::CreateConstantBuffer(NewCapacity * ObjectSlotBytes);
	if (!PerObjectSlotCB || !PerObjectSlotCB->GetBuffer())
	{
		HTR_LOG(Warning, "Per-object constant buffer creation failed; using per-draw updates.");
		PerObjectSlotCB.reset();
		PerObjectSlotCapacity = 0;
		return;
	}
	PerObjectSlotCapacity = NewCapacity;
}

uint8* FRenderer::BeginObjectConstants(uint32 MaxSlots)
{
	if (!bUsePerObjectSlots || MaxSlots == 0) return nullptr;
	EnsurePerObjectSlotCapacity(MaxSlots);
	if (!PerObjectSlotCB) { bUsePerObjectSlots = false; return nullptr; }
	uint8* Destination = static_cast<uint8*>(RenderCommand::MapWriteDiscard(PerObjectSlotCB.get()));
	if (!Destination) bUsePerObjectSlots = false;
	return Destination;
}

void FRenderer::EndObjectConstants()
{
	if (PerObjectSlotCB) RenderCommand::Unmap(PerObjectSlotCB.get());
}

void FRenderer::InitScenePasses(const FString& SkyPanoramaPath)
{
	if (!SkyPanoramaPath.empty())
	{
		Skybox = MakeUnique<FSkyboxRenderer>();
		if (!Skybox->Init(SkyPanoramaPath))
		{
			HTR_LOG(Warning, "Skybox panorama not loaded: {}", SkyPanoramaPath);
			Skybox.reset();
		}
	}
	HeightFog = MakeUnique<FHeightFogRenderer>();
	HeightFog->Init(this);
	FXAA = MakeUnique<FFXAARenderer>();
	FXAA->Init(this);
	CameraOverlay = MakeUnique<FCameraOverlayRenderer>();   // FXAA->Init(this); 아래
	CameraOverlay->Init(this);
	ToneMapConstants = RenderCommand::CreateConstantBuffer(sizeof(FToneMapData));
	DecalConstants = RenderCommand::CreateConstantBuffer(sizeof(FDecalConstants));
	SceneText = MakeUnique<FTextRenderer>();
	SceneText->Init();
}

FOcclusionMeasureResult FRenderer::MeasureDrawVisibility(std::span<const FMeshDrawBatch> Batches)
{
	FOcclusionMeasureResult Result;
	uint32 Count = 0;
	for (const FMeshDrawBatch& Batch : Batches) Count += static_cast<uint32>(Batch.Items.Num());
	if (!Count) return Result;
	ID3D11Device* Device = FRenderDevice::Get()->GetDevice();
	ID3D11DeviceContext* Context = FRenderDevice::Get()->GetContext();
	if (!Device || !Context) return Result;
	const auto StartTime = std::chrono::high_resolution_clock::now();
	if (!DepthLessEqualReadOnly)
	{
		D3D11_DEPTH_STENCIL_DESC Desc{};
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		if (FAILED(Device->CreateDepthStencilState(&Desc, DepthLessEqualReadOnly.GetAddressOf()))) return Result;
	}
	while (OcclusionQueries.Num() < Count)
	{
		const D3D11_QUERY_DESC Desc{ D3D11_QUERY_OCCLUSION, 0 };
		ComPtr<ID3D11Query> Query;
		if (FAILED(Device->CreateQuery(&Desc, Query.GetAddressOf()))) return Result;
		OcclusionQueries.Add(std::move(Query));
	}
	uint32 QueryIndex = 0;
	for (const FMeshDrawBatch& Batch : Batches)
	{
		BindBatch(Batch);
		RenderCommand::SetBlendState(EBlendState::NoColorWrite);
		Context->OMSetDepthStencilState(DepthLessEqualReadOnly.Get(), 0);
		for (const FMeshDrawBatch::FItem& Item : Batch.Items)
		{
			BindObject(Batch, Item);
			ID3D11Query* Query = OcclusionQueries[QueryIndex++].Get();
			Context->Begin(Query);
			RenderCommand::DrawIndexed(Item.IndexCount, Item.StartIndex);
			Context->End(Query);
		}
	}
	std::unordered_map<const void*, bool> ObjectVisible;
	ObjectVisible.reserve(Count);
	QueryIndex = 0;
	for (const FMeshDrawBatch& Batch : Batches)
		for (const FMeshDrawBatch::FItem& Item : Batch.Items)
		{
			UINT64 Samples = 0;
			while (Context->GetData(OcclusionQueries[QueryIndex++].Get(), &Samples, sizeof(Samples), 0) == S_FALSE) {}
			const bool bVisible = Samples > 0;
			++Result.TotalDraws;
			Result.TotalTriangles += Item.IndexCount / 3;
			if (bVisible)
			{
				++Result.VisibleDraws;
				Result.VisibleTriangles += Item.IndexCount / 3;
			}
			if (Item.bOccludedByGpu)
			{
				++Result.GPUOccludedDraws;
				if (bVisible) ++Result.FalseCulls;
			}
			bool& bObjectVisible = ObjectVisible[Item.ObjectKey];
			bObjectVisible = bObjectVisible || bVisible;
		}
	Result.TotalObjects = static_cast<uint32>(ObjectVisible.size());
	for (const auto& [Key, bVisible] : ObjectVisible)
		Result.VisibleObjects += bVisible ? 1 : 0;
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
	Result.ElapsedMs = std::chrono::duration<double, std::milli>(
		std::chrono::high_resolution_clock::now() - StartTime).count();
	Result.bValid = true;
	return Result;
}

