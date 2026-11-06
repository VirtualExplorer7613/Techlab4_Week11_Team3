#include "EnginePCH.h"
#include "MovieSceneTracks/MovieSceneEventTrack.h"

#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"
#include <algorithm>

int32 FMovieSceneEventChannel::AddKey(FFrameNumber Time, const FMovieSceneEvent& Event)
{
	const auto Upper = std::upper_bound(Times.begin(), Times.end(), Time);
	const int32 Index = static_cast<int32>(Upper - Times.begin());
	Times.Insert(Time, Index);
	Events.Insert(Event, Index);
	return Index;
}

void FMovieSceneEventChannel::RemoveKey(int32 Index)
{
	if (Index < 0 || Index >= Times.Num()) return;
	Times.RemoveAt(Index);
	Events.RemoveAt(Index);
}

int32 FMovieSceneEventChannel::SetKeyTime(int32 Index, FFrameNumber NewTime)
{
	if (Index < 0 || Index >= Times.Num()) return -1;
	const FMovieSceneEvent Event = Events[Index];
	RemoveKey(Index);
	return AddKey(NewTime, Event);
}

void UMovieSceneEventTriggerSection::Evaluate(const FMovieSceneContext& Context, IMovieScenePlayer& Player, UObject* BoundObject)
{
	// 재생 중 앞으로 지나간 키만: (Previous, Current]. 스크럽·점프로는 쏘지 않는다
	if (Context.GetStatus() != EMovieScenePlayerStatus::Playing || Context.HasJumped()) return;
	const UMovieSceneEventTrack* Track = Cast<UMovieSceneEventTrack>(GetOuter());
	if (Track && !Track->bFireEventsWhenForwards) return;

	UWorld* World = Player.GetPlaybackContext();
	const double Previous = Context.GetPreviousTime().AsDecimal();
	const double Current = Context.GetTime().AsDecimal();
	for (int32 Index = 0; Index < EventChannel.Times.Num(); ++Index)
	{
		const double KeyTime = EventChannel.Times[Index].Value;
		if (KeyTime <= Previous || KeyTime > Current) continue;

		const FMovieSceneEvent& Event = EventChannel.Events[Index];
		switch (Event.Kind)
		{
		case EMovieSceneEventKind::HitStop:
			if (World) World->StartHitStop(Event.Duration, Event.Dilation);
			break;
		case EMovieSceneEventKind::Log:
			HTR_LOG(Info, "Sequence event: {}", Event.Message);
			break;
		}
	}
}

void UMovieSceneEventTriggerSection::SaveJson(nlohmann::json& Out) const
{
	Super::SaveJson(Out);
	nlohmann::json Items = nlohmann::json::array();
	for (int32 Index = 0; Index < EventChannel.Times.Num(); ++Index)
	{
		const FMovieSceneEvent& Event = EventChannel.Events[Index];
		Items.push_back({
			{ "Time", EventChannel.Times[Index].Value },
			{ "Kind", static_cast<int32>(Event.Kind) },
			{ "Duration", Event.Duration },
			{ "Dilation", Event.Dilation },
			{ "Message", Event.Message } });
	}
	Out["Events"] = std::move(Items);
}

void UMovieSceneEventTriggerSection::LoadJson(const nlohmann::json& In)
{
	Super::LoadJson(In);
	EventChannel = FMovieSceneEventChannel();
	for (const nlohmann::json& Item : In.value("Events", nlohmann::json::array()))
	{
		FMovieSceneEvent Event;
		Event.Kind = static_cast<EMovieSceneEventKind>(Item.value("Kind", 0));
		Event.Duration = Item.value("Duration", 0.1f);
		Event.Dilation = Item.value("Dilation", 0.05f);
		Event.Message = Item.value("Message", FString());
		EventChannel.AddKey(FFrameNumber(Item.value("Time", 0)), Event);
	}
}

UMovieSceneSection* UMovieSceneEventTrack::CreateNewSection()
{
	return NewObject<UMovieSceneEventTriggerSection>(this);
}

void UMovieSceneEventTrack::SaveJson(nlohmann::json& Out) const
{
	Super::SaveJson(Out);
	Out["FireForwards"] = bFireEventsWhenForwards;
	Out["FireBackwards"] = bFireEventsWhenBackwards;
}

void UMovieSceneEventTrack::LoadJson(const nlohmann::json& In)
{
	Super::LoadJson(In);
	bFireEventsWhenForwards = In.value("FireForwards", true);
	bFireEventsWhenBackwards = In.value("FireBackwards", true);
}
