#pragma once

struct alignas(16) FToneMapData
{
	float InvGamma = 1.0f / 2.2f;
	float bGammaCorretion = 1.0f;
	float Padding[2]{};
};

static_assert(sizeof(FToneMapData) == 16);

