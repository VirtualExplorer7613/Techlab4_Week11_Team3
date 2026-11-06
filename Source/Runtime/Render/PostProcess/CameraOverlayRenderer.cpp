#include "EnginePCH.h"
#include "Render/PostProcess/CameraOverlayRenderer.h"
#include "Render/RenderCommand.h"
#include "Render/RenderResourceManager.h"
#include "Render/SceneView.h"

void FCameraOverlayRenderer::Init(FRenderer* InRenderer)
{
    Renderer = InRenderer;
    Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/CameraOverlay.hlsl");

    PipelineState.Shader = Shader;
    PipelineState.Topology = EPrimitiveTopology::TriangleList;
    PipelineState.RasterizerState = ERasterizerState::SolidNone;
    PipelineState.BlendState = EBlendState::AlphaBlend;          // SrcAlpha / InvSrcAlpha (RenderDevice.cpp)
    PipelineState.DepthStencilState = EDepthStencilState::Disabled;

    ConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FCameraOverlayData));
    assert(Shader && Shader->VertexShader && Shader->PixelShader && ConstantBuffer);
}

bool FCameraOverlayRenderer::OnRender(const FSceneView& View, uint32 Width, uint32 Height, bool bVignette, bool bOverlay)
{
    if (!Shader || !ConstantBuffer || !Width || !Height) return false;

    FCameraOverlayData Data{};
    Data.FadeColor.V[0] = View.OverlayColor.R;
    Data.FadeColor.V[1] = View.OverlayColor.G;
    Data.FadeColor.V[2] = View.OverlayColor.B;
    Data.FadeColor.V[3] = bOverlay ? FMath::Clamp(View.OverlayColor.A, 0.0f, 1.0f) : 0.0f;
    Data.ViewSize.V[0] = static_cast<float>(Width);
    Data.ViewSize.V[1] = static_cast<float>(Height);
    Data.VignetteIntensity = bVignette ? View.VignetteIntensity : 0.0f;
    Data.VignettePower = 2.0f;
    Data.LetterboxAmount = bOverlay ? FMath::Clamp(View.LetterboxAmount, 0.0f, 1.0f) : 0.0f;
    Data.LetterboxAspect = std::max(View.LetterboxAspect, 0.1f);

    RenderCommand::BindPipelineState(PipelineState);
    RenderCommand::BindVertexBuffer(nullptr);
    RenderCommand::UpdateBufferData(ConstantBuffer.get(), &Data, sizeof(Data));
    RenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Pixel);
    RenderCommand::Draw(3);
    return true;
}