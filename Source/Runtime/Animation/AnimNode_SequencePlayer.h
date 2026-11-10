#pragma once

#include "AnimNode_Base.h"

class UAnimSequence;

struct FAnimNode_SequencePlayer : public FAnimNode_Base
{
	void Update(const FAnimationUpdateContext& Context) override;
	void Evaluate(FPoseContext& Output) override;
	double GetDuration() const;

	const UAnimSequence* Sequence = nullptr;
	double CurrentTime = 0.0;
	bool bLoop = true;
};
