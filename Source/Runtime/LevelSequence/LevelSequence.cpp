#include "EnginePCH.h"
#include "LevelSequence/LevelSequence.h"

#include "MovieScene/MovieScene.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"
#include <fstream>

namespace
{
	AActor* FindActorByName(UWorld* World, const FString& Name)
	{
		if (!World || Name.empty()) return nullptr;
		auto Search = [&Name](ULevel* Level) -> AActor*
		{
			if (!Level) return nullptr;
			for (AActor* Actor : Level->GetActors())
				if (Actor && Actor->GetName() == Name) return Actor;
			return nullptr;
		};
		if (AActor* Found = Search(World->GetCurrentLevel())) return Found;
		if (AActor* Found = Search(World->GetPersistentLevel())) return Found;
		for (ULevel* Level : World->GetLevels())
			if (AActor* Found = Search(Level)) return Found;
		return nullptr;
	}
}

ULevelSequence::~ULevelSequence()
{
	UMovieScene* ToDelete = MovieScene;
	MovieScene = nullptr;
	delete ToDelete;
}

void ULevelSequence::Initialize()
{
	if (!MovieScene) MovieScene = NewObject<UMovieScene>(this);
}

UObject* ULevelSequence::LocateBoundObject(const FGuid& ObjectId, UWorld* Context) const
{
	const FString* ActorName = FindBoundActorName(ObjectId);
	return ActorName ? FindActorByName(Context, *ActorName) : nullptr;
}

void ULevelSequence::BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UWorld* Context)
{
	UnbindPossessableObjects(ObjectId);
	BindingReferences.Add({ ObjectId, PossessedObject.GetName() });
}

void ULevelSequence::UnbindPossessableObjects(const FGuid& ObjectId)
{
	for (int32 Index = BindingReferences.Num() - 1; Index >= 0; --Index)
		if (BindingReferences[Index].ObjectId == ObjectId) BindingReferences.RemoveAt(Index);
}

const FString* ULevelSequence::FindBoundActorName(const FGuid& ObjectId) const
{
	for (const FLevelSequenceBindingReference& Reference : BindingReferences)
		if (Reference.ObjectId == ObjectId) return &Reference.ActorName;
	return nullptr;
}

bool ULevelSequence::SaveToFile(const FString& Path) const
{
	if (!MovieScene) return false;
	nlohmann::json Root;
	Root["Type"] = "LevelSequence";
	nlohmann::json Scene;
	MovieScene->SaveJson(Scene);
	Root["MovieScene"] = std::move(Scene);

	nlohmann::json References = nlohmann::json::array();
	for (const FLevelSequenceBindingReference& Reference : BindingReferences)
		References.push_back({ { "Guid", Reference.ObjectId.ToString() }, { "Actor", Reference.ActorName } });
	Root["BindingReferences"] = std::move(References);

	std::error_code Error;
	const fs::path FilePath(Path);
	if (FilePath.has_parent_path()) fs::create_directories(FilePath.parent_path(), Error);
	std::ofstream File(FilePath);
	if (!File) return false;
	File << Root.dump(2);
	AssetPath = Path;
	return static_cast<bool>(File);
}

bool ULevelSequence::LoadFromFile(const FString& Path)
{
	std::ifstream File(Path);
	if (!File) return false;
	const nlohmann::json Root = nlohmann::json::parse(File, nullptr, false);
	if (Root.is_discarded() || !Root.is_object() || Root.value("Type", FString()) != "LevelSequence") return false;

	Initialize();
	MovieScene->LoadJson(Root.value("MovieScene", nlohmann::json::object()));
	BindingReferences.Empty();
	for (const nlohmann::json& Item : Root.value("BindingReferences", nlohmann::json::array()))
	{
		FLevelSequenceBindingReference Reference;
		if (!FGuid::Parse(Item.value("Guid", FString()), Reference.ObjectId)) continue;
		Reference.ActorName = Item.value("Actor", FString());
		BindingReferences.Add(Reference);
	}
	AssetPath = Path;
	return true;
}

ULevelSequence* ULevelSequence::LoadLevelSequence(const FString& Path)
{
	ULevelSequence* Sequence = NewObject<ULevelSequence>();
	if (Sequence->LoadFromFile(Path)) return Sequence;
	HTR_LOG(Warning, "Level sequence not loaded: {}", Path);
	delete Sequence;
	return nullptr;
}
