#pragma once

enum class EAttenuationDistanceModel : uint8
{
	Linear,
	Logarithmic,
	Inverse,
	NaturalSound,
};

struct FSoundAttenuationSettings
{
	bool bAttenuate = true;      // 거리 감쇠
	bool bSpatialize = true;     // 좌우 패닝
	EAttenuationDistanceModel DistanceAlgorithm = EAttenuationDistanceModel::Linear;
	float InnerRadius = 400.0f;        // 이 안은 볼륨 1 (UE 기본값, cm 단위. 엔진 월드 스케일에 맞출 것)
	float FalloffDistance = 3600.0f;   // InnerRadius 바깥으로 이만큼에서 0
	float dBAttenuationAtMax = -60.0f; // NaturalSound 전용

	// 0~1 볼륨 배율. UE FBaseAttenuationSettings::Evaluate (Attenuation.cpp:138)
	float Evaluate(float Distance) const;
};