#pragma once

#include "UObject/Object.h"
#include "MovieScene/MovieSceneFwd.h"
#include "Core/Delegate/Delegate.h"
#include "MovieSceneSequencePlayer.generated.h"

// Windows.h의 GetCurrentTime 매크로가 UE와 같은 함수 이름을 바꿔 버리지 않게
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

class UMovieScene;
class UMovieSceneSequence;

// UE FMovieSceneSequenceLoopCount: 0 = 반복 없음, -1 = 무한
struct FMovieSceneSequenceLoopCount
{
	int32 Value = 0;
};

// UE FMovieSceneSequencePlaybackSettings (이 엔진에서 의미 있는 필드만)
struct FMovieSceneSequencePlaybackSettings
{
	bool bAutoPlay = false;
	FMovieSceneSequenceLoopCount LoopCount;
	float PlayRate = 1.0f;
	float StartTime = 0.0f;              // 초
	bool bDisableCameraCuts = false;
	bool bPauseAtEnd = false;
	EMovieSceneCompletionModeOverride FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::None;
};

// UE EUpdatePositionMethod
enum class EUpdatePositionMethod : uint8
{
	Play,    // 지나간 구간의 이벤트를 쏜다
	Jump,    // 이벤트 없이 그 시간으로
	Scrub,   // Jump + 상태를 Scrubbing으로
};

enum class EMovieScenePositionType : uint8
{
	Frame,   // DisplayRate 프레임
	Time,    // 초
};

// UE FMovieSceneSequencePlaybackParams
struct FMovieSceneSequencePlaybackParams
{
	FFrameTime Frame;
	float Time = 0.0f;
	EMovieScenePositionType PositionType = EMovieScenePositionType::Frame;
	EUpdatePositionMethod UpdateMethod = EUpdatePositionMethod::Jump;

	FMovieSceneSequencePlaybackParams() = default;
	FMovieSceneSequencePlaybackParams(FFrameTime InFrame, EUpdatePositionMethod InMethod)
		: Frame(InFrame), PositionType(EMovieScenePositionType::Frame), UpdateMethod(InMethod) {}
	FMovieSceneSequencePlaybackParams(float InTime, EUpdatePositionMethod InMethod)
		: Time(InTime), PositionType(EMovieScenePositionType::Time), UpdateMethod(InMethod) {}
};

using FOnMovieSceneSequencePlayerEvent = TDelegate<>;

// UE UMovieSceneSequencePlayer. 시간 커서를 움직이고 그 구간을 평가한다.
UCLASS()
class UMovieSceneSequencePlayer : public UObject, public IMovieScenePlayer
{
	GENERATED_BODY()

public:
	UMovieSceneSequencePlayer() = default;
	~UMovieSceneSequencePlayer() override;

	void Initialize(UMovieSceneSequence* InSequence, UWorld* InWorld, const FMovieSceneSequencePlaybackSettings& InSettings);

	void Play();
	void PlayLooping(int32 NumLoops = -1);
	void Pause();
	void Scrub();
	void Stop();
	void StopAtCurrentTime();
	void GoToEndAndStop();
	void SetPlaybackPosition(const FMovieSceneSequencePlaybackParams& PlaybackParams);

	bool IsPlaying() const { return Status == EMovieScenePlayerStatus::Playing; }
	bool IsPaused() const { return Status == EMovieScenePlayerStatus::Paused; }
	EMovieScenePlayerStatus::Type GetPlaybackStatus() const { return Status; }

	// UE와 같이 DisplayRate 기준 시간
	FQualifiedFrameTime GetCurrentTime() const;
	FQualifiedFrameTime GetDuration() const;
	FQualifiedFrameTime GetStartTime() const;
	FQualifiedFrameTime GetEndTime() const;
	FFrameRate GetFrameRate() const;
	double GetCurrentTimeSeconds() const;

	float GetPlayRate() const { return PlaybackSettings.PlayRate; }
	void SetPlayRate(float PlayRate) { PlaybackSettings.PlayRate = PlayRate; }
	FMovieSceneSequencePlaybackSettings& GetPlaybackSettings() { return PlaybackSettings; }

	// UE UMovieSceneSequencePlayer::Update. 액터 Tick(Time Dilation 적용된 DeltaTime)이나 에디터 패널이 부른다
	void Update(float DeltaSeconds);

	// 키를 고친 뒤 같은 시간으로 다시 평가 (이벤트 없음)
	void Refresh();
	// 대상 월드가 먼저 사라졌을 때: 되돌리지 않고 상태만 버린다
	void DiscardStateAndStop();

	// 지금 Camera Cut이 고른 카메라 (없으면 nullptr). 에디터 뷰포트 미리보기용
	UObject* GetActiveCameraObject() const { return CurrentCameraObject; }

	FOnMovieSceneSequencePlayerEvent OnPlay;
	FOnMovieSceneSequencePlayerEvent OnPause;
	FOnMovieSceneSequencePlayerEvent OnStop;
	FOnMovieSceneSequencePlayerEvent OnFinished;

	// 에디터 미리보기에서 true: 섹션 완료 모드가 ProjectDefault여도 Stop 때 되돌린다
	bool bRestoreStateByDefault = true;

	// IMovieScenePlayer
	UWorld* GetPlaybackContext() const override { return World; }
	UMovieSceneSequence* GetEvaluatedSequence() const override { return Sequence; }
	FMovieScenePreAnimatedState& GetPreAnimatedState() override { return PreAnimatedState; }
	void SetCameraCutCandidate(UObject* CameraObject, float BlendTime) override;
	UObject* FindBoundObject(const FGuid& BindingGuid) override;
	bool ShouldRestoreState() const override;

protected:
	// UE UMovieSceneSequencePlayer::UpdateCameraCut. ULevelSequencePlayer가 뷰 타깃을 바꾼다
	virtual void UpdateCameraCut(UObject* CameraObject, float BlendTime) {}

	UMovieScene* GetMovieScene() const;
	FFrameTime GetLastValidTime() const;
	void UpdateTimeCursorPosition(FFrameTime NewPosition, EUpdatePositionMethod Method);
	void EvaluateRange(FFrameTime PreviousTime, FFrameTime CurrentTime, bool bHasPrevious, EMovieScenePlayerStatus::Type EvalStatus);
	void StopInternal(FFrameTime TimeToResetTo);

	UMovieSceneSequence* Sequence = nullptr;
	UWorld* World = nullptr;
	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	EMovieScenePlayerStatus::Type Status = EMovieScenePlayerStatus::Stopped;
	FFrameTime PlayPosition;          // TickResolution 기준
	int32 CurrentNumLoops = 0;
	FMovieScenePreAnimatedState PreAnimatedState;

	UObject* CameraCutCandidate = nullptr;
	float CameraCutBlendTime = 0.0f;
	UObject* CurrentCameraObject = nullptr;
};
