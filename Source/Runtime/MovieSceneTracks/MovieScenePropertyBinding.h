#pragma once

#include "Core/EngineString.h"

class UObject;
class AActor;
class USceneComponent;
class UCameraComponent;

// UE FMovieScenePropertyBinding
struct FMovieScenePropertyBinding
{
	FString PropertyName;
};

// UE FTrackInstancePropertyBindings 축소판.
// 바인딩된 액터와 그 컴포넌트에서 이름이 같은 float 프로퍼티를 찾는다.
// UE처럼 "Set" + 이름 세터가 있는 프로퍼티는 세터를 쓴다 (리플렉션이 함수를 모르므로 알려진 것만 등록).
namespace MovieScenePropertyBindings
{
	bool GetFloat(UObject* BoundObject, const FString& PropertyName, float& OutValue, UObject** OutOwner = nullptr);
	bool SetFloat(UObject* BoundObject, const FString& PropertyName, float Value);
}

namespace MovieSceneHelpers
{
	AActor* ActorFromBoundObject(UObject* BoundObject);
	USceneComponent* SceneComponentFromBoundObject(UObject* BoundObject);
	// UE MovieSceneHelpers::CameraComponentFromRuntimeObject
	UCameraComponent* CameraComponentFromRuntimeObject(UObject* RuntimeObject);
}
