#pragma once
#include "Render/Renderer.h"
#include "Render/PipelineState.h"

struct FSceneView;

struct alignas(16) FCameraOverlayData
{
	FVector4 FadeColor;
	FVector2 ViewSize;
	float VignetteIntensity;
	float VignettePower;
	float LetterboxAmount;
	float LetterboxAspect;
	float Padding[2];
};
static_assert(sizeof(FCameraOverlayData) == 48);

class FCameraOverlayRenderer
{
public:
	void Init(FRenderer* InRenderer);
	bool OnRender(const FSceneView& View, uint32 Width, uint32 Height, bool bVignette, bool bOverlay);
private:
	FRenderer* Renderer = nullptr;
	FShaderProgram* Shader = nullptr;
	FPipelineState PipelineState;
	TUniquePtr<FConstantBuffer> ConstantBuffer;
};

