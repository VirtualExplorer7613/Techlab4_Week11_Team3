#include "EnginePCH.h"
#include "Render/MeshPassProcessor.h"
#include "Render/SceneRenderData.h"
#include "Render/SceneView.h"
#include "Render/RenderResourceManager.h"
#include "Render/Mesh.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Core/EngineTimer.h"
#include <algorithm>
#include <cstring>

namespace
{
	float GetNormalEncoding(UTexture2D* Texture)
	{
		if (!Texture || !Texture->GetResource() || !Texture->GetResource()->GetSRV()) return 0.0f;
		switch (Texture->GetResource()->GetFormat())
		{
		case DXGI_FORMAT_BC5_UNORM: return 2.0f;
		case DXGI_FORMAT_BC5_SNORM: return 3.0f;
		default: return 1.0f;
		}
	}

	const FMatrix& PacketWorld(const FRenderPacket& Packet)
	{
		if (Packet.Proxy) return Packet.Proxy->GetLocalToWorld();
		return Packet.Model ? *Packet.Model : FMatrix::Identity;
	}

	uint64 GroupKey(const FStaticDrawGroup& Group)
	{
		return (static_cast<uint64>(Group.Material->SortID) << 47)
			| (static_cast<uint64>(Group.Mesh->SortID) << 31)
			| (static_cast<uint64>(Group.LODIndex & 0x3) << 29);
	}

	// 메시 종류(스태틱·스켈레탈)는 모르고 정점·인덱스 버퍼만 받는다
	bool BuildBatch(FMeshDrawBatch& Batch, FVertexBuffer* VertexBuffer, FIndexBuffer* IndexBuffer,
		UMaterial* Material, EMeshPassType Pass, const FSceneView& View, ELightingModel LightingModel,
		const void* OverrideParams = nullptr, uint32 OverrideSize = 0)
	{
		if (!Material) return false;
		Batch.VertexBuffer = VertexBuffer;
		Batch.IndexBuffer = IndexBuffer;
		if (!Batch.VertexBuffer || !Batch.IndexBuffer) return false;
		const bool bUsesUberShader = Pass == EMeshPassType::Forward || Pass == EMeshPassType::WorldNormal;
		if (bUsesUberShader)
		{
			const bool bWorldNormal = Pass == EMeshPassType::WorldNormal;
			FPipelineState* Base = FRenderResourceManager::GetPSO(bWorldNormal
				? EPSOType::StaticMesh_WorldNormal : EPSOType::StaticMesh_Forward);
			FShaderProgram* Shader = bWorldNormal ? (Base ? Base->Shader : nullptr)
				: FRenderResourceManager::GetForwardShaderProgram(LightingModel);
			if (!Base || !Shader || !Shader->VertexShader || !Shader->PixelShader) return false;
			Batch.Pipeline = *Base;
			Batch.Pipeline.Shader = Shader;
			Batch.bUseObjectSlots = false;
			Batch.bBindViewConstants = false;
			Batch.ObjectBindingSlot = 0;
			Batch.ObjectStages = EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel;
			Batch.MaterialSlot = 2;
			Batch.MaterialStages = EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel;
			Batch.SamplerStages = Batch.MaterialStages;
			Batch.bForceWireframe = !bWorldNormal && View.bIsWireframe;

			FForwardMaterialConstants Params{};
			Params.BaseColor = Material->BaseColor;
			Params.SpecularColorShininess = FVector4(Material->SpecularColor.X,
				Material->SpecularColor.Y, Material->SpecularColor.Z, std::max(1.0f, Material->Shininess));
			UTexture2D* Color = Material->Textures.IsEmpty() ? nullptr : Material->Textures[0];
			UTexture2D* Normal = Material->Textures.Num() > 1 ? Material->Textures[1] : nullptr;
			const bool bHasColor = Color && Color->GetResource() && Color->GetResource()->GetSRV();
			Params.SurfaceParams = FVector4(Material->bFlipNormalY ? 1.0f : 0.0f, 0.0f, bHasColor ? 1.0f : 0.0f,
				GetNormalEncoding(Normal));
			const FVector2 UV = Material->UVScrollSpeed * EngineTimer::GetTotalTime();
			Params.UVTransform = FVector4(UV.X, UV.Y, 1.0f, 1.0f);
			std::memcpy(Batch.MaterialData.data(), &Params, sizeof(Params));
			Batch.MaterialDataSize = sizeof(Params);
		}
		else
		{
			EPSOType Type = Material->PSOType;
			if (Pass == EMeshPassType::GBuffer && Type == EPSOType::StaticMesh_Opaque)
				Type = EPSOType::StaticMesh_GBuffer;
			else if (View.bIsWireframe && Type == EPSOType::StaticMesh_Opaque)
				Type = EPSOType::StaticMesh_Wireframe;
			FPipelineState* Pipeline = FRenderResourceManager::GetPSO(Type);
			if (!Pipeline) return false;
			Batch.Pipeline = *Pipeline;
			Batch.bForceWireframe = Pass == EMeshPassType::GBuffer && View.bIsWireframe;
			if (Material->PSOType == EPSOType::StaticMesh_Opaque ||
				Material->PSOType == EPSOType::StaticMesh_Translucent ||
				Material->PSOType == EPSOType::StaticMesh_Wireframe)
			{
				FStaticMeshMaterialParams Params{};
				Params.BaseColor = Material->BaseColor;
				Params.UVOffset = Material->UVScrollSpeed * EngineTimer::GetTotalTime();
				Params.TextureFlags.X = GetNormalEncoding(Material->Textures.Num() > 1 ? Material->Textures[1] : nullptr);
				Params.TextureFlags.Y = Material->bFlipNormalY ? 1.0f : 0.0f;
				std::memcpy(Batch.MaterialData.data(), &Params, sizeof(Params));
				Batch.MaterialDataSize = sizeof(Params);
			}
			else if (OverrideParams && OverrideSize <= Batch.MaterialData.size())
			{
				std::memcpy(Batch.MaterialData.data(), OverrideParams, OverrideSize);
				Batch.MaterialDataSize = OverrideSize;
			}
		}
		Batch.Sampler = Material->SamplerState;
		Batch.Textures.Reserve(Material->Textures.Num());
		for (int32 Index = 0; Index < Material->Textures.Num(); ++Index)
		{
			// Forward 조명은 t8 이상을 전역 리소스에 사용한다.
			if (bUsesUberShader && Index >= 8) break;
			UTexture2D* Texture = Material->Textures[Index];
			EShaderBindFlagBits Stages = EShaderBindFlagBits::Pixel;
			if (bUsesUberShader && Index == 1)
				Stages = EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel;
			if (bUsesUberShader && Index == 0 &&
				(!Texture || !Texture->GetResource() || !Texture->GetResource()->GetSRV()))
				Texture = nullptr;
			if (bUsesUberShader && Index == 1 && GetNormalEncoding(Texture) <= 0.5f)
				Texture = nullptr;
			Batch.Textures.Add({ Texture, Stages });
		}
		return true;
	}

	FMeshDrawBatch::FItem BuildItem(const FMatrix& World, const FSceneView& View,
		EMeshPassType Pass, uint32 Slot, uint32 StartIndex, uint32 IndexCount,
		const void* ObjectKey, bool bOccludedByGpu, bool bReceiveShadow)
	{
		FMeshDrawBatch::FItem Item{};
		Item.ObjectSlot = Slot;
		Item.StartIndex = StartIndex;
		Item.IndexCount = IndexCount;
		Item.ObjectKey = ObjectKey;
		Item.bOccludedByGpu = bOccludedByGpu;
		if (Pass == EMeshPassType::Forward || Pass == EMeshPassType::WorldNormal)
		{
			FForwardObjectConstants Constants{};
			Constants.World = World;
			Constants.View = View.ViewMatrix;
			Constants.Projection = View.ProjectionMatrix;
			Constants.NormalMatrix = World.Inverse().GetTransposed();
			Constants.ShadowFlags.X = static_cast<float>(bReceiveShadow);
			std::memcpy(Item.ObjectData.data(), &Constants, sizeof(Constants));
			Item.ObjectDataSize = sizeof(Constants);
		}
		else
		{
			const FPerObjectConstants Constants = MakePerObjectConstants(World, bReceiveShadow);
			std::memcpy(Item.ObjectData.data(), &Constants, sizeof(Constants));
			Item.ObjectDataSize = sizeof(Constants);
		}
		return Item;
	}

	FMeshDrawBatch::FItem BuildBatchItem(const FMatrix& World, const FMatrix& View, const FMatrix& Projection,
		uint32 Slot, uint32 StartIndex, uint32 IndexCount,
		const void* ObjectKey, bool bOccludedByGpu)
	{
		FMeshDrawBatch::FItem Item{};
		Item.ObjectSlot = Slot;
		Item.StartIndex = StartIndex;
		Item.IndexCount = IndexCount;
		Item.ObjectKey = ObjectKey;
		Item.bOccludedByGpu = bOccludedByGpu;

		FForwardObjectConstants Constants{};
		Constants.World = World;
		Constants.View = View;
		Constants.Projection = Projection;
		Constants.NormalMatrix = World.Inverse().GetTransposed();
		std::memcpy(Item.ObjectData.data(), &Constants, sizeof(Constants));
		Item.ObjectDataSize = sizeof(Constants);
		
		return Item;
	}

}

bool FMeshPassProcessor::Build(const FSceneView& View, const FRenderQueue& Queue,
	const FSceneRenderData* SceneData, EMeshPassType Pass,
	TArray<FMeshDrawBatch>& Out, ELightingModel LightingModel) const
{
	Out.Reset();
	if (Pass != EMeshPassType::Translucent && SceneData)
	{
		TArray<const FStaticDrawGroup*> Groups;
		for (const FSceneRenderData::FGatherChunk& Chunk : SceneData->GatherChunks)
			for (const FStaticDrawGroup& Group : Chunk.Groups)
				if (!Group.Items.empty()) Groups.Add(&Group);

		std::sort(Groups.begin(), Groups.end(),
			[](const FStaticDrawGroup* A, const FStaticDrawGroup* B) { return GroupKey(*A) < GroupKey(*B); }); // 재질, 메시, LOD 순으로 정렬

		for (const FStaticDrawGroup* Group : Groups)
		{
			FMeshDrawBatch Batch; // Render에게 전달하는 최종 구조체
			if (!BuildBatch(Batch, Group->Mesh->GetVertexBuffer(Group->LODIndex), Group->Mesh->GetIndexBuffer(Group->LODIndex),
				Group->Material, Pass, View, LightingModel))
				return false;

			Batch.Items.Reserve(static_cast<uint32>(Group->Items.size()));

			for (const FStaticDrawItem& Item : Group->Items)
				Batch.Items.Add(BuildItem(Item.Proxy->GetLocalToWorld(), View, Pass, Item.Slot,
					Item.StartIndex, Item.IndexCount, Item.Proxy, Item.bOccludedByGpu != 0, Item.Proxy->bReceiveShadow));
			Out.Add(std::move(Batch));
		}
	}
	const uint32 Begin = Pass == EMeshPassType::Translucent ? Queue.GetFirstTranslucentIndex() : 0;
	const uint32 End = Pass == EMeshPassType::Translucent ? Queue.Num() : Queue.GetFirstTranslucentIndex();
	Out.Reserve(Out.Num() + End - Begin);
	for (uint32 Index = Begin; Index < End; ++Index)
	{
		const FRenderPacket& Packet = Queue[Index];
		if (Pass == EMeshPassType::WorldNormal &&
			(!Packet.Material || Packet.Material->PSOType != EPSOType::StaticMesh_Opaque)) continue;
		FMeshDrawBatch Batch;
		FVertexBuffer* VertexBuffer = Packet.Mesh ? Packet.Mesh->GetVertexBuffer(Packet.LODIndex) : Packet.VertexBuffer;
		FIndexBuffer* IndexBuffer = Packet.Mesh ? Packet.Mesh->GetIndexBuffer(Packet.LODIndex) : Packet.IndexBuffer;
		if (!BuildBatch(Batch, VertexBuffer, IndexBuffer, Packet.Material, Pass, View,
			LightingModel, Packet.MaterialParamData, Packet.MaterialParamDataSize)) return false;

		const uint32 IndexCount = Packet.IndexCount ? Packet.IndexCount : Batch.IndexBuffer->GetIndexCount();
		const void* ObjectKey = Packet.Proxy ? static_cast<const void*>(Packet.Proxy) : static_cast<const void*>(&Packet);

		Batch.Items.Add(BuildItem(PacketWorld(Packet), View, Pass, Packet.Slot,
			Packet.StartIndex, IndexCount, ObjectKey, Packet.bOccludedByGpu, !Packet.Proxy || Packet.Proxy->bReceiveShadow));
		Out.Add(std::move(Batch));
	}
	return true;
}

bool FMeshPassProcessor::Build(const TArray<FPrimitiveSceneProxy*>& ShadowMeshes, EPSOType PSOType, TArray<FMeshDrawBatch>& OutBatches,
	const FVector& PreTranslation)
{
	OutBatches.Reset();

	const FPipelineState* Pipeline = FRenderResourceManager::GetPSO(PSOType);

	if (!Pipeline || !Pipeline->Shader || !Pipeline->Shader->VertexShader)
	{
		return false;
	}

	for (FPrimitiveSceneProxy* Proxy : ShadowMeshes)
	{
		if (!Proxy || !Proxy->GetRenderState())
		{
			continue;
		}

		if (Proxy->GetLODCount() == 0)
		{
			continue;
		}

		if (!Proxy->bCastsShadow) continue;

		// 우선 LOD 0 사용. 버퍼는 프록시가 메시 종류에 맞게 준다 (스켈레탈은 스키닝 결과)
		constexpr uint8 LOD = 0;
		const FCachedMeshLOD& CachedLOD = Proxy->GetLOD(LOD);

		FMeshDrawBatch Batch{};
		Batch.Pipeline = *Pipeline;
		Batch.VertexBuffer = Proxy->GetVertexBuffer(LOD);
		Batch.IndexBuffer = Proxy->GetIndexBuffer(LOD);
		Batch.bUseObjectSlots = false;
		Batch.bBindViewConstants = true;
		Batch.ObjectBindingSlot = 2;
		Batch.ObjectStages = EShaderBindFlagBits::Vertex;

		if (!Batch.VertexBuffer || !Batch.IndexBuffer)
		{
			continue; // 아직 버퍼가 없는 프록시(스키닝 결과를 만들기 전 등)는 이번 그림자에서 뺀다
		}

		for (uint32 Index = 0; Index < CachedLOD.NumSections; ++Index)
		{
			const FCachedMeshSection& Section = Proxy->GetSection(CachedLOD.FirstSection + Index);

			if (Section.IndexCount == 0)
			{
				continue;
			}

			FMeshDrawBatch::FItem Item{};
			Item.StartIndex = Section.StartIndex;
			Item.IndexCount = Section.IndexCount;
			Item.ObjectKey = Proxy;

			// ShadowDepth.hlsl은 법선(NormalMatrix)으로 기울기 bias를 구하므로 World만이 아니라 오브젝트 상수 전체를 올린다.
			FPerObjectConstants Constants = MakePerObjectConstants(Proxy->GetLocalToWorld());
			Constants.World.M[3][0] += PreTranslation.X;
			Constants.World.M[3][1] += PreTranslation.Y;
			Constants.World.M[3][2] += PreTranslation.Z;
			std::memcpy(Item.ObjectData.data(), &Constants, sizeof(Constants));
			Item.ObjectDataSize = sizeof(Constants);

			Batch.Items.Add(std::move(Item));
		}

		if (!Batch.Items.IsEmpty())
		{
			OutBatches.Add(std::move(Batch));
		}
	}

	return true;
}

bool FMeshPassProcessor::Build(const TArray<FPrimitiveSceneProxy*>& Receivers, UTexture2D* DecalTexture, EPSOType PSOType, TArray<FMeshDrawBatch>& OutBatches) const
{
	OutBatches.Reset();

	const FPipelineState* Pipeline = FRenderResourceManager::GetPSO(PSOType);
	if (!Pipeline || !Pipeline->Shader || !Pipeline->Shader->VertexShader || !Pipeline->Shader->PixelShader)
	{
		return false;
	}

	for (FPrimitiveSceneProxy* Receiver : Receivers)
	{
		if (!Receiver || !Receiver->GetRenderState())
		{
			continue;
		}

		uint32 LOD = 0;
		const FCachedMeshLOD& CachedLOD = Receiver->GetLOD(LOD);

		FMeshDrawBatch Batch{};
		Batch.Pipeline = *Pipeline;
		Batch.VertexBuffer = Receiver->GetVertexBuffer(LOD);
		Batch.IndexBuffer = Receiver->GetIndexBuffer(LOD);
		Batch.bUseObjectSlots = false;
		Batch.bBindViewConstants = true;
		Batch.ObjectBindingSlot = 2;
		Batch.ObjectStages = EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel;
		Batch.Textures = { { DecalTexture, EShaderBindFlagBits::Pixel } };

		if (!Batch.VertexBuffer || !Batch.IndexBuffer)
		{
			continue;
		}

		for (uint32 Index = 0; Index < CachedLOD.NumSections; ++Index)
		{
			const FCachedMeshSection& Section = Receiver->GetSection(CachedLOD.FirstSection + Index);

			if (Section.IndexCount == 0)
			{
				continue;
			}

			FMeshDrawBatch::FItem Item{};
			Item.StartIndex = Section.StartIndex;
			Item.IndexCount = Section.IndexCount;
			Item.ObjectKey = Receiver;

			FPerObjectConstants Constants = MakePerObjectConstants(Receiver->GetLocalToWorld(), Receiver->bReceiveShadow);
			std::memcpy(Item.ObjectData.data(), &Constants, sizeof(Constants));
			Item.ObjectDataSize = sizeof(Constants);

			Batch.Items.Add(std::move(Item));
		}

		if (!Batch.Items.IsEmpty())
		{
			OutBatches.Add(std::move(Batch));
		}
	}

	return !OutBatches.IsEmpty();
}

