#include "EnginePCH.h"
#include "MovieScene/MovieSceneSection.h"
#include "MovieScene/MovieSceneTrack.h"

// UActorComponent처럼 지워질 때 트랙 목록에서 빠진다.
// 엔진 종료 때 DestroyAllObjectsExceptEngine이 UObject를 하나씩 지우므로 트랙이 지운 포인터를 다시 지우지 않게 한다
UMovieSceneSection::~UMovieSceneSection()
{
	if (UMovieSceneTrack* Track = Cast<UMovieSceneTrack>(GetOuter())) Track->OnSectionDestroyed(this);
}

bool UMovieSceneSection::ShouldRestoreState(const IMovieScenePlayer& Player) const
{
	switch (EvalOptions.CompletionMode)
	{
	case EMovieSceneCompletionMode::KeepState:    return false;
	case EMovieSceneCompletionMode::RestoreState: return true;
	default:                                      return Player.ShouldRestoreState();
	}
}

void UMovieSceneSection::SaveJson(nlohmann::json& Out) const
{
	Out["Class"] = GetClass()->Name;
	Out["Active"] = bIsActive;
	Out["Completion"] = static_cast<int32>(EvalOptions.CompletionMode);
	if (SectionRange.bHasLowerBound) Out["Start"] = SectionRange.LowerBound.Value;
	if (SectionRange.bHasUpperBound) Out["End"] = SectionRange.UpperBound.Value;

	nlohmann::json Channels = nlohmann::json::array();
	for (int32 Index = 0; Index < GetNumChannels(); ++Index)
	{
		nlohmann::json Channel;
		const_cast<UMovieSceneSection*>(this)->GetChannel(Index)->SaveJson(Channel);
		Channels.push_back(std::move(Channel));
	}
	Out["Channels"] = std::move(Channels);
}

void UMovieSceneSection::LoadJson(const nlohmann::json& In)
{
	bIsActive = In.value("Active", true);
	EvalOptions.CompletionMode = static_cast<EMovieSceneCompletionMode>(In.value("Completion", static_cast<int32>(EMovieSceneCompletionMode::ProjectDefault)));
	SectionRange = FMovieSceneFrameRange::All();
	if (In.contains("Start")) SetStartFrame(FFrameNumber(In.value("Start", 0)));
	if (In.contains("End")) SetEndFrame(FFrameNumber(In.value("End", 0)));

	const nlohmann::json Channels = In.value("Channels", nlohmann::json::array());
	for (int32 Index = 0; Index < GetNumChannels() && Index < static_cast<int32>(Channels.size()); ++Index)
		GetChannel(Index)->LoadJson(Channels[Index]);
}
