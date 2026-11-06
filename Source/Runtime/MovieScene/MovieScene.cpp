#include "EnginePCH.h"
#include "MovieScene/MovieScene.h"
#include "MovieScene/MovieSceneSequence.h"

#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	UMovieSceneTrack* CreateTrack(UMovieScene* Outer, UClass* TrackClass)
	{
		if (!TrackClass || !TrackClass->IsChildOf(UMovieSceneTrack::StaticClass()) || !TrackClass->Constructor) return nullptr;
		UMovieSceneTrack* Track = NewObject<UMovieSceneTrack>(Outer, TrackClass);
		// 시퀀서와 같이 새 트랙에는 섹션 하나를 바로 만든다 (CameraCut은 컷을 추가할 때)
		if (Track && !Track->SupportsMultipleSections())
			if (UMovieSceneSection* Section = Track->CreateNewSection()) Track->AddSection(Section);
		return Track;
	}
}

UMovieScene::UMovieScene()
{
	// 기본 5초
	SetPlaybackRange(FFrameNumber(0), TickResolution.AsFrameNumber(5.0).Value);
}

UMovieScene::~UMovieScene()
{
	Reset();
	if (UMovieSceneSequence* Sequence = Cast<UMovieSceneSequence>(GetOuter())) Sequence->OnMovieSceneDestroyed(this);
}

// 트랙 소멸자가 목록을 고치므로 먼저 비운 뒤 지운다
void UMovieScene::Reset()
{
	TArray<UMovieSceneTrack*> ToDelete = Tracks;
	for (const FMovieSceneBinding& Binding : ObjectBindings)
		for (UMovieSceneTrack* Track : Binding.Tracks) ToDelete.Add(Track);
	if (CameraCutTrack) ToDelete.Add(CameraCutTrack);
	ObjectBindings.Empty();
	Possessables.Empty();
	Tracks.Empty();
	CameraCutTrack = nullptr;
	for (UMovieSceneTrack* Track : ToDelete) delete Track;
}

void UMovieScene::OnTrackDestroyed(UMovieSceneTrack* Track)
{
	if (Track == CameraCutTrack) { CameraCutTrack = nullptr; return; }
	if (const int32 Index = Tracks.Find(Track); Index >= 0) { Tracks.RemoveAt(Index); return; }
	for (FMovieSceneBinding& Binding : ObjectBindings)
		if (const int32 Index = Binding.Tracks.Find(Track); Index >= 0) { Binding.Tracks.RemoveAt(Index); return; }
}

FGuid UMovieScene::AddPossessable(const FString& Name, UClass* Class)
{
	FMovieScenePossessable Possessable;
	Possessable.Guid = FGuid::NewGuid();
	Possessable.Name = Name;
	Possessable.PossessedObjectClass = Class ? Class->Name : FString();
	Possessables.Add(Possessable);

	FMovieSceneBinding Binding;
	Binding.ObjectGuid = Possessable.Guid;
	ObjectBindings.Add(Binding);
	return Possessable.Guid;
}

bool UMovieScene::RemovePossessable(const FGuid& PossessableGuid)
{
	for (int32 Index = 0; Index < ObjectBindings.Num(); ++Index)
	{
		if (ObjectBindings[Index].ObjectGuid != PossessableGuid) continue;
		TArray<UMovieSceneTrack*> ToDelete = ObjectBindings[Index].Tracks;
		ObjectBindings.RemoveAt(Index);
		for (UMovieSceneTrack* Track : ToDelete) delete Track;
		break;
	}
	for (int32 Index = 0; Index < Possessables.Num(); ++Index)
	{
		if (Possessables[Index].Guid != PossessableGuid) continue;
		Possessables.RemoveAt(Index);
		return true;
	}
	return false;
}

FMovieScenePossessable* UMovieScene::FindPossessable(const FGuid& Guid)
{
	for (FMovieScenePossessable& Possessable : Possessables)
		if (Possessable.Guid == Guid) return &Possessable;
	return nullptr;
}

FMovieSceneBinding* UMovieScene::FindBinding(const FGuid& ForGuid)
{
	for (FMovieSceneBinding& Binding : ObjectBindings)
		if (Binding.ObjectGuid == ForGuid) return &Binding;
	return nullptr;
}

UMovieSceneTrack* UMovieScene::AddTrack(UClass* TrackClass, const FGuid& ObjectGuid)
{
	FMovieSceneBinding* Binding = FindBinding(ObjectGuid);
	if (!Binding) return nullptr;
	UMovieSceneTrack* Track = CreateTrack(this, TrackClass);
	if (Track) Binding->Tracks.Add(Track);
	return Track;
}

UMovieSceneTrack* UMovieScene::AddTrack(UClass* TrackClass)
{
	UMovieSceneTrack* Track = CreateTrack(this, TrackClass);
	if (Track) Tracks.Add(Track);
	return Track;
}

bool UMovieScene::RemoveTrack(UMovieSceneTrack* Track)
{
	if (!Track) return false;
	if (Track == CameraCutTrack)
	{
		RemoveCameraCutTrack();
		return true;
	}
	if (const int32 Index = Tracks.Find(Track); Index >= 0)
	{
		Tracks.RemoveAt(Index);
		delete Track;
		return true;
	}
	for (FMovieSceneBinding& Binding : ObjectBindings)
	{
		if (const int32 Index = Binding.Tracks.Find(Track); Index >= 0)
		{
			Binding.Tracks.RemoveAt(Index);
			delete Track;
			return true;
		}
	}
	return false;
}

UMovieSceneTrack* UMovieScene::FindTrack(UClass* TrackClass) const
{
	for (UMovieSceneTrack* Track : Tracks)
		if (Track && Track->GetClass() == TrackClass) return Track;
	return nullptr;
}

UMovieSceneTrack* UMovieScene::AddCameraCutTrack(UClass* TrackClass)
{
	if (!CameraCutTrack) CameraCutTrack = CreateTrack(this, TrackClass);
	return CameraCutTrack;
}

void UMovieScene::RemoveCameraCutTrack()
{
	UMovieSceneTrack* Track = CameraCutTrack;
	CameraCutTrack = nullptr;
	delete Track;
}

void UMovieScene::SetPlaybackRange(FFrameNumber Start, int32 Duration)
{
	PlaybackRange = FMovieSceneFrameRange::Make(Start, Start + FFrameNumber(std::max(Duration, 1)));
}

void UMovieScene::SaveJson(nlohmann::json& Out) const
{
	Out["TickResolution"] = { TickResolution.Numerator, TickResolution.Denominator };
	Out["DisplayRate"] = { DisplayRate.Numerator, DisplayRate.Denominator };
	Out["PlaybackStart"] = PlaybackRange.LowerBound.Value;
	Out["PlaybackEnd"] = PlaybackRange.UpperBound.Value;

	auto SaveTracks = [](const TArray<UMovieSceneTrack*>& InTracks)
	{
		nlohmann::json Items = nlohmann::json::array();
		for (const UMovieSceneTrack* Track : InTracks)
		{
			nlohmann::json Item;
			Track->SaveJson(Item);
			Items.push_back(std::move(Item));
		}
		return Items;
	};

	nlohmann::json Bindings = nlohmann::json::array();
	for (const FMovieSceneBinding& Binding : ObjectBindings)
	{
		const FMovieScenePossessable* Possessable = const_cast<UMovieScene*>(this)->FindPossessable(Binding.ObjectGuid);
		Bindings.push_back({
			{ "Guid", Binding.ObjectGuid.ToString() },
			{ "Name", Possessable ? Possessable->Name : FString() },
			{ "Class", Possessable ? Possessable->PossessedObjectClass : FString() },
			{ "Tracks", SaveTracks(Binding.Tracks) } });
	}
	Out["Bindings"] = std::move(Bindings);
	Out["Tracks"] = SaveTracks(Tracks);
	if (CameraCutTrack)
	{
		nlohmann::json Item;
		CameraCutTrack->SaveJson(Item);
		Out["CameraCutTrack"] = std::move(Item);
	}
}

void UMovieScene::LoadJson(const nlohmann::json& In)
{
	Reset();
	if (const auto It = In.find("DisplayRate"); It != In.end() && It->is_array() && It->size() == 2)
		DisplayRate = FFrameRate((*It)[0].get<int32>(), (*It)[1].get<int32>());
	// TickResolution은 24000 고정. 다른 값으로 저장된 파일은 키 시간이 어긋나므로 경고만 남긴다
	if (const auto It = In.find("TickResolution"); It != In.end() && It->is_array() && It->size() == 2)
		if ((*It)[0].get<int32>() != TickResolution.Numerator)
			HTR_LOG(Warning, "Sequence tick resolution {} differs from 24000", (*It)[0].get<int32>());

	const int32 Start = In.value("PlaybackStart", 0);
	const int32 End = In.value("PlaybackEnd", TickResolution.AsFrameNumber(5.0).Value);
	SetPlaybackRange(FFrameNumber(Start), End - Start);

	auto LoadTracks = [this](const nlohmann::json& Items, TArray<UMovieSceneTrack*>& OutTracks)
	{
		for (const nlohmann::json& Item : Items)
		{
			UMovieSceneTrack* Track = NewMovieSceneTrack(this, Item.value("Class", FString()));
			if (!Track) continue;
			Track->LoadJson(Item);
			OutTracks.Add(Track);
		}
	};

	for (const nlohmann::json& Item : In.value("Bindings", nlohmann::json::array()))
	{
		FMovieScenePossessable Possessable;
		if (!FGuid::Parse(Item.value("Guid", FString()), Possessable.Guid)) continue;
		Possessable.Name = Item.value("Name", FString());
		Possessable.PossessedObjectClass = Item.value("Class", FString());
		Possessables.Add(Possessable);

		FMovieSceneBinding Binding;
		Binding.ObjectGuid = Possessable.Guid;
		LoadTracks(Item.value("Tracks", nlohmann::json::array()), Binding.Tracks);
		ObjectBindings.Add(Binding);
	}
	LoadTracks(In.value("Tracks", nlohmann::json::array()), Tracks);

	if (const auto It = In.find("CameraCutTrack"); It != In.end())
	{
		CameraCutTrack = NewMovieSceneTrack(this, It->value("Class", FString()));
		if (CameraCutTrack) CameraCutTrack->LoadJson(*It);
	}
}
