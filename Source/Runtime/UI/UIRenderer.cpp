#include "EnginePCH.h"
#include "UI/UIRenderer.h"

#include "Render/RenderCommand.h"
#include "Render/RenderResourceManager.h"
#include "UI/UIDrawList.h"

void FUIRenderer::Init(uint32 InMaxQuads)
{
	MaxVertices = InMaxQuads * 4;
	MaxIndices = InMaxQuads * 6;

	VertexBuffer = RenderCommand::CreateDynamicVertexBuffer(sizeof(FUIVertex) * MaxVertices, sizeof(FUIVertex));
	IndexBuffer = RenderCommand::CreateDynamicIndexBuffer(MaxIndices);
	ParamBuffer = RenderCommand::CreateConstantBuffer(sizeof(FUIParams));

	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/UIShader.hlsl");

	PipelineState.Shader = Shader;
	PipelineState.Topology = EPrimitiveTopology::TriangleList;
	PipelineState.RasterizerState = ERasterizerState::SolidNone;      // Y 반전으로 감는 방향이 바뀌므로 컬링 끔
	PipelineState.BlendState = EBlendState::AlphaBlend;
	PipelineState.DepthStencilState = EDepthStencilState::Disabled;   // UI는 항상 맨 위
}

void FUIRenderer::Render(const FUIDrawList& DrawList, uint32 Width, uint32 Height)
{
	const TArray<FUIVertex>& Vertices = DrawList.GetVertices();
	const TArray<uint32>& Indices = DrawList.GetIndices();
	if (Indices.IsEmpty() || Width == 0 || Height == 0 || !Shader) return;

	if (Vertices.Num() > static_cast<int32>(MaxVertices) || Indices.Num() > static_cast<int32>(MaxIndices))
	{
		HTR_LOG(Warning, "UI quad count exceeds buffer ({} quads max)", MaxVertices / 4);
		return;
	}

	RenderCommand::UpdateBufferData(VertexBuffer.get(), Vertices.GetData(), sizeof(FUIVertex) * Vertices.Num());
	RenderCommand::UpdateBufferData(IndexBuffer.get(), Indices.GetData(), sizeof(uint32) * Indices.Num());

	RenderCommand::BindPipelineState(PipelineState);
	RenderCommand::BindSamplerState(0, ESamplerState::LinearClamp, EShaderBindFlagBits::Pixel);
	RenderCommand::BindVertexBuffer(VertexBuffer.get());
	RenderCommand::BindIndexBuffer(IndexBuffer.get());

	FUIParams Params;
	Params.InvViewportSize = FVector2(1.0f / Width, 1.0f / Height);

	for (const FUIDrawBatch& Batch : DrawList.GetBatches())
	{
		if (Batch.IndexCount == 0) continue;

		Params.PxRange = Batch.PxRange;
		RenderCommand::UpdateBufferData(ParamBuffer.get(), &Params, sizeof(FUIParams));
		RenderCommand::BindConstantBuffer(0, ParamBuffer.get(), EShaderBindFlagBits::Vertex);
		RenderCommand::BindConstantBuffer(0, ParamBuffer.get(), EShaderBindFlagBits::Pixel);
		if (Batch.Texture)
			RenderCommand::BindShaderResource(0, Batch.Texture, EShaderBindFlagBits::Pixel);

		RenderCommand::DrawIndexed(Batch.IndexCount, Batch.FirstIndex);
	}

	RenderCommand::UnbindShaderResources(0, 1, EShaderBindFlagBits::Pixel);
}