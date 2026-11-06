#include "EnginePCH.h"
#include "LevelSequence/LevelSequenceActor.h"

#include "LevelSequence/LevelSequence.h"
#include "LevelSequence/LevelSequencePlayer.h"
#include "Render/SceneView.h"
#include "UObject/UObjectGlobals.h"

ALevelSequenceActor::ALevelSequenceActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

ALevelSequenceActor::~ALevelSequenceActor()
{
	ReleasePlayer();
}

FMovieSceneSequencePlaybackSettings ALevelSequenceActor::MakePlaybackSettings() const
{
	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bAutoPlay = bAutoPlay;
	Settings.LoopCount.Value = LoopCount;
	Settings.PlayRate = PlayRate;
	Settings.StartTime = StartTime;
	Settings.bDisableCameraCuts = bDisableCameraCuts;
	Settings.bPauseAtEnd = bPauseAtEnd;
	Settings.FinishCompletionStateOverride = bRestoreState
		? EMovieSceneCompletionModeOverride::ForceRestoreState
		: EMovieSceneCompletionModeOverride::ForceKeepState;
	return Settings;
}

void ALevelSequenceActor::ApplyPlaybackSettings(const FMovieSceneSequencePlaybackSettings& Settings)
{
	bAutoPlay = Settings.bAutoPlay;
	LoopCount = Settings.LoopCount.Value;
	PlayRate = Settings.PlayRate;
	StartTime = Settings.StartTime;
	bDisableCameraCuts = Settings.bDisableCameraCuts;
	bPauseAtEnd = Settings.bPauseAtEnd;
	bRestoreState = Settings.FinishCompletionStateOverride != EMovieSceneCompletionModeOverride::ForceKeepState;
}

bool ALevelSequenceActor::InitializePlayer()
{
	ReleasePlayer();
	LevelSequence = ULevelSequence::LoadLevelSequence(LevelSequenceAsset);
	if (!LevelSequence) return false;
	SequencePlayer = NewObject<ULevelSequencePlayer>(this);
	SequencePlayer->Initialize(LevelSequence, GetWorld(), MakePlaybackSettings());
	return true;
}

void ALevelSequenceActor::ReleasePlayer()
{
	delete SequencePlayer;
	SequencePlayer = nullptr;
	delete LevelSequence;
	LevelSequence = nullptr;
}

void ALevelSequenceActor::BeginPlay()
{
	Super::BeginPlay();
	if (!SequencePlayer) InitializePlayer();
	if (SequencePlayer && bAutoPlay) SequencePlayer->Play();
}

void ALevelSequenceActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// DeltaTime은 월드 Time Dilation이 적용된 값 (UE도 월드 게임 시간으로 진행)
	if (SequencePlayer) SequencePlayer->Update(DeltaTime);
}

void ALevelSequenceActor::EndPlay()
{
	// 월드가 정리되는 중이라 다른 액터가 먼저 지워졌을 수 있다. 되돌리지 않고 버린다
	// 단 Fade/Letterbox 트랙이 쓰는 GCameraOverlayDebug는 월드 밖 전역이라 남기면 PIE 종료·레벨 이동 후에도 남는다.
	// Keep State로 끝난 값(예: 검은 화면)도 이 월드 안에서만 의미가 있으므로 초기화한다
	if (SequencePlayer)
	{
		GCameraOverlayDebug = FCameraOverlayDebug{};
		SequencePlayer->DiscardStateAndStop();
	}
	Super::EndPlay();
}
