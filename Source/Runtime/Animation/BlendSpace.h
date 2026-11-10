#pragma once

#include "AnimationAsset.h"
#include "Containers/Array.h"
#include "Math/Vector.h"
#include "BlendSpace.generated.h"

class UAnimSequence;

struct FBlendSample
{
    const UAnimSequence* Animation = nullptr;
    FVector SampleValue = FVector::ZeroVector;
};

struct FBlendSampleWeight
{
    int32 SampleIndex;
    float Weight;
};

UCLASS()
class UBlendSpace : public UAnimationAsset
{
    GENERATED_BODY()

public:
    const TArray<FBlendSample>& GetSamples() const { return SampleData; }

    virtual bool GetSampleWeights(const FVector& Input, TArray<FBlendSampleWeight>& OutWeights) const = 0;

protected:
    TArray<FBlendSample> SampleData;
};