#pragma once

#include "UObject/Object.h"
#include "MovieScene/MovieSceneFwd.h"
#include "MovieScene/MovieSceneFloatChannel.h"
#include "json.hpp"
#include "MovieSceneSection.generated.h"

// UE FMovieSceneSectionEvalOptions
struct FMovieSceneSectionEvalOptions
{
	EMovieSceneCompletionMode CompletionMode = EMovieSceneCompletionMode::ProjectDefault;
};

// UE UMovieSceneSection. 트랙 위의 한 구간과 그 구간의 채널을 가진다.
// UE5는 섹션 데이터를 ECS 시스템이 평가한다. 여기서는 UE4 평가 템플릿처럼 섹션이 직접 평가한다.
UCLASS()
class UMovieSceneSection : public UObject
{
	GENERATED_BODY()

public:
	UMovieSceneSection() = default;
	~UMovieSceneSection() override;

	const FMovieSceneFrameRange& GetRange() const { return SectionRange; }
	void SetRange(const FMovieSceneFrameRange& InRange) { SectionRange = InRange; }
	bool HasStartFrame() const { return SectionRange.bHasLowerBound; }
	bool HasEndFrame() const { return SectionRange.bHasUpperBound; }
	FFrameNumber GetInclusiveStartFrame() const { return SectionRange.LowerBound; }
	FFrameNumber GetExclusiveEndFrame() const { return SectionRange.UpperBound; }
	void SetStartFrame(FFrameNumber Frame) { SectionRange.LowerBound = Frame; SectionRange.bHasLowerBound = true; }
	void SetEndFrame(FFrameNumber Frame) { SectionRange.UpperBound = Frame; SectionRange.bHasUpperBound = true; }
	void SetEndFrameOpen() { SectionRange.bHasUpperBound = false; }

	bool IsActive() const { return bIsActive; }
	void SetIsActive(bool bInActive) { bIsActive = bInActive; }

	EMovieSceneCompletionMode GetCompletionMode() const { return EvalOptions.CompletionMode; }
	// ProjectDefault를 플레이어 설정으로 풀어서 되돌릴지 결정
	bool ShouldRestoreState(const IMovieScenePlayer& Player) const;

	// 시퀀서 패널이 키 줄을 그리기 위한 채널 목록 (UE FMovieSceneChannelProxy 역할)
	virtual int32 GetNumChannels() const { return 0; }
	virtual FMovieSceneFloatChannel* GetChannel(int32 Index) { return nullptr; }
	virtual const char* GetChannelName(int32 Index) const { return ""; }

	// BoundObject: 오브젝트 바인딩 트랙이면 바인딩된 오브젝트, 전역 트랙이면 nullptr
	virtual void Evaluate(const FMovieSceneContext& Context, IMovieScenePlayer& Player, UObject* BoundObject) {}

	// "Key" 버튼: 대상의 현재 값을 Time에 키로 기록
	virtual void KeyCurrentValues(FFrameNumber Time, IMovieScenePlayer& Player, UObject* BoundObject) {}

	virtual void SaveJson(nlohmann::json& Out) const;
	virtual void LoadJson(const nlohmann::json& In);

	FMovieSceneSectionEvalOptions EvalOptions;

protected:
	FMovieSceneFrameRange SectionRange;
	bool bIsActive = true;
};
