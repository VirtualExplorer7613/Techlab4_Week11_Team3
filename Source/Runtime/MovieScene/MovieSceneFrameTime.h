#pragma once

#include "Core/Types.h"
#include <cmath>
#include <limits>

// UE Misc/FrameNumber.h, FrameTime.h, FrameRate.h, QualifiedFrameTime.h 축소판.
// 시퀀스 시간은 TickResolution(기본 24000fps) 단위 정수 틱으로 저장한다.

struct FFrameNumber
{
	int32 Value = 0;

	constexpr FFrameNumber() = default;
	constexpr FFrameNumber(int32 InValue) : Value(InValue) {}

	friend bool operator==(FFrameNumber A, FFrameNumber B) { return A.Value == B.Value; }
	friend bool operator!=(FFrameNumber A, FFrameNumber B) { return A.Value != B.Value; }
	friend bool operator< (FFrameNumber A, FFrameNumber B) { return A.Value <  B.Value; }
	friend bool operator<=(FFrameNumber A, FFrameNumber B) { return A.Value <= B.Value; }
	friend bool operator> (FFrameNumber A, FFrameNumber B) { return A.Value >  B.Value; }
	friend bool operator>=(FFrameNumber A, FFrameNumber B) { return A.Value >= B.Value; }
	friend FFrameNumber operator+(FFrameNumber A, FFrameNumber B) { return A.Value + B.Value; }
	friend FFrameNumber operator-(FFrameNumber A, FFrameNumber B) { return A.Value - B.Value; }
};

// 정수 프레임 + 0~1 서브프레임
struct FFrameTime
{
	FFrameNumber FrameNumber;
	float SubFrame = 0.0f;

	FFrameTime() = default;
	FFrameTime(FFrameNumber InFrame) : FrameNumber(InFrame) {}
	FFrameTime(FFrameNumber InFrame, float InSubFrame) : FrameNumber(InFrame), SubFrame(InSubFrame) {}

	FFrameNumber GetFrame() const { return FrameNumber; }
	double AsDecimal() const { return static_cast<double>(FrameNumber.Value) + SubFrame; }
	FFrameNumber FloorToFrame() const { return FrameNumber; }
	FFrameNumber RoundToFrame() const { return SubFrame < 0.5f ? FrameNumber : FFrameNumber(FrameNumber.Value + 1); }

	static FFrameTime FromDecimal(double InDecimal)
	{
		const double Floored = std::floor(InDecimal);
		return FFrameTime(FFrameNumber(static_cast<int32>(Floored)), static_cast<float>(InDecimal - Floored));
	}

	friend bool operator< (const FFrameTime& A, const FFrameTime& B) { return A.AsDecimal() <  B.AsDecimal(); }
	friend bool operator<=(const FFrameTime& A, const FFrameTime& B) { return A.AsDecimal() <= B.AsDecimal(); }
	friend bool operator> (const FFrameTime& A, const FFrameTime& B) { return A.AsDecimal() >  B.AsDecimal(); }
	friend bool operator>=(const FFrameTime& A, const FFrameTime& B) { return A.AsDecimal() >= B.AsDecimal(); }
	friend FFrameTime operator+(const FFrameTime& A, const FFrameTime& B) { return FromDecimal(A.AsDecimal() + B.AsDecimal()); }
	friend FFrameTime operator-(const FFrameTime& A, const FFrameTime& B) { return FromDecimal(A.AsDecimal() - B.AsDecimal()); }
};

struct FFrameRate
{
	int32 Numerator = 24000;
	int32 Denominator = 1;

	constexpr FFrameRate() = default;
	constexpr FFrameRate(int32 InNumerator, int32 InDenominator) : Numerator(InNumerator), Denominator(InDenominator) {}

	double AsDecimal() const { return static_cast<double>(Numerator) / static_cast<double>(Denominator); }
	double AsInterval() const { return static_cast<double>(Denominator) / static_cast<double>(Numerator); }
	double AsSeconds(const FFrameTime& Time) const { return Time.AsDecimal() * AsInterval(); }
	FFrameTime AsFrameTime(double InSeconds) const { return FFrameTime::FromDecimal(InSeconds * AsDecimal()); }
	FFrameNumber AsFrameNumber(double InSeconds) const { return FFrameNumber(static_cast<int32>(std::floor(InSeconds * AsDecimal()))); }

	friend bool operator==(const FFrameRate& A, const FFrameRate& B) { return A.Numerator == B.Numerator && A.Denominator == B.Denominator; }
};

// UE ConvertFrameTime: 한 프레임레이트의 시간을 다른 프레임레이트로
inline FFrameTime ConvertFrameTime(const FFrameTime& SourceTime, const FFrameRate& SourceRate, const FFrameRate& DestinationRate)
{
	if (SourceRate == DestinationRate) return SourceTime;
	return DestinationRate.AsFrameTime(SourceRate.AsSeconds(SourceTime));
}

struct FQualifiedFrameTime
{
	FFrameTime Time;
	FFrameRate Rate;

	FQualifiedFrameTime() = default;
	FQualifiedFrameTime(const FFrameTime& InTime, const FFrameRate& InRate) : Time(InTime), Rate(InRate) {}

	double AsSeconds() const { return Rate.AsSeconds(Time); }
	FFrameTime ConvertTo(const FFrameRate& DesiredRate) const { return ConvertFrameTime(Time, Rate, DesiredRate); }
};

// UE TRange<FFrameNumber> 축소판: [Lower, Upper). 열린 쪽은 무한
struct FMovieSceneFrameRange
{
	FFrameNumber LowerBound;
	FFrameNumber UpperBound;
	bool bHasLowerBound = false;
	bool bHasUpperBound = false;

	static FMovieSceneFrameRange All() { return {}; }
	static FMovieSceneFrameRange Make(FFrameNumber Lower, FFrameNumber Upper)
	{
		FMovieSceneFrameRange Range;
		Range.LowerBound = Lower;
		Range.UpperBound = Upper;
		Range.bHasLowerBound = Range.bHasUpperBound = true;
		return Range;
	}

	bool Contains(FFrameNumber Frame) const
	{
		if (bHasLowerBound && Frame < LowerBound) return false;
		if (bHasUpperBound && Frame >= UpperBound) return false;
		return true;
	}
	bool Contains(const FFrameTime& Time) const { return Contains(Time.FloorToFrame()); }

	FFrameNumber GetLowerBoundValue() const { return LowerBound; }
	FFrameNumber GetUpperBoundValue() const { return UpperBound; }
	int32 Size() const { return UpperBound.Value - LowerBound.Value; }
};
