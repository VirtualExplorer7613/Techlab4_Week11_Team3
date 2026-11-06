#pragma once

#include "Containers/Array.h"
#include "Core/Misc/Guid.h"
#include "MovieScene/MovieSceneFrameTime.h"
#include <functional>

class UObject;
class UWorld;
class UMovieSceneSequence;

// UE MovieSceneFwd.h와 같은 이름·순서
namespace EMovieScenePlayerStatus
{
	enum Type : int
	{
		Stopped,
		Playing,
		Scrubbing,
		Jumping,
		Stepping,
		Paused,
		MAX
	};
}

// UE EMovieSceneCompletionMode: 섹션 평가가 끝났을 때 원래 값으로 되돌릴지
enum class EMovieSceneCompletionMode : uint8
{
	KeepState,
	RestoreState,
	ProjectDefault,
};

// UE EMovieSceneCompletionModeOverride (PlaybackSettings.FinishCompletionStateOverride)
enum class EMovieSceneCompletionModeOverride : uint8
{
	None,
	ForceKeepState,
	ForceRestoreState,
};

// UE FMovieSceneEvaluationRange: 이번 평가 구간 (Previous, Current]
struct FMovieSceneEvaluationRange
{
	FFrameTime PreviousTime;
	FFrameTime CurrentTime;
	FFrameRate FrameRate;
	bool bHasPrevious = false;

	FFrameTime GetTime() const { return CurrentTime; }
	FFrameRate GetFrameRate() const { return FrameRate; }
};

// UE FMovieSceneContext
struct FMovieSceneContext
{
	FMovieSceneEvaluationRange Range;
	EMovieScenePlayerStatus::Type Status = EMovieScenePlayerStatus::Stopped;

	FFrameTime GetTime() const { return Range.CurrentTime; }
	FFrameTime GetPreviousTime() const { return Range.PreviousTime; }
	FFrameRate GetFrameRate() const { return Range.FrameRate; }
	EMovieScenePlayerStatus::Type GetStatus() const { return Status; }
	bool HasJumped() const { return !Range.bHasPrevious; }
};

// UE FMovieScenePreAnimatedState 축소판.
// 시퀀스가 처음 건드리기 직전 값을 (오브젝트, 종류)마다 한 번만 저장하고 Restore 때 되돌린다.
class FMovieScenePreAnimatedState
{
public:
	using FRestoreFunction = std::function<void()>;

	bool Contains(const void* Object, uint32 TypeID) const
	{
		for (const FEntry& Entry : Entries)
			if (Entry.Object == Object && Entry.TypeID == TypeID) return true;
		return false;
	}

	// MakeRestore는 처음 한 번만 불린다. 현재 값을 캡처한 복구 함수를 돌려줘야 한다
	template <typename FactoryType>
	void SavePreAnimatedState(const void* Object, uint32 TypeID, FactoryType&& MakeRestore)
	{
		if (!Object || Contains(Object, TypeID)) return;
		Entries.Add({ Object, TypeID, MakeRestore() });
	}

	void RestorePreAnimatedState()
	{
		// 저장 역순으로 되돌린다
		for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
			if (Entries[Index].Restore) Entries[Index].Restore();
		Entries.Empty();
	}

	// 대상 월드가 이미 없어진 경우: 되돌리지 않고 버린다
	void DiscardPreAnimatedState() { Entries.Empty(); }

private:
	struct FEntry
	{
		const void* Object = nullptr;
		uint32 TypeID = 0;
		FRestoreFunction Restore;
	};
	TArray<FEntry> Entries;
};

// UE IMovieScenePlayer: 트랙 평가가 플레이어에게 요청하는 것들
class IMovieScenePlayer
{
public:
	virtual ~IMovieScenePlayer() = default;

	virtual UWorld* GetPlaybackContext() const = 0;
	virtual UMovieSceneSequence* GetEvaluatedSequence() const = 0;
	virtual FMovieScenePreAnimatedState& GetPreAnimatedState() = 0;
	// 이번 평가에서 Camera Cut 트랙이 고른 카메라 오브젝트 (없으면 nullptr)
	virtual void SetCameraCutCandidate(UObject* CameraObject, float BlendTime) = 0;
	// 바인딩 GUID → 실제 오브젝트
	virtual UObject* FindBoundObject(const FGuid& BindingGuid) = 0;
	// 섹션 완료 모드가 ProjectDefault일 때 쓰는 값
	virtual bool ShouldRestoreState() const = 0;
};

// PreAnimatedState TypeID
namespace MovieScenePreAnimatedType
{
	constexpr uint32 Transform = 1;
	constexpr uint32 FloatProperty = 2;
	constexpr uint32 Fade = 3;
	constexpr uint32 Slomo = 4;
	constexpr uint32 CameraOverlay = 5;
}
