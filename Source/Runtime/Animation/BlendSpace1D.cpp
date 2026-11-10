#include "EnginePCH.h"
#include "BlendSpace1D.h"

#include <cmath>

bool UBlendSpace1D::AddSample(const UAnimSequence* Animation, const FVector& SampleValue) 
{
    const float X = SampleValue.X;
    if (!Animation || !std::isfinite(X))
    {
        return false;
    }

    int32 InsertIndex = 0;
    while (InsertIndex < SampleData.Num() && SampleData[InsertIndex].SampleValue.X < X)
    {
        ++InsertIndex;
    }

    if (InsertIndex < SampleData.Num() && SampleData[InsertIndex].SampleValue.X == X)
    {
        return false;
    }

    SampleData.Insert(FBlendSample{ Animation, SampleValue }, InsertIndex);
    return true;
}

bool UBlendSpace1D::GetSampleWeights(const FVector& Input, TArray<FBlendSampleWeight>& OutWeights) const
{
    OutWeights.Reset();

    const double X = Input.X;
    if (SampleData.IsEmpty() || !std::isfinite(X))
    {
        return false;
    }

    if (X <= SampleData[0].SampleValue.X)
    {
        OutWeights.Add(FBlendSampleWeight{ 0, 1.0f });
        return true;
    }

    for (int32 IndexB = 1; IndexB < SampleData.Num(); ++IndexB)
    {
        const double PositionB = SampleData[IndexB].SampleValue.X;

        if (X > PositionB)
        {
            continue;
        }

        if (X == PositionB)
        {
            OutWeights.Add(FBlendSampleWeight{ IndexB, 1.0f });
            return true;
        }

        const int32 IndexA = IndexB - 1;

        const double PositionA = SampleData[IndexA].SampleValue.X;

        const float Alpha = static_cast<float>((X - PositionA) / (PositionB - PositionA));

        OutWeights.Add(FBlendSampleWeight{ IndexA, 1.0f - Alpha });

        OutWeights.Add(FBlendSampleWeight{ IndexB, Alpha });
        return true;
    }

    const int32 LastIndex = SampleData.Num() - 1;
    OutWeights.Add(FBlendSampleWeight{ LastIndex, 1.0f });
    return true;
}
