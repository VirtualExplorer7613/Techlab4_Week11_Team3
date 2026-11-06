#pragma once

#include "MovieScene/MovieSceneTrack.h"
#include "MovieSceneEventTrack.generated.h"

// UE FMovieSceneEvent는 디렉터 블루프린트 함수를 가리킨다. 여기서는 엔진에 있는 동작만 고른다
enum class EMovieSceneEventKind : uint8
{
	HitStop,   // UWorld::StartHitStop(Duration, Dilation)
	Log,
};

struct FMovieSceneEvent
{
	EMovieSceneEventKind Kind = EMovieSceneEventKind::HitStop;
	float Duration = 0.1f;     // HitStop: 실제 시간(초)
	float Dilation = 0.05f;    // HitStop: 시간 배율
	FString Message;           // Log
};

// UE FMovieSceneEventChannel: 키 시간 + 이벤트
struct FMovieSceneEventChannel
{
	TArray<FFrameNumber> Times;
	TArray<FMovieSceneEvent> Events;

	int32 GetNumKeys() const { return Times.Num(); }
	int32 AddKey(FFrameNumber Time, const FMovieSceneEvent& Event);
	void RemoveKey(int32 Index);
	int32 SetKeyTime(int32 Index, FFrameNumber NewTime);
};

// UE UMovieSceneEventTriggerSection
UCLASS()
class UMovieSceneEventTriggerSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieSceneEventTriggerSection() = default;

	void Evaluate(const FMovieSceneContext& Context, IMovieScenePlayer& Player, UObject* BoundObject) override;
	void SaveJson(nlohmann::json& Out) const override;
	void LoadJson(const nlohmann::json& In) override;

	FMovieSceneEventChannel EventChannel;
};

// UE UMovieSceneEventTrack
UCLASS()
class UMovieSceneEventTrack : public UMovieSceneTrack
{
	GENERATED_BODY()

public:
	UMovieSceneEventTrack() = default;

	const char* GetDisplayName() const override { return "Events"; }
	UMovieSceneSection* CreateNewSection() override;
	void SaveJson(nlohmann::json& Out) const override;
	void LoadJson(const nlohmann::json& In) override;

	// UE와 같은 이름. 역재생이 없으므로 Backwards는 쓰이지 않는다
	bool bFireEventsWhenForwards = true;
	bool bFireEventsWhenBackwards = true;
};
