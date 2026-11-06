#include "EnginePCH.h"
#include "MovieScene/MovieSceneSequencePlayer.h"

#include "MovieScene/MovieScene.h"
#include "MovieScene/MovieSceneSequence.h"
#include "Engine/World.h"
#include <cmath>

UMovieSceneSequencePlayer::~UMovieSceneSequencePlayer()
{
	PreAnimatedState.DiscardPreAnimatedState();
}

void UMovieSceneSequencePlayer::Initialize(UMovieSceneSequence* InSequence, UWorld* InWorld, const FMovieSceneSequencePlaybackSettings& InSettings)
{
	DiscardStateAndStop();
	Sequence = InSequence;
	World = InWorld;
	PlaybackSettings = InSettings;

	if (UMovieScene* MovieScene = GetMovieScene())
	{
		const FFrameTime StartOffset = MovieScene->GetTickResolution().AsFrameTime(std::max(0.0f, PlaybackSettings.StartTime));
		PlayPosition = FFrameTime(MovieScene->GetPlaybackRange().GetLowerBoundValue()) + StartOffset;
	}
}

UMovieScene* UMovieSceneSequencePlayer::GetMovieScene() const
{
	return Sequence ? Sequence->GetMovieScene() : nullptr;
}

FFrameTime UMovieSceneSequencePlayer::GetLastValidTime() const
{
	// 재생 범위는 [Start, End). 마지막으로 평가할 수 있는 틱은 End - 1
	const UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene) return FFrameTime();
	return FFrameTime(MovieScene->GetPlaybackRange().GetUpperBoundValue() - FFrameNumber(1));
}

void UMovieSceneSequencePlayer::Play()
{
	UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene || !World || IsPlaying()) return;

	const FFrameTime Start(MovieScene->GetPlaybackRange().GetLowerBoundValue());
	if (Status == EMovieScenePlayerStatus::Stopped || PlayPosition >= GetLastValidTime())
	{
		if (PlayPosition >= GetLastValidTime()) PlayPosition = Start;
		CurrentNumLoops = 0;
		// 시작 프레임 이벤트도 나가도록 바로 앞 틱을 이전 시간으로 둔다
		Status = EMovieScenePlayerStatus::Playing;
		EvaluateRange(FFrameTime(PlayPosition.FrameNumber - FFrameNumber(1)), PlayPosition, true, EMovieScenePlayerStatus::Playing);
	}
	Status = EMovieScenePlayerStatus::Playing;
	OnPlay.Broadcast();
}

void UMovieSceneSequencePlayer::PlayLooping(int32 NumLoops)
{
	PlaybackSettings.LoopCount.Value = NumLoops;
	Play();
}

void UMovieSceneSequencePlayer::Pause()
{
	if (!IsPlaying()) return;
	Status = EMovieScenePlayerStatus::Paused;
	OnPause.Broadcast();
}

void UMovieSceneSequencePlayer::Scrub()
{
	Status = EMovieScenePlayerStatus::Scrubbing;
}

void UMovieSceneSequencePlayer::Stop()
{
	const UMovieScene* MovieScene = GetMovieScene();
	StopInternal(MovieScene ? FFrameTime(MovieScene->GetPlaybackRange().GetLowerBoundValue()) : FFrameTime());
}

void UMovieSceneSequencePlayer::StopAtCurrentTime()
{
	StopInternal(PlayPosition);
}

void UMovieSceneSequencePlayer::GoToEndAndStop()
{
	SetPlaybackPosition(FMovieSceneSequencePlaybackParams(GetLastValidTime(), EUpdatePositionMethod::Jump));
	StopInternal(PlayPosition);
}

void UMovieSceneSequencePlayer::StopInternal(FFrameTime TimeToResetTo)
{
	if (CurrentCameraObject || CameraCutCandidate)
	{
		UpdateCameraCut(nullptr, 0.0f);
		CurrentCameraObject = CameraCutCandidate = nullptr;
	}
	// 섹션별 완료 모드로 저장된 것만 들어 있으므로 전부 되돌리면 된다
	PreAnimatedState.RestorePreAnimatedState();
	if (World) World->GetScene().UpdateAllTransforms();

	const bool bWasStopped = Status == EMovieScenePlayerStatus::Stopped;
	Status = EMovieScenePlayerStatus::Stopped;
	PlayPosition = TimeToResetTo;
	CurrentNumLoops = 0;
	if (!bWasStopped) OnStop.Broadcast();
}

void UMovieSceneSequencePlayer::DiscardStateAndStop()
{
	PreAnimatedState.DiscardPreAnimatedState();
	CurrentCameraObject = CameraCutCandidate = nullptr;
	Status = EMovieScenePlayerStatus::Stopped;
	CurrentNumLoops = 0;
}

void UMovieSceneSequencePlayer::SetPlaybackPosition(const FMovieSceneSequencePlaybackParams& PlaybackParams)
{
	UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene || !World) return;

	FFrameTime NewPosition = PlaybackParams.PositionType == EMovieScenePositionType::Time
		? MovieScene->GetTickResolution().AsFrameTime(PlaybackParams.Time)
		: ConvertFrameTime(PlaybackParams.Frame, MovieScene->GetDisplayRate(), MovieScene->GetTickResolution());

	const FFrameTime Start(MovieScene->GetPlaybackRange().GetLowerBoundValue());
	if (NewPosition < Start) NewPosition = Start;
	if (NewPosition > GetLastValidTime()) NewPosition = GetLastValidTime();

	if (PlaybackParams.UpdateMethod == EUpdatePositionMethod::Scrub) Status = EMovieScenePlayerStatus::Scrubbing;
	else if (Status == EMovieScenePlayerStatus::Stopped) Status = EMovieScenePlayerStatus::Paused;
	UpdateTimeCursorPosition(NewPosition, PlaybackParams.UpdateMethod);
}

void UMovieSceneSequencePlayer::Refresh()
{
	if (Status == EMovieScenePlayerStatus::Stopped) Status = EMovieScenePlayerStatus::Paused;
	EvaluateRange(PlayPosition, PlayPosition, false, EMovieScenePlayerStatus::Jumping);
}

void UMovieSceneSequencePlayer::Update(float DeltaSeconds)
{
	UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene || !IsPlaying()) return;
	const FFrameTime Delta = MovieScene->GetTickResolution().AsFrameTime(static_cast<double>(DeltaSeconds) * PlaybackSettings.PlayRate);
	UpdateTimeCursorPosition(PlayPosition + Delta, EUpdatePositionMethod::Play);
}

void UMovieSceneSequencePlayer::UpdateTimeCursorPosition(FFrameTime NewPosition, EUpdatePositionMethod Method)
{
	UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene) return;

	const FFrameTime Previous = PlayPosition;
	if (Method != EUpdatePositionMethod::Play)
	{
		PlayPosition = NewPosition;
		EvaluateRange(Previous, PlayPosition, false,
			Method == EUpdatePositionMethod::Scrub ? EMovieScenePlayerStatus::Scrubbing : EMovieScenePlayerStatus::Jumping);
		return;
	}

	const FFrameTime Start(MovieScene->GetPlaybackRange().GetLowerBoundValue());
	const FFrameTime LastValid = GetLastValidTime();
	if (NewPosition <= LastValid)
	{
		PlayPosition = NewPosition;
		EvaluateRange(Previous, PlayPosition, true, EMovieScenePlayerStatus::Playing);
		return;
	}

	// 끝을 넘음: 끝까지 평가(이벤트 처리) 후 반복 또는 종료
	EvaluateRange(Previous, LastValid, true, EMovieScenePlayerStatus::Playing);

	const int32 LoopCount = PlaybackSettings.LoopCount.Value;
	if (LoopCount < 0 || CurrentNumLoops < LoopCount)
	{
		++CurrentNumLoops;
		const double Duration = std::max(1.0, LastValid.AsDecimal() + 1.0 - Start.AsDecimal());
		const double Overplay = std::fmod(NewPosition.AsDecimal() - LastValid.AsDecimal() - 1.0, Duration);
		PlayPosition = FFrameTime::FromDecimal(Start.AsDecimal() + std::max(0.0, Overplay));
		EvaluateRange(FFrameTime(Start.FrameNumber - FFrameNumber(1)), PlayPosition, true, EMovieScenePlayerStatus::Playing);
		return;
	}

	PlayPosition = LastValid;
	if (PlaybackSettings.bPauseAtEnd)
	{
		Pause();
		return;
	}
	// UE FinishPlaybackInternal: 멈추고(완료 모드에 따라 복구) OnFinished
	StopInternal(LastValid);
	OnFinished.Broadcast();
}

void UMovieSceneSequencePlayer::EvaluateRange(FFrameTime PreviousTime, FFrameTime CurrentTime, bool bHasPrevious, EMovieScenePlayerStatus::Type EvalStatus)
{
	UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene || !World) return;

	FMovieSceneContext Context;
	Context.Range.PreviousTime = PreviousTime;
	Context.Range.CurrentTime = CurrentTime;
	Context.Range.FrameRate = MovieScene->GetTickResolution();
	Context.Range.bHasPrevious = bHasPrevious;
	Context.Status = EvalStatus;

	CameraCutCandidate = nullptr;
	CameraCutBlendTime = 0.0f;

	// 오브젝트 바인딩 트랙 → 전역 트랙 → 카메라 컷 순서
	for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
	{
		UObject* BoundObject = FindBoundObject(Binding.GetObjectGuid());
		if (!BoundObject) continue;
		for (UMovieSceneTrack* Track : Binding.GetTracks())
			if (Track && !Track->bIsEvalDisabled) Track->Evaluate(Context, *this, BoundObject);
	}
	for (UMovieSceneTrack* Track : MovieScene->GetTracks())
		if (Track && !Track->bIsEvalDisabled) Track->Evaluate(Context, *this, nullptr);
	if (UMovieSceneTrack* CutTrack = MovieScene->GetCameraCutTrack(); CutTrack && !CutTrack->bIsEvalDisabled)
		CutTrack->Evaluate(Context, *this, nullptr);

	if (!PlaybackSettings.bDisableCameraCuts && CameraCutCandidate != CurrentCameraObject)
	{
		CurrentCameraObject = CameraCutCandidate;
		UpdateCameraCut(CurrentCameraObject, CameraCutBlendTime);
	}

	// 월드 Tick 뒤에 평가할 수도 있으므로 렌더 전에 Transform을 반영
	World->GetScene().UpdateAllTransforms();
}

void UMovieSceneSequencePlayer::SetCameraCutCandidate(UObject* CameraObject, float BlendTime)
{
	CameraCutCandidate = CameraObject;
	CameraCutBlendTime = BlendTime;
}

UObject* UMovieSceneSequencePlayer::FindBoundObject(const FGuid& BindingGuid)
{
	return Sequence ? Sequence->LocateBoundObject(BindingGuid, World) : nullptr;
}

bool UMovieSceneSequencePlayer::ShouldRestoreState() const
{
	switch (PlaybackSettings.FinishCompletionStateOverride)
	{
	case EMovieSceneCompletionModeOverride::ForceKeepState:    return false;
	case EMovieSceneCompletionModeOverride::ForceRestoreState: return true;
	default:                                                   return bRestoreStateByDefault;
	}
}

FFrameRate UMovieSceneSequencePlayer::GetFrameRate() const
{
	const UMovieScene* MovieScene = GetMovieScene();
	return MovieScene ? MovieScene->GetDisplayRate() : FFrameRate(30, 1);
}

FQualifiedFrameTime UMovieSceneSequencePlayer::GetCurrentTime() const
{
	const UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene) return {};
	return FQualifiedFrameTime(ConvertFrameTime(PlayPosition, MovieScene->GetTickResolution(), MovieScene->GetDisplayRate()), MovieScene->GetDisplayRate());
}

FQualifiedFrameTime UMovieSceneSequencePlayer::GetStartTime() const
{
	const UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene) return {};
	return FQualifiedFrameTime(ConvertFrameTime(FFrameTime(MovieScene->GetPlaybackRange().GetLowerBoundValue()),
		MovieScene->GetTickResolution(), MovieScene->GetDisplayRate()), MovieScene->GetDisplayRate());
}

FQualifiedFrameTime UMovieSceneSequencePlayer::GetEndTime() const
{
	const UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene) return {};
	return FQualifiedFrameTime(ConvertFrameTime(FFrameTime(MovieScene->GetPlaybackRange().GetUpperBoundValue()),
		MovieScene->GetTickResolution(), MovieScene->GetDisplayRate()), MovieScene->GetDisplayRate());
}

FQualifiedFrameTime UMovieSceneSequencePlayer::GetDuration() const
{
	const UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene) return {};
	return FQualifiedFrameTime(ConvertFrameTime(FFrameTime(FFrameNumber(MovieScene->GetPlaybackRange().Size())),
		MovieScene->GetTickResolution(), MovieScene->GetDisplayRate()), MovieScene->GetDisplayRate());
}

double UMovieSceneSequencePlayer::GetCurrentTimeSeconds() const
{
	const UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene) return 0.0;
	return MovieScene->GetTickResolution().AsSeconds(PlayPosition - FFrameTime(MovieScene->GetPlaybackRange().GetLowerBoundValue()));
}
