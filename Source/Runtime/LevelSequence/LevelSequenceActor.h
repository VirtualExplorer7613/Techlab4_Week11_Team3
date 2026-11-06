#pragma once

#include "GameFramework/Actor.h"
#include "MovieScene/MovieSceneSequencePlayer.h"
#include "LevelSequenceActor.generated.h"

class ULevelSequence;
class ULevelSequencePlayer;

// UE ALevelSequenceActor: 레벨에 놓으면 BeginPlay 때 시퀀스를 불러와 재생한다.
// UE는 PlaybackSettings 구조체를 UPROPERTY로 노출한다. 이 엔진은 USTRUCT 리플렉션이 없어 필드를 펼쳐 둔다.
UCLASS()
class ALevelSequenceActor : public AActor
{
	GENERATED_BODY()

public:
	ALevelSequenceActor();
	~ALevelSequenceActor() override;

	void BeginPlay() override;
	void Tick(float DeltaTime) override;
	void EndPlay() override;

	// 시퀀스를 불러와 플레이어를 만든다 (BeginPlay에서 자동, CreateLevelSequencePlayer에서 직접)
	bool InitializePlayer();
	ULevelSequencePlayer* GetSequencePlayer() const { return SequencePlayer; }
	ULevelSequence* GetSequence() const { return LevelSequence; }

	FMovieSceneSequencePlaybackSettings MakePlaybackSettings() const;
	void ApplyPlaybackSettings(const FMovieSceneSequencePlaybackSettings& Settings);

	// UE TSoftObjectPtr<ULevelSequence> LevelSequenceAsset 대응
	UPROPERTY(EditAnywhere)
	FString LevelSequenceAsset = "Assets/Sequences/Demo.sequence";

	// PlaybackSettings
	UPROPERTY(EditAnywhere)
	bool bAutoPlay = true;
	UPROPERTY(EditAnywhere)
	int32 LoopCount = 0;            // 0 = 반복 없음, -1 = 무한
	UPROPERTY(EditAnywhere)
	float PlayRate = 1.0f;
	UPROPERTY(EditAnywhere)
	float StartTime = 0.0f;
	UPROPERTY(EditAnywhere)
	bool bDisableCameraCuts = false;
	UPROPERTY(EditAnywhere)
	bool bPauseAtEnd = false;
	UPROPERTY(EditAnywhere)
	bool bRestoreState = true;      // 끝나면 원래 값으로 (FinishCompletionStateOverride)

private:
	void ReleasePlayer();

	ULevelSequence* LevelSequence = nullptr;
	ULevelSequencePlayer* SequencePlayer = nullptr;
};
