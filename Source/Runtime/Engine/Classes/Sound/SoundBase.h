#pragma once
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "SoundBase.generated.h"

// 재생 가능한 모든 사운드의 기반. UE에서는 SoundCue, MetaSound도 여기서 파생된다.
UCLASS()
class USoundBase : public UObject
{
	GENERATED_BODY()
public:
	virtual float GetDuration() const { return 0.0f; }
};