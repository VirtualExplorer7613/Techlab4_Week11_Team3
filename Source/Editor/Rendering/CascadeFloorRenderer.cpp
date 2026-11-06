#include "EnginePCH.h"
#include "Editor/Rendering/CascadeFloorRenderer.h"
#include "Render/RenderCommand.h"
#include "Render/RenderResourceManager.h"
#include "Render/Texture2D.h"
#include <cstring>

void FCascadeFloorRenderer::Init()
{
	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/CascadeFloorShader.hlsl");
	PipelineState.Shader = Shader;
	PipelineState.Topology = EPrimitiveTopology::TriangleList;
	PipelineState.RasterizerState = ERasterizerState::SolidNone;
	PipelineState.BlendState = EBlendState::AlphaBlend;
	PipelineState.DepthStencilState = EDepthStencilState::Disabled;
	ConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FCascadeFloorData));
}

bool FCascadeFloorRenderer::OnRender(const FSceneView& View, const FSceneRenderData& RenderData,
	FTexture2D* DepthTarget, FTexture2D* ColorTarget)
{
	if (!DepthTarget || !ColorTarget || !Shader || !Shader->VertexShader ||
		!Shader->PixelShader || !ConstantBuffer ||
		!RenderData.LightParameterCounts[FSceneRenderData::DirectionalLightSlotIndex])
		return false;

	const uint32 Count = RenderData.CascadeShadowCount;
	if (!Count || Count > FSceneRenderData::MaxCascadeShadowCount ||
		!ColorTarget->GetWidth() || !ColorTarget->GetHeight() ||
		DepthTarget->GetWidth() != ColorTarget->GetWidth() ||
		DepthTarget->GetHeight() != ColorTarget->GetHeight())
		return false;

	FCascadeFloorData Data{};
	Data.InverseViewProjection = View.ViewProjectionMatrix.Inverse();
	Data.ViewProjection = View.ViewProjectionMatrix;
	Data.View = View.ViewMatrix;
	std::memcpy(Data.CascadeShadowDistances, RenderData.CascadeShadowDistances, sizeof(Data.CascadeShadowDistances));
	Data.Viewport = FVector4(static_cast<float>(ColorTarget->GetWidth()),
		static_cast<float>(ColorTarget->GetHeight()), 0.0f, 0.0f);
	Data.CascadeCount = Count;

	RenderCommand::BindPipelineState(PipelineState);
	RenderCommand::BindVertexBuffer(nullptr);
	RenderCommand::UpdateBufferData(ConstantBuffer.get(), &Data, sizeof(Data));
	RenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(0, DepthTarget, EShaderBindFlagBits::Pixel);
	RenderCommand::Draw(6);
	RenderCommand::BindShaderResource(0, static_cast<FTexture2D*>(nullptr), EShaderBindFlagBits::Pixel);
	return true;
}
