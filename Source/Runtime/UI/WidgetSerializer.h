#pragma once

#include "UI/UserWidget.h"

namespace FWidgetSerializer
{
	// 위젯 하나와 그 자손을 { Class, Properties, Children } 노드로 만든다.
	json SaveWidget(const UWidget* Widget);
	// 노드에서 위젯 트리를 새로 만든다. 클래스를 못 찾으면 nullptr.
	UWidget* LoadWidget(const json& Node);
}

// .widget 파일을 경로당 한 번만 읽어 둔다. 여기 든 JSON이 "에셋(템플릿)"이다.
class FWidgetAssetCache
{
public:
	static const json* Find(const FString& Path);
	static void Invalidate(const FString& Path);   // 에디터가 저장한 뒤 부른다

private:
	static TMap<FString, json>& GetMap();
};
