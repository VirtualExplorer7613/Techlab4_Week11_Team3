#pragma once

#include "Containers/Array.h"
#include "MovieScene/MovieSceneFrameTime.h"
#include "json.hpp"

// UE Curves/RealCurve.h와 같은 이름·순서
enum ERichCurveInterpMode : uint8
{
	RCIM_Linear,
	RCIM_Constant,
	RCIM_Cubic,
	RCIM_None
};

enum ERichCurveTangentMode : uint8
{
	RCTM_Auto,   // 이웃 키로 자동 계산
	RCTM_User,   // Arrive = Leave, 사용자가 지정
	RCTM_Break,  // Arrive, Leave 따로
	RCTM_None
};

// UE FMovieSceneTangentData. 탄젠트 단위는 "값 / 틱" (TickResolution 기준)
struct FMovieSceneTangentData
{
	float ArriveTangent = 0.0f;
	float LeaveTangent = 0.0f;
};

// UE FMovieSceneFloatValue: 기본 Cubic + Auto
struct FMovieSceneFloatValue
{
	float Value = 0.0f;
	FMovieSceneTangentData Tangent;
	ERichCurveInterpMode InterpMode = RCIM_Cubic;
	ERichCurveTangentMode TangentMode = RCTM_Auto;

	FMovieSceneFloatValue() = default;
	explicit FMovieSceneFloatValue(float InValue) : Value(InValue) {}
};

// UE FMovieSceneFloatChannel. 시간(Times)과 값(Values)을 같은 인덱스의 두 배열로 들고, 항상 시간순으로 정렬한다.
struct FMovieSceneFloatChannel
{
	bool Evaluate(FFrameTime InTime, float& OutValue) const;

	int32 GetNumKeys() const { return Times.Num(); }
	const TArray<FFrameNumber>& GetTimes() const { return Times; }
	TArray<FMovieSceneFloatValue>& GetValues() { return Values; }
	const TArray<FMovieSceneFloatValue>& GetValues() const { return Values; }

	int32 AddCubicKey(FFrameNumber InTime, float InValue, ERichCurveTangentMode TangentMode = RCTM_Auto);
	int32 AddLinearKey(FFrameNumber InTime, float InValue);
	int32 AddConstantKey(FFrameNumber InTime, float InValue);
	// 같은 시간 키가 있으면 값만 바꾸고, 없으면 Cubic 키를 추가한다 (시퀀서 "Key" 동작)
	int32 UpdateOrAddKey(FFrameNumber InTime, float InValue);
	int32 FindKey(FFrameNumber InTime, int32 Tolerance = 0) const;
	void RemoveKey(int32 Index);
	// 키 시간을 옮긴다. 정렬 뒤 새 인덱스를 돌려준다
	int32 SetKeyTime(int32 Index, FFrameNumber NewTime);
	void AutoSetTangents(float Tension = 0.0f);
	void Reset();

	void SetDefault(float InDefaultValue) { DefaultValue = InDefaultValue; bHasDefaultValue = true; }
	void RemoveDefault() { bHasDefaultValue = false; }
	bool GetDefault(float& OutValue) const { if (bHasDefaultValue) OutValue = DefaultValue; return bHasDefaultValue; }

	void SaveJson(nlohmann::json& Out) const;
	void LoadJson(const nlohmann::json& In);

private:
	int32 InsertKey(FFrameNumber InTime, const FMovieSceneFloatValue& InValue);

	TArray<FFrameNumber> Times;
	TArray<FMovieSceneFloatValue> Values;
	float DefaultValue = 0.0f;
	bool bHasDefaultValue = false;
};
