#pragma once

#include "Shader.h"
#include "RenderStates.h"
#include "RenderTypes.h"


struct FPipelineState
{
	FPipelineState();
	~FPipelineState() = default;

	FShaderProgram* Shader = nullptr;
	EPrimitiveTopology       Topology = EPrimitiveTopology::TriangleList;
	ERasterizerState         RasterizerState = ERasterizerState::SolidBack;
	EBlendState              BlendState = EBlendState::Opaque;
	EDepthStencilState       DepthStencilState = EDepthStencilState::Default;
};