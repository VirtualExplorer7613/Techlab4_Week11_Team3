#pragma once

#include "BlendSpace.h"
#include "BlendSpace1D.generated.h"

UCLASS()
class UBlendSpace1D : public UBlendSpace
{
    GENERATED_BODY()

public:
    void ResetSamples() { SampleData.Reset(); }
    bool AddSample(const UAnimSequence* Animation, const FVector& SampleValue);
    bool GetSampleWeights(const FVector& Input, TArray<FBlendSampleWeight>& OutWeights) const override;
};
