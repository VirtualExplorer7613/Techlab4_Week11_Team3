#pragma once

#include "Engine.h"
#include "Engine/GameSettings.h"

#include "UI/UIRenderer.h"
#include "UI/UIDrawList.h"
#include "GameEngine.generated.h"

class FGameViewportClient;

UCLASS()
class UGameEngine : public UEngine
{
	GENERATED_BODY()
public:
	FEngineConfig GetConfig() const override;

	bool Init() override;
	void Tick(float DeltaTime) override;
	void PreExit() override;

	// 첫 맵: 컨텍스트와 게임 인스턴스를 만들고 씬 파일로 Game 월드를 연다.
	// 이후 전환은 UGameplayStatics::OpenLevel → UEngine::LoadMap으로 간다.
	bool OpenStartupMap(const FString& MapPath);
private:
	void DispatchInput(FGameViewportClient& Viewport, float DeltaTime);
	void RenderFrame(FGameViewportClient& Viewport, UWorld* World);

	// GetConfig는 창을 만들기 전에 const로 불리므로 거기서 읽어 둔다.
	mutable FGameSettings Settings;

	TUniquePtr<FUIRenderer> UIRenderer;
	FUIDrawList UIDrawList;
};