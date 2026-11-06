#include "EnginePCH.h"
#include "UI/WidgetSerializer.h"

#include "UObject/UObjectHash.h"

#include <fstream>

json FWidgetSerializer::SaveWidget(const UWidget* Widget)
{
	json Node;
	if (!Widget) return Node;

	Node["Class"] = Widget->GetClass()->Name;

	json Properties = json::object();
	const_cast<UWidget*>(Widget)->Serialize(Properties, false);   // 리플렉션 속성 전부
	Node["Properties"] = std::move(Properties);

	if (const UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		json Children = json::array();
		for (const UWidget* Child : Panel->GetChildren())
			Children.push_back(SaveWidget(Child));
		Node["Children"] = std::move(Children);
	}
	return Node;
}

UWidget* FWidgetSerializer::LoadWidget(const json& Node)
{
	if (!Node.is_object() || !Node.contains("Class")) return nullptr;

	UClass* Class = FindClass(Node["Class"].get<std::string>());
	if (!Class || !Class->IsChildOf(UWidget::StaticClass()) || !Class->Constructor)
	{
		HTR_LOG(Warning, "Unknown widget class: {}", Node["Class"].dump());
		return nullptr;
	}

	UWidget* Widget = NewObject<UWidget>(nullptr, Class);
	if (Node.contains("Properties"))
	{
		json Properties = Node["Properties"];        // Serialize가 non-const json&를 받는다
		Widget->Serialize(Properties, true);
	}
	Widget->OnLoaded();

	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget); Panel && Node.contains("Children"))
	{
		for (const json& ChildNode : Node["Children"])
			if (UWidget* Child = LoadWidget(ChildNode))
				Panel->AddChild(Child);
	}
	return Widget;
}

TMap<FString, json>& FWidgetAssetCache::GetMap()
{
	static TMap<FString, json> Map;
	return Map;
}

const json* FWidgetAssetCache::Find(const FString& Path)
{
	if (const json* Cached = GetMap().Find(Path))
		return Cached;

	std::ifstream In(Path);
	if (!In)
	{
		HTR_LOG(Error, "Cannot open widget asset: {}", Path);
		return nullptr;
	}
	json Data = json::parse(In, nullptr, false);
	if (Data.is_discarded() || !Data.contains("Root"))
	{
		HTR_LOG(Error, "Invalid widget asset: {}", Path);
		return nullptr;
	}
	GetMap().Add(Path, std::move(Data));
	return GetMap().Find(Path);
}

void FWidgetAssetCache::Invalidate(const FString& Path)
{
	GetMap().Remove(Path);
}

UUserWidget* CreateWidget(UWorld* World, const FString& AssetPath)
{
	const json* Data = FWidgetAssetCache::Find(AssetPath);
	if (!Data) return nullptr;

	UClass* Class = FindClass(Data->value("ParentClass", std::string("UUserWidget")));
	if (!Class || !Class->IsChildOf(UUserWidget::StaticClass()))
		Class = UUserWidget::StaticClass();

	UUserWidget* Widget = NewObject<UUserWidget>(nullptr, Class);
	Widget->SetOwningWorld(World);
	Widget->AssetPath = AssetPath;

	UPanelWidget* Root = Cast<UPanelWidget>(FWidgetSerializer::LoadWidget((*Data)["Root"]));
	if (!Root)
		HTR_LOG(Warning, "Widget asset root must be a panel: {}", AssetPath);
	Widget->SetRootWidget(Root);
	return Widget;
}