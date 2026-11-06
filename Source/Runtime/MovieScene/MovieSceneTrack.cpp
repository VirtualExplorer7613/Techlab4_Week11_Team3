#include "EnginePCH.h"
#include "MovieScene/MovieSceneTrack.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "MovieScene/MovieScene.h"

namespace
{
	// 섹션 소멸자가 목록을 고치므로 먼저 비운 뒤 지운다 (AActor 소멸자와 같은 방식)
	void DeleteSections(TArray<UMovieSceneSection*>& Sections)
	{
		TArray<UMovieSceneSection*> ToDelete = Sections;
		Sections.Empty();
		for (UMovieSceneSection* Section : ToDelete)
			delete Section;
	}
}

UMovieSceneTrack::~UMovieSceneTrack()
{
	DeleteSections(Sections);
	if (UMovieScene* MovieScene = Cast<UMovieScene>(GetOuter())) MovieScene->OnTrackDestroyed(this);
}

void UMovieSceneTrack::AddSection(UMovieSceneSection* Section)
{
	if (!Section || Sections.Find(Section) >= 0) return;
	Sections.Add(Section);
}

void UMovieSceneTrack::RemoveSection(UMovieSceneSection* Section)
{
	const int32 Index = Sections.Find(Section);
	if (Index < 0) return;
	Sections.RemoveAt(Index);
	delete Section;
}

void UMovieSceneTrack::OnSectionDestroyed(UMovieSceneSection* Section)
{
	const int32 Index = Sections.Find(Section);
	if (Index >= 0) Sections.RemoveAt(Index);
}

void UMovieSceneTrack::Evaluate(const FMovieSceneContext& Context, IMovieScenePlayer& Player, UObject* BoundObject)
{
	const FFrameNumber Frame = Context.GetTime().FloorToFrame();
	for (UMovieSceneSection* Section : Sections)
	{
		if (Section && Section->IsActive() && Section->GetRange().Contains(Frame))
			Section->Evaluate(Context, Player, BoundObject);
	}
}

void UMovieSceneTrack::SaveJson(nlohmann::json& Out) const
{
	Out["Class"] = GetClass()->Name;
	Out["EvalDisabled"] = bIsEvalDisabled;
	nlohmann::json Items = nlohmann::json::array();
	for (const UMovieSceneSection* Section : Sections)
	{
		nlohmann::json Item;
		Section->SaveJson(Item);
		Items.push_back(std::move(Item));
	}
	Out["Sections"] = std::move(Items);
}

void UMovieSceneTrack::LoadJson(const nlohmann::json& In)
{
	bIsEvalDisabled = In.value("EvalDisabled", false);
	DeleteSections(Sections);
	for (const nlohmann::json& Item : In.value("Sections", nlohmann::json::array()))
	{
		UMovieSceneSection* Section = CreateNewSection();
		if (!Section) continue;
		Section->LoadJson(Item);
		Sections.Add(Section);
	}
}

UMovieSceneTrack* NewMovieSceneTrack(UObject* Outer, const FString& ClassName)
{
	TArray<UClass*> Classes;
	GetDerivedClasses(UMovieSceneTrack::StaticClass(), Classes);
	for (UClass* Class : Classes)
	{
		if (Class && Class->Name == ClassName && Class->Constructor)
			return static_cast<UMovieSceneTrack*>(NewObject<UMovieSceneTrack>(Outer, Class));
	}
	return nullptr;
}
