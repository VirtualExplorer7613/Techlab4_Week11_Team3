#include "EnginePCH.h"
#include "Engine/GameSettings.h"

#include "Core/ConfigFile.h"

namespace
{
	// 창 제목에 한글을 쓸 수 있게 UTF-8을 UTF-16으로 바꾼다.
	std::wstring Utf8ToWide(const FString& Utf8)
	{
		if (Utf8.empty()) return {};
		const int Length = MultiByteToWideChar(CP_UTF8, 0, Utf8.data(), static_cast<int>(Utf8.size()), nullptr, 0);
		std::wstring Wide(Length, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, Utf8.data(), static_cast<int>(Utf8.size()), Wide.data(), Length);
		return Wide;
	}

	// 문자열 값은 editor.ini가 저장하는 표기와 같다.
	ERenderingPath ParseRenderingPath(const FString& Name, ERenderingPath Fallback)
	{
		if (Name == "Forward") return ERenderingPath::Forward;
		if (Name == "Deferred") return ERenderingPath::Deferred;
		return Fallback;
	}

	ELightingModel ParseLightingModel(const FString& Name, ELightingModel Fallback)
	{
		if (Name == "Gouraud") return ELightingModel::Gouraud;
		if (Name == "Lambert") return ELightingModel::Lambert;
		if (Name == "Phong") return ELightingModel::Phong;
		if (Name == "Unlit") return ELightingModel::Unlit;
		return Fallback;
	}

	ELightCullingMode ParseLightCullingMode(const FString& Name, ELightCullingMode Fallback)
	{
		if (Name == "Off") return ELightCullingMode::Off;
		if (Name == "Tiled") return ELightCullingMode::Tiled;
		if (Name == "Clustered") return ELightCullingMode::Clustered;
		return Fallback;
	}
}

FGameSettings FGameSettings::Load(const FString& Path)
{
	FGameSettings Settings;
	FConfigFile Config;
	if (!Config.Load(Path))
	{
		HTR_LOG(Warning, "Game config not found: {}. Using defaults.", Path);
		return Settings;
	}

	Settings.StartupMap = Config.GetString("Game", "GameDefaultMap", Settings.StartupMap);
	Settings.SkyPanorama = Config.GetString("Game", "SkyPanorama", Settings.SkyPanorama);

	// 음수는 잘못된 값으로 보고 기본값을 쓴다. 0은 주 모니터 해상도라는 뜻이라 허용한다.
	const auto GetUInt = [&Config](const char* Section, const char* Key, uint32 Default, bool bAllowZero)
		{
			const int32 Value = Config.GetInt(Section, Key, static_cast<int32>(Default));
			return (Value > 0 || (bAllowZero && Value == 0)) ? static_cast<uint32>(Value) : Default;
		};

	Settings.WindowTitle = Utf8ToWide(Config.GetString("Window", "Title", "Hitori"));
	Settings.Width = GetUInt("Window", "ResolutionSizeX", Settings.Width, true);
	Settings.Height = GetUInt("Window", "ResolutionSizeY", Settings.Height, true);
	Settings.bBorderless = Config.GetBool("Window", "Borderless", Settings.bBorderless);
	Settings.bVSync = Config.GetBool("Window", "VSync", Settings.bVSync);

	FSceneRenderOptions& Options = Settings.RenderOptions;
	Options.RenderingPath = ParseRenderingPath(Config.GetString("Rendering", "RenderingPath", ""), Options.RenderingPath);
	Options.LightingModel = ParseLightingModel(Config.GetString("Rendering", "LightingModel", ""), Options.LightingModel);
	Options.LightCullingMode = ParseLightCullingMode(Config.GetString("Rendering", "LightCullingMode", ""), Options.LightCullingMode);
	Options.bEnableFXAA = Config.GetBool("Rendering", "EnableFXAA", Options.bEnableFXAA);
	Options.bGammaCorrection = Config.GetBool("Rendering", "GammaCorrection", Options.bGammaCorrection);
	Options.bShowVignette = Config.GetBool("Rendering", "ShowVignette", Options.bShowVignette);
	Options.bShowCameraOverlay = Config.GetBool("Rendering", "ShowCameraOverlay", Options.bShowCameraOverlay);
	if (const FString Gamma = Config.GetString("Rendering", "DisplayGamma", ""); !Gamma.empty())
	{
		try { Options.DisplayGamma = FMath::Clamp(std::stof(Gamma), 1.0f, 3.0f); } catch (...) {}
	}

	Options.bCacheShadows = Config.GetBool("Rendering", "CacheShadows", Options.bCacheShadows);

	const int32 PcfMode = Config.GetInt("Rendering", "PcfMode", static_cast<int32>(Options.PcfMode));
	if (PcfMode >= 0 && PcfMode <= static_cast<int32>(EPcfMode::PCSS))
		Options.PcfMode = static_cast<EPcfMode>(PcfMode);

	Options.DirectionalShadowMapSize = GetUInt("Rendering", "DirectionalShadowMapSize", Options.DirectionalShadowMapSize, false);
	Options.PointShadowMapSize = GetUInt("Rendering", "PointShadowMapSize", Options.PointShadowMapSize, false);
	Options.SpotShadowMapSize = GetUInt("Rendering", "SpotShadowMapSize", Options.SpotShadowMapSize, false);
	return Settings;
}
