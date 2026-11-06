#pragma once

#include "Render/PipelineState.h"
#include "Render/SceneRenderData.h"
#include "Render/SceneView.h"
#include "Render/RenderingInfo.h"

struct alignas(16) FCascadeFloorData
{
	FMatrix InverseViewProjection;
	FMatrix ViewProjection;
	FMatrix View;
	float CascadeShadowDistances[FSceneRenderData::MaxCascadeShadowCount];
	FVector4 Viewport;
	uint32 CascadeCount;
	uint32 Padding[3]{};
};

class FCascadeFloorRenderer
{
public:
	void Init();
	bool OnRender(const FSceneView& View, const FSceneRenderData& RenderData,
		FTexture2D* DepthTarget, FTexture2D* ColorTarget);

private:
	FShaderProgram* Shader = nullptr;
	FPipelineState PipelineState;
	TUniquePtr<FConstantBuffer> ConstantBuffer;
};
