#include "EnginePCH.h"
#include "GameEngine.h"

#include "Launch/LaunchEngineLoop.h"
#include "Core/Window.h"
#include "Core/Stats/LightweightStats.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Input/InputSystem.h"
#include "Render/RenderCommand.h"
#include "Render/Renderer.h"
#include "Render/SceneRenderer.h"
#include "Render/Swapchain.h"
#include "Render/Texture2D.h"
#include "Audio/AudioDevice.h"
namespace
{
	constexpr const char* GameConfigPath = "Config/DefaultGame.ini";
}

FEngineConfig UGameEngine::GetConfig() const
{
	Settings = FGameSettings::Load(GameConfigPath);

	FEngineConfig Desc;
	Desc.Title = Settings.WindowTitle.c_str();   // GEngine이 살아 있는 동안 유효하다.
	Desc.Width = Settings.Width;
	Desc.Height = Settings.Height;
	Desc.bBorderless = Settings.bBorderless;
	Desc.SyncInterval = Settings.bVSync ? 1 : 0;
	// 3D는 게임 뷰포트의 오프스크린 타깃에 그리고 백버퍼로는 복사만 한다.
	Desc.bCreateDepthBuffer = false;
	// Esc는 FGameViewportClient가 받아 종료 요청으로 바꾼다. 나중에 게임 UI가 가로챌 수 있다.
	Desc.bExitOnEscape = false;
	Desc.SplashImage = "Resources/Splash.png";
	Desc.SkyPanorama = Settings.SkyPanorama.c_str();   // 2-2단계에서 추가한 항목
	return Desc;
}

bool UGameEngine::Init()
{
	if (!Super::Init()) return false;

	UIRenderer = MakeUnique<FUIRenderer>();
	UIRenderer->Init();

	if(!OpenStartupMap(Settings.StartupMap))
	{
		const FString Message = "Cannot open startup map:\n" + Settings.StartupMap +
			"\n\nCheck GameDefaultMap in Config/DefaultGame.ini and the Scenes folder.";
		MessageBoxA(GetEngineLoop().GetMainWindow()->GetHandle(), Message.c_str(), "Hitori", MB_OK | MB_ICONERROR);
		return false;
	}

	return true;
}

void UGameEngine::Tick(float DeltaTime)
{
	FWorldContext* Context = GetWorldContextFromType(EWorldType::Game);
	if (!Context) return;
	TickWorldTravel(*Context);
	if (!Context->World() || !Context->GameViewport)
		return;
	FGameViewportClient& Viewport = *Context->GameViewport;
	UWorld* World = Context->World();

	FEngineLoop& Loop = GetEngineLoop();
	HWND WindowHandle = Loop.GetMainWindow()->GetHandle();

	RECT ClientRect{};
	POINT ClientOrigin{ 0, 0 };

	if (::GetClientRect(WindowHandle, &ClientRect) &&
		::ClientToScreen(WindowHandle, &ClientOrigin))
	{
		Viewport.SetRect(FRect{
			static_cast<float>(ClientOrigin.x),
			static_cast<float>(ClientOrigin.y),
			static_cast<float>(ClientRect.right - ClientRect.left),
			static_cast<float>(ClientRect.bottom - ClientRect.top)
			});
	}

	DispatchInput(Viewport, DeltaTime);
	if (Viewport.ConsumeExitRequest())
	{
		GetEngineLoop().RequestExit();
		return;
	}

	World->Tick(DeltaTime);
	if (FAudioDevice* AudioDevice = GetMainAudioDevice())
	{
		if (APlayerController* PC = World->GetPlayerController())
		{
			FVector Location, Front, Right;
			//PC->GetAudioListenerPosition(Location, Front, Right);
			AudioDevice->SetListener(World, Location, Front, Right);
		}
		AudioDevice->Update(true);
	}
	Viewport.TickUI(DeltaTime);
	RenderFrame(Viewport, World);
	FStatRegistry::EndFrame();
}

bool UGameEngine::OpenStartupMap(const FString& MapPath)
{
	if (FWorldContext* Previous = GetWorldContextFromType(EWorldType::Game))
		DestroyWorldContext(Previous->World());

	FWorldContext& Context = CreateNewWorldContext(EWorldType::Game);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	GameInstance->SetWorldContext(&Context);
	if (!GameInstance->InitializeStandalone(MapPath) || !GameInstance->StartGameInstance())
	{
		DestroyWorldContext(Context.World());
		return false;
	}
	HTR_LOG(Info, "Game map loaded: {}", MapPath);
	return true;
}

void UGameEngine::DispatchInput(FGameViewportClient& Viewport, float DeltaTime)
{
	// 다른 창이 앞에 있으면 눌린 키를 모두 놓은 것으로 처리한다.
	if (::GetForegroundWindow() != GetEngineLoop().GetMainWindow()->GetHandle())
	{
		Viewport.LostFocus();
		return;
	}

	constexpr EKeyCode Keys[] = { EKeyCode::W, EKeyCode::A, EKeyCode::S, EKeyCode::D,
		EKeyCode::Space, EKeyCode::Shift, EKeyCode::Escape };
	for (EKeyCode Key : Keys)
	{
		if (FInputSystem::IsKeyPressed(Key))  Viewport.InputKey(nullptr, static_cast<int32>(Key), true);
		if (FInputSystem::IsKeyReleased(Key)) Viewport.InputKey(nullptr, static_cast<int32>(Key), false);
	}

	Viewport.MouseMove(nullptr, FInputSystem::GetMouseX(), FInputSystem::GetMouseY());

	constexpr EMouseButton MouseButtons[] = { EMouseButton::Left, EMouseButton::Right };
	for (EMouseButton Button : MouseButtons)
	{
		const int32 Key = Button == EMouseButton::Left
			? static_cast<int32>(EKeyCode::LButton) : static_cast<int32>(EKeyCode::RButton);
		if (FInputSystem::IsMousePressed(Button))  Viewport.InputKey(nullptr, Key, true);
		if (FInputSystem::IsMouseReleased(Button)) Viewport.InputKey(nullptr, Key, false);
	}

	Viewport.InputAxis(nullptr, 0, EGameInputAxis::MouseX, static_cast<float>(FInputSystem::GetMouseDeltaX()), DeltaTime);
	Viewport.InputAxis(nullptr, 0, EGameInputAxis::MouseY, static_cast<float>(FInputSystem::GetMouseDeltaY()), DeltaTime);
}

void UGameEngine::RenderFrame(FGameViewportClient& Viewport, UWorld* World)
{
	FEngineLoop& Loop = GetEngineLoop();
	const uint32 Width = Loop.GetViewportWidth();
	const uint32 Height = Loop.GetViewportHeight();
	if (!Width || !Height) return;

	// 크기가 같으면 아무것도 하지 않으므로 매 프레임 불러도 된다. 창 크기 변경도 여기서 따라간다.
	Viewport.Resize(Width, Height);

	const FSceneRenderOptions& Options = Settings.RenderOptions;
	const FRect ViewRect{ 0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height) };
	const FSceneView SceneView = Viewport.CalcSceneView(ViewRect);

	FSceneRenderer SceneRenderer(World, SceneView, Viewport.GetRenderData());
	SceneRenderer.InitViews(Loop.GetRenderer(), Options);
	// 에디터와 같은 FSceneRenderer::Render. 에디터 전용 패스가 없으니 ExtraPasses는 넘기지 않는다.
	if (!SceneRenderer.Render(Loop.GetRenderer(), Viewport.GetViewTargets(), Options, 
		[this, &Viewport, Width, Height](FScenePassCallbacks& Callbacks)
		{
			Callbacks.Overlays.GameUI = [this, &Viewport, Width, Height]
				{
					if (!UIRenderer) return;
					Viewport.PaintUI(UIDrawList);
					UIRenderer->Render(UIDrawList, Width, Height);
				};
		}))
		return;

	// FXAA를 켜면 SceneColor가 FXAA 결과로 바뀌어 있다. PIE 별도 창과 같은 방식으로 복사한다.
	FTexture2D* Backbuffer = Loop.GetSwapchain()->GetBackbuffer();
	FTexture2D* SceneColor = Viewport.GetColorTarget();
	if (!Backbuffer || !SceneColor ||
		Backbuffer->GetWidth() != SceneColor->GetWidth() || Backbuffer->GetHeight() != SceneColor->GetHeight())
		return;
	FRenderDevice::Get()->GetContext()->CopyResource(Backbuffer->GetRawPtr(), SceneColor->GetRawPtr());
}

// UObject 일괄 삭제와 Device 해제 전에 월드와 GPU 자원을 먼저 정리한다.
void UGameEngine::PreExit()
{
	if (FWorldContext* Context = GetWorldContextFromType(EWorldType::Game))
		DestroyWorldContext(Context->World());
	UIRenderer.reset();
}