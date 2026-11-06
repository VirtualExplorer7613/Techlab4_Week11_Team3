#pragma once

#include "MovieScene/MovieSceneSequence.h"
#include "LevelSequence.generated.h"

class UMovieScene;

// UE FLevelSequenceBindingReference: 바인딩 GUID ↔ 레벨 액터
// UE는 액터 경로(SoftObjectPath)를 저장한다. 여기서는 액터 이름 (PIE 복제 월드에서도 같은 이름)
struct FLevelSequenceBindingReference
{
	FGuid ObjectId;
	FString ActorName;
};

// UE ULevelSequence: 레벨 액터를 움직이는 시퀀스 에셋
UCLASS()
class ULevelSequence : public UMovieSceneSequence
{
	GENERATED_BODY()

public:
	ULevelSequence() = default;
	~ULevelSequence() override;

	// UE ULevelSequence::Initialize: 빈 UMovieScene을 만든다
	void Initialize();

	UMovieScene* GetMovieScene() const override { return MovieScene; }
	UObject* LocateBoundObject(const FGuid& ObjectId, UWorld* Context) const override;
	void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UWorld* Context) override;
	void UnbindPossessableObjects(const FGuid& ObjectId) override;
	void OnMovieSceneDestroyed(UMovieScene* InMovieScene) override { if (MovieScene == InMovieScene) MovieScene = nullptr; }
	const FString* FindBoundActorName(const FGuid& ObjectId) const;

	// UE는 .uasset 직렬화. 여기서는 JSON 내용의 .sequence 파일 (.widget처럼 전용 확장자. AssetManager는 .json을 폰트 아틀라스로 읽는다)
	bool SaveToFile(const FString& Path) const;
	bool LoadFromFile(const FString& Path);
	const FString& GetAssetPath() const { return AssetPath; }

	// UE LoadObject<ULevelSequence> 대응. 실패하면 nullptr. 호출한 쪽이 delete한다
	static ULevelSequence* LoadLevelSequence(const FString& Path);

private:
	UMovieScene* MovieScene = nullptr;
	TArray<FLevelSequenceBindingReference> BindingReferences;
	mutable FString AssetPath;
};
