#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Render/SceneViewTargets.h"
#include "UI/UIDrawList.h"

class UWidget;
class UPanelWidget;
class FUIRenderer;

class FUIEditorPanel : public IEditorPanel
{
public:
	virtual bool Init() override { return true; }
	virtual void Tick(float DeltaTime) override {}
	virtual void OnRender() override;
	virtual const char* GetPanelName() const override { return "UI Editor"; }

	void NewAsset(const FString& Path);
	void OpenAsset(const FString& Path);
	void Save();

	// ImGui 프레임을 만들기 전에 엔진이 부른다. 프리뷰 텍스처에 위젯을 그린다.
	void RenderPreview(FUIRenderer& Renderer);

private:
	enum class EDragMode { None, Move, Resize };

	void DrawToolbar();
	void DrawPalette();
	void DrawHierarchy(UWidget* Widget);
	void DrawCanvas();
	void DrawDetails();
	void HandleCanvasInput(bool bCanvasHovered);
	void HandleShortcuts();

	UWidget* PickWidget(UWidget* Widget, float X, float Y) const;
	bool IsOverResizeHandle(float X, float Y) const;
	void ApplyDrag(float DX, float DY);
	void SetAnchorsKeepRect(UWidget* Widget, const FVector4& NewAnchors);
	void DeleteSelected();

	FString Snapshot() const;
	void RestoreSnapshot(const FString& Data);
	void CommitUndoIfChanged();
	void Undo();
	void Redo();


	ImVec2 ToScreen(float X, float Y) const { return ImVec2(CanvasMin.x + X * ViewScale, CanvasMin.y + Y * ViewScale); }

	FString AssetPath;
	FString ParentClassName = "UUserWidget";
	UPanelWidget* Root = nullptr;
	UWidget* Selected = nullptr;

	FVector2 PreviewSize = FVector2(1920.0f, 1080.0f);
	FSceneViewTargets PreviewTargets;    // SceneColor를 프리뷰 텍스처로 쓴다
	FUIDrawList DrawList;
	ImVec2 CanvasMin;
	float ViewScale = 1.0f;

	EDragMode DragMode = EDragMode::None;
	float DragStartX = 0.0f, DragStartY = 0.0f;
	FVector4 DragStartOffsets;

	struct FReparent { UWidget* Widget = nullptr; UPanelWidget* NewParent = nullptr; } PendingReparent;

	TArray<FString> UndoStack;
	TArray<FString> RedoStack;
	FString LastSnapshot;
	bool bDirty = false;
};