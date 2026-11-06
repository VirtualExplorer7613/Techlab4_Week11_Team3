#pragma once

#include "Render/Buffer.h"
#include "Render/PipelineState.h"

class FUIDrawList;

class FUIRenderer
{
public:
	void Init(uint32 InMaxQuads = 4096);
	// 현재 바인딩된 렌더 타깃(Width x Height)에 DrawList를 그린다.
	void Render(const FUIDrawList& DrawList, uint32 Width, uint32 Height);

private:
	struct FUIParams
	{
		FVector2 InvViewportSize;
		float PxRange = 0.0f;
		float Padding = 0.0f;
	};

	FShaderProgram* Shader = nullptr;
	FPipelineState PipelineState;

	TUniquePtr<FVertexBuffer> VertexBuffer;
	TUniquePtr<FIndexBuffer> IndexBuffer;
	TUniquePtr<FConstantBuffer> ParamBuffer;

	uint32 MaxVertices = 0;
	uint32 MaxIndices = 0;
};