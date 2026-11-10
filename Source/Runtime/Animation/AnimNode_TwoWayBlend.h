#pragma once

#include "AnimNode_Base.h"

struct FPoseLink
{
	FAnimNode_Base* Node = nullptr;
};

struct FAnimNode_TwoWayBlend : public FAnimNode_Base
{
	FPoseLink A;          
	FPoseLink B;
	float Alpha = 0.0f;

	void Update(const FAnimationUpdateContext& Context) override;
	void Evaluate(FPoseContext& Output) override;
};
