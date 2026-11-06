#include "EnginePCH.h"
#include "Audio/SoundAttenuation.h"
#include <algorithm>
#include <cmath>

float FSoundAttenuationSettings::Evaluate(float Distance) const
{
	if (!bAttenuate || Distance <= InnerRadius) return 1.0f;
	if (FalloffDistance <= 0.0f || Distance >= InnerRadius + FalloffDistance) return 0.0f;

	const float Alpha = std::clamp((Distance - InnerRadius) / FalloffDistance, 0.0f, 1.0f);
	float Result = 1.0f;
	switch (DistanceAlgorithm)
	{
	case EAttenuationDistanceModel::Linear:
		Result = 1.0f - Alpha;
		break;
	case EAttenuationDistanceModel::Logarithmic:
		Result = 0.5f * -std::log(std::max(Alpha, 1.0e-4f));
		break;
	case EAttenuationDistanceModel::Inverse:
		Result = 0.02f / std::max(Alpha, 1.0e-4f);
		break;
	case EAttenuationDistanceModel::NaturalSound:
		Result = std::pow(10.0f, (Alpha * dBAttenuationAtMax) / 20.0f);
		break;
	}
	return std::clamp(Result, 0.0f, 1.0f);
}