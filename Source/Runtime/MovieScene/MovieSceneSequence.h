#pragma once

#include "UObject/Object.h"
#include "MovieScene/MovieSceneFwd.h"
#include "MovieSceneSequence.generated.h"

class UMovieScene;
class UWorld;

// UE UMovieSceneSequence: 시퀀스 에셋의 공통 부모. 바인딩을 실제 오브젝트로 찾는 방법은 하위 클래스가 정한다
UCLASS()
class UMovieSceneSequence : public UObject
{
	GENERATED_BODY()

public:
	UMovieSceneSequence() = default;

	virtual UMovieScene* GetMovieScene() const { return nullptr; }
	// UE LocateBoundObjects: 바인딩 GUID → 현재 월드의 오브젝트
	virtual UObject* LocateBoundObject(const FGuid& ObjectId, UWorld* Context) const { return nullptr; }
	virtual void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UWorld* Context) {}
	virtual void UnbindPossessableObjects(const FGuid& ObjectId) {}
	// UMovieScene 소멸자가 부른다. 들고 있는 포인터만 비운다
	virtual void OnMovieSceneDestroyed(UMovieScene* InMovieScene) {}
};
