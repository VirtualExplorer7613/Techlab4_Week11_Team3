#pragma once

#include "MovieScene/MovieSceneSequencePlayer.h"
#include "LevelSequencePlayer.generated.h"

class ULevelSequence;
class ALevelSequenceActor;
class UCameraComponent;
class AActor;

// UE FOnLevelSequencePlayerCameraCutEvent
using FOnLevelSequencePlayerCameraCutEvent = TDelegate<UCameraComponent*>;

// UE ULevelSequencePlayer: 레벨 시퀀스 재생 + 카메라 컷을 게임 뷰포트에 적용
UCLASS()
class ULevelSequencePlayer : public UMovieSceneSequencePlayer
{
	GENERATED_BODY()

public:
	ULevelSequencePlayer() = default;

	void Initialize(ULevelSequence* InLevelSequence, UWorld* InWorld, const FMovieSceneSequencePlaybackSettings& InSettings);

	// UE ULevelSequencePlayer::CreateLevelSequencePlayer: 시퀀스 액터를 스폰하고 그 플레이어를 돌려준다
	static ULevelSequencePlayer* CreateLevelSequencePlayer(UWorld* World, const FString& LevelSequencePath,
		const FMovieSceneSequencePlaybackSettings& Settings, ALevelSequenceActor*& OutActor);

	// 카메라 컷이 바뀔 때. nullptr = 컷 끝(원래 카메라로)
	FOnLevelSequencePlayerCameraCutEvent OnCameraCut;

	UCameraComponent* GetActiveCameraComponent() const;

protected:
	void UpdateCameraCut(UObject* CameraObject, float BlendTime) override;

private:
	AActor* SavedViewTarget = nullptr;
	uint32 SavedViewTargetUUID = 0;
	bool bHasSavedViewTarget = false;
};
