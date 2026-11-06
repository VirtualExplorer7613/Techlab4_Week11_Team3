#pragma once

#include "BoxComponent.h"
#include "IceVolumeComponent.generated.h"

// 빙판의 이동 영역. 바닥 메시와 별도로 플레이어 높이를 덮도록 배치한다.
UCLASS()
class UIceVolumeComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UIceVolumeComponent()
	{
		ShapeColor = FColor(100, 190, 255, 255);
		bBlockComponent = false;
	}

	FString GetIceType() const { return IceType; }
	void SetIceType(const FString& InIceType);  
	 
private:
	UPROPERTY(EditAnywhere)
	FString IceType = "default"; // default, slippery, reverse
};
