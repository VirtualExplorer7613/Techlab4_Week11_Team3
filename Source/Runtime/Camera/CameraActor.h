#pragma once

#include "../GameFramework/Actor.h"
#include "UObject/Class.h"

#include "CameraComponent.h"
#include "CameraActor.generated.h"

//class UCameraComponent;

UCLASS()
class ACameraActor : public AActor
{
	GENERATED_BODY()

public:
	// 카메라 컴포넌트를 기본 서브오브젝트로 생성해 Actor 루트에 연결한다.
	ACameraActor();

	UCameraComponent* GetCameraComponent() const { return CameraComponent; };

private:
	UCameraComponent* CameraComponent;
};
