#pragma once

#include <string>

#include "Core/Types.h"
#include "Core/EngineString.h"
#include "Render/SceneRenderOptions.h"

// 패키징된 게임의 실행 설정. Config/DefaultGame.ini에서 읽고, 파일이나 항목이 없으면 기본값을 쓴다.
struct FGameSettings
{
	FString StartupMap = "Scenes/Demo_1_LightTypes.scene";   // [Game] GameDefaultMap
	FString SkyPanorama = "Assets/SkySphere/Sky.hdr";

	std::wstring WindowTitle = L"Hitori";
	uint32 Width = 1600;
	uint32 Height = 900;
	bool bBorderless = false;
	bool bVSync = true;

	FSceneRenderOptions RenderOptions;

	FGameSettings()
	{
		RenderOptions.DirectionalShadowMapSize = 1024;
		RenderOptions.PointShadowMapSize = 512;
		RenderOptions.SpotShadowMapSize = 512;
	}

	static FGameSettings Load(const FString& Path);
};
