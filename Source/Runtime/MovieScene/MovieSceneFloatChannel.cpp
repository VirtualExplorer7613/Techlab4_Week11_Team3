#include "EnginePCH.h"
#include "MovieScene/MovieSceneFloatChannel.h"

#include <algorithm>
#include <cmath>

namespace
{
	// UE GSequencerLinearCubicInterpolation 기본값 1: Linear 키 다음이 Cubic 키면 그 구간은 Cubic으로 계산한다
	constexpr bool bLinearCubicInterpolation = true;

	float BezierInterp(float P0, float P1, float P2, float P3, float Alpha)
	{
		const float U = 1.0f - Alpha;
		return U * U * U * P0 + 3.0f * U * U * Alpha * P1 + 3.0f * U * Alpha * Alpha * P2 + Alpha * Alpha * Alpha * P3;
	}
}

bool FMovieSceneFloatChannel::Evaluate(FFrameTime InTime, float& OutValue) const
{
	const int32 Num = Times.Num();
	if (Num == 0)
	{
		return GetDefault(OutValue);
	}

	// UE 기본 외삽(RCCE_Constant): 범위 밖은 끝 키 값
	const double Time = InTime.AsDecimal();
	if (Time <= Times[0].Value)       { OutValue = Values[0].Value; return true; }
	if (Time >= Times[Num - 1].Value) { OutValue = Values[Num - 1].Value; return true; }

	const auto Upper = std::upper_bound(Times.begin(), Times.end(), InTime.FrameNumber);
	const int32 Index2 = std::max(1, static_cast<int32>(Upper - Times.begin()));
	const int32 Index1 = Index2 - 1;
	const FMovieSceneFloatValue& Key1 = Values[Index1];
	const FMovieSceneFloatValue& Key2 = Values[Index2];

	const double Diff = static_cast<double>(Times[Index2].Value - Times[Index1].Value);
	if (Diff <= 0.0)
	{
		OutValue = Key2.Value;
		return true;
	}
	if (Key1.InterpMode == RCIM_Constant)
	{
		OutValue = Key1.Value;
		return true;
	}

	const float Alpha = static_cast<float>((Time - Times[Index1].Value) / Diff);
	if (Key1.InterpMode == RCIM_Linear && (!bLinearCubicInterpolation || Key2.InterpMode != RCIM_Cubic))
	{
		OutValue = Key1.Value + (Key2.Value - Key1.Value) * Alpha;
		return true;
	}

	// UE EvalForTwoKeys: P1 = P0 + Leave * Diff / 3, P2 = P3 - Arrive * Diff / 3 (탄젠트와 Diff 둘 다 틱 단위)
	const float OneThird = 1.0f / 3.0f;
	const float P0 = Key1.Value;
	const float P3 = Key2.Value;
	const float P1 = P0 + static_cast<float>(Key1.Tangent.LeaveTangent * Diff) * OneThird;
	const float P2 = P3 - static_cast<float>(Key2.Tangent.ArriveTangent * Diff) * OneThird;
	OutValue = BezierInterp(P0, P1, P2, P3, Alpha);
	return true;
}

int32 FMovieSceneFloatChannel::InsertKey(FFrameNumber InTime, const FMovieSceneFloatValue& InValue)
{
	const auto Upper = std::upper_bound(Times.begin(), Times.end(), InTime);
	const int32 Index = static_cast<int32>(Upper - Times.begin());
	Times.Insert(InTime, Index);
	Values.Insert(InValue, Index);
	AutoSetTangents();
	return Index;
}

int32 FMovieSceneFloatChannel::AddCubicKey(FFrameNumber InTime, float InValue, ERichCurveTangentMode TangentMode)
{
	FMovieSceneFloatValue Value(InValue);
	Value.InterpMode = RCIM_Cubic;
	Value.TangentMode = TangentMode;
	return InsertKey(InTime, Value);
}

int32 FMovieSceneFloatChannel::AddLinearKey(FFrameNumber InTime, float InValue)
{
	FMovieSceneFloatValue Value(InValue);
	Value.InterpMode = RCIM_Linear;
	return InsertKey(InTime, Value);
}

int32 FMovieSceneFloatChannel::AddConstantKey(FFrameNumber InTime, float InValue)
{
	FMovieSceneFloatValue Value(InValue);
	Value.InterpMode = RCIM_Constant;
	return InsertKey(InTime, Value);
}

int32 FMovieSceneFloatChannel::UpdateOrAddKey(FFrameNumber InTime, float InValue)
{
	if (const int32 Existing = FindKey(InTime); Existing >= 0)
	{
		Values[Existing].Value = InValue;
		AutoSetTangents();
		return Existing;
	}
	return AddCubicKey(InTime, InValue);
}

int32 FMovieSceneFloatChannel::FindKey(FFrameNumber InTime, int32 Tolerance) const
{
	for (int32 Index = 0; Index < Times.Num(); ++Index)
	{
		if (std::abs(Times[Index].Value - InTime.Value) <= Tolerance) return Index;
	}
	return -1;
}

void FMovieSceneFloatChannel::RemoveKey(int32 Index)
{
	if (Index < 0 || Index >= Times.Num()) return;
	Times.RemoveAt(Index);
	Values.RemoveAt(Index);
	AutoSetTangents();
}

int32 FMovieSceneFloatChannel::SetKeyTime(int32 Index, FFrameNumber NewTime)
{
	if (Index < 0 || Index >= Times.Num()) return -1;
	const FMovieSceneFloatValue Value = Values[Index];
	Times.RemoveAt(Index);
	Values.RemoveAt(Index);
	return InsertKey(NewTime, Value);
}

void FMovieSceneFloatChannel::AutoSetTangents(float Tension)
{
	// UE AutoCalcTangent: (1 - Tension) * ((P - Prev) + (Next - P)) / (NextTime - PrevTime). 시간은 틱
	const int32 Num = Times.Num();
	for (int32 Index = 0; Index < Num; ++Index)
	{
		FMovieSceneFloatValue& Key = Values[Index];
		if (Key.TangentMode != RCTM_Auto) continue;

		float Tangent = 0.0f;
		if (Index > 0 && Index < Num - 1 && Key.InterpMode == RCIM_Cubic)
		{
			const double PrevToNext = std::max(1.0, static_cast<double>(Times[Index + 1].Value - Times[Index - 1].Value));
			Tangent = static_cast<float>((1.0f - Tension) * (Values[Index + 1].Value - Values[Index - 1].Value) / PrevToNext);
		}
		Key.Tangent.ArriveTangent = Key.Tangent.LeaveTangent = Tangent;
	}
}

void FMovieSceneFloatChannel::Reset()
{
	Times.Empty();
	Values.Empty();
	bHasDefaultValue = false;
}

void FMovieSceneFloatChannel::SaveJson(nlohmann::json& Out) const
{
	nlohmann::json Keys = nlohmann::json::array();
	for (int32 Index = 0; Index < Times.Num(); ++Index)
	{
		const FMovieSceneFloatValue& Value = Values[Index];
		Keys.push_back({
			{ "Time", Times[Index].Value },
			{ "Value", Value.Value },
			{ "Interp", static_cast<int32>(Value.InterpMode) },
			{ "TangentMode", static_cast<int32>(Value.TangentMode) },
			{ "Arrive", Value.Tangent.ArriveTangent },
			{ "Leave", Value.Tangent.LeaveTangent } });
	}
	Out["Keys"] = std::move(Keys);
	if (bHasDefaultValue) Out["Default"] = DefaultValue;
}

void FMovieSceneFloatChannel::LoadJson(const nlohmann::json& In)
{
	Reset();
	if (!In.is_object()) return;
	for (const nlohmann::json& Item : In.value("Keys", nlohmann::json::array()))
	{
		FMovieSceneFloatValue Value(Item.value("Value", 0.0f));
		Value.InterpMode = static_cast<ERichCurveInterpMode>(Item.value("Interp", static_cast<int32>(RCIM_Cubic)));
		Value.TangentMode = static_cast<ERichCurveTangentMode>(Item.value("TangentMode", static_cast<int32>(RCTM_Auto)));
		Value.Tangent.ArriveTangent = Item.value("Arrive", 0.0f);
		Value.Tangent.LeaveTangent = Item.value("Leave", 0.0f);
		const FFrameNumber Time(Item.value("Time", 0));
		const auto Upper = std::upper_bound(Times.begin(), Times.end(), Time);
		const int32 Index = static_cast<int32>(Upper - Times.begin());
		Times.Insert(Time, Index);
		Values.Insert(Value, Index);
	}
	if (In.contains("Default")) SetDefault(In.value("Default", 0.0f));
	AutoSetTangents();
}
