#include "EnginePCH.h"
#include "Editor/UIEditor/UIEditorPanel.h"

#include "Editor/Details/PropertyEditor.h"
#include "Render/RenderCommand.h"
#include "UI/UIRenderer.h"
#include "UI/UserWidget.h"
#include "UI/WidgetSerializer.h"
#include "UObject/UObjectHash.h"

#include <fstream>

void FUIEditorPanel::OnRender()
{
	ImGui::Begin(GetPanelName(), nullptr, ImGuiWindowFlags_MenuBar);
	DrawToolbar();

	if (!Root)
	{
		ImGui::TextDisabled("Double-click a .widget file in the Content Drawer to open it here.");
		ImGui::End();
		return;
	}

	HandleShortcuts();

	if (ImGui::BeginTable("UIEditorLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthFixed, 220.0f);
		ImGui::TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthFixed, 300.0f);
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		DrawPalette();
		ImGui::Separator();
		DrawHierarchy(Root);

		ImGui::TableNextColumn();
		DrawCanvas();

		ImGui::TableNextColumn();
		DrawDetails();

		ImGui::EndTable();
	}

	// 트리를 순회하는 도중에는 바꾸지 않고, 그리기가 끝난 뒤 옮긴다.
	if (PendingReparent.Widget)
	{
		PendingReparent.NewParent->AddChild(PendingReparent.Widget);
		PendingReparent = {};
	}

	CommitUndoIfChanged();
	ImGui::End();
}

void FUIEditorPanel::DrawToolbar()
{
	if (!ImGui::BeginMenuBar()) return;

	if (ImGui::MenuItem("Save", "Ctrl+S", false, Root != nullptr)) Save();
	if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !UndoStack.IsEmpty())) Undo();
	if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !RedoStack.IsEmpty())) Redo();

	// 앵커가 맞는지 다른 비율에서 확인한다.
	static const FVector2 Sizes[] = { {1920, 1080}, {1280, 720}, {2560, 1080}, {1080, 1920} };
	const FString SizeLabel = std::format("{}x{}", (int)PreviewSize.X, (int)PreviewSize.Y);
	if (ImGui::BeginMenu(SizeLabel.c_str()))
	{
		for (const FVector2& Size : Sizes)
			if (ImGui::MenuItem(std::format("{}x{}", (int)Size.X, (int)Size.Y).c_str()))
				PreviewSize = Size;
		ImGui::EndMenu();
	}

	// 이 에셋으로 만들 C++ 클래스 (UUserWidget 자손)
	if (ImGui::BeginMenu(("Parent: " + ParentClassName).c_str()))
	{
		TArray<UClass*> Classes;
		GetDerivedClasses(UUserWidget::StaticClass(), Classes);
		Classes.Add(UUserWidget::StaticClass());
		for (UClass* Class : Classes)
			if (ImGui::MenuItem(Class->Name.c_str(), nullptr, Class->Name == ParentClassName))
			{
				ParentClassName = Class->Name;
				bDirty = true;
			}
		ImGui::EndMenu();
	}

	ImGui::TextDisabled("%s%s", AssetPath.c_str(), bDirty ? " *" : "");
	ImGui::EndMenuBar();
}

void FUIEditorPanel::RenderPreview(FUIRenderer& Renderer)
{
	if (!Root || !IsOpen()) return;

	const uint32 Width = static_cast<uint32>(PreviewSize.X);
	const uint32 Height = static_cast<uint32>(PreviewSize.Y);
	if (!PreviewTargets.Resize(Width, Height)) return;

	DrawList.Reset();
	Root->Arrange({ FUIRect{ 0.0f, 0.0f, PreviewSize.X, PreviewSize.Y }, 1.0f });
	Root->Paint(DrawList, 1.0f);

	FTexture2D* Color = PreviewTargets.SceneColor.get();
	RenderCommand::BeginRenderPass(Color, PreviewTargets.Depth.get(), Width, Height);
	const float Clear[4] = { 0.10f, 0.12f, 0.15f, 1.0f };
	RenderCommand::ClearRenderTarget(Color, Clear);
	Renderer.Render(DrawList, Width, Height);
	RenderCommand::EndRenderPass();
}

void FUIEditorPanel::DrawCanvas()
{
	FTexture2D* Preview = PreviewTargets.SceneColor.get();
	if (!Preview || !Preview->GetSRV()) return;

	const ImVec2 Avail = ImGui::GetContentRegionAvail();
	ViewScale = std::min(Avail.x / PreviewSize.X, Avail.y / PreviewSize.Y);
	const ImVec2 Size(PreviewSize.X * ViewScale, PreviewSize.Y * ViewScale);

	CanvasMin = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##UICanvas", Size);
	const bool bHovered = ImGui::IsItemHovered();

	ImDrawList* DL = ImGui::GetWindowDrawList();
	const ImVec2 CanvasMax(CanvasMin.x + Size.x, CanvasMin.y + Size.y);
	DL->AddImage(Preview->GetSRV(), CanvasMin, CanvasMax);
	DL->AddRect(CanvasMin, CanvasMax, IM_COL32(90, 98, 110, 255));

	if (Selected && Selected != Root)
	{
		const ImU32 Accent = IM_COL32(255, 170, 40, 255);
		const FUIRect& R = Selected->GetCachedRect();
		DL->AddRect(ToScreen(R.X, R.Y), ToScreen(R.X + R.W, R.Y + R.H), Accent, 0.0f, 0, 2.0f);

		// 오른쪽 아래 크기 조절 손잡이
		const ImVec2 H = ToScreen(R.X + R.W, R.Y + R.H);
		DL->AddRectFilled(ImVec2(H.x - 5, H.y - 5), ImVec2(H.x + 5, H.y + 5), Accent);

		// 앵커: 부모 영역 안의 점(마름모) 또는 영역(점선 사각형)
		if (const UPanelWidget* Parent = Selected->GetParent())
		{
			const FUIRect& P = Parent->GetCachedRect();
			const FVector4& A = Selected->Anchors;
			const ImVec2 A0 = ToScreen(P.X + P.W * A.X, P.Y + P.H * A.Y);
			const ImVec2 A1 = ToScreen(P.X + P.W * A.Z, P.Y + P.H * A.W);
			if (A.X == A.Z && A.Y == A.W)
				DL->AddQuad(ImVec2(A0.x, A0.y - 7), ImVec2(A0.x + 7, A0.y), ImVec2(A0.x, A0.y + 7), ImVec2(A0.x - 7, A0.y), Accent, 2.0f);
			else
				DL->AddRect(A0, A1, IM_COL32(255, 170, 40, 140));
		}
	}

	HandleCanvasInput(bHovered);
}

UWidget* FUIEditorPanel::PickWidget(UWidget* Widget, float X, float Y) const
{
	if (!Widget || Widget->IsCollapsed() || Widget->Visibility == static_cast<int32>(EUIVisibility::Hidden))
		return nullptr;

	if (const UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		TArray<UWidget*> Sorted;
		Panel->GetSortedChildren(Sorted);
		for (auto It = Sorted.rbegin(); It != Sorted.rend(); ++It)
			if (UWidget* Hit = PickWidget(*It, X, Y))
				return Hit;
	}
	// 루트는 빈 곳 클릭으로 선택되지 않게 한다.
	return (Widget != Root && Widget->GetCachedRect().Contains(X, Y)) ? Widget : nullptr;
}

bool FUIEditorPanel::IsOverResizeHandle(float X, float Y) const
{
	if (!Selected) return false;
	const FUIRect& R = Selected->GetCachedRect();
	const float Grab = 8.0f / ViewScale;   // 화면에서 8픽셀
	return std::abs(X - (R.X + R.W)) < Grab && std::abs(Y - (R.Y + R.H)) < Grab;
}

void FUIEditorPanel::HandleCanvasInput(bool bCanvasHovered)
{
	const ImVec2 Mouse = ImGui::GetMousePos();
	const float X = (Mouse.x - CanvasMin.x) / ViewScale;   // 디자인 좌표
	const float Y = (Mouse.y - CanvasMin.y) / ViewScale;

	if (bCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		DragMode = EDragMode::None;
		if (IsOverResizeHandle(X, Y))
			DragMode = EDragMode::Resize;
		else if ((Selected = PickWidget(Root, X, Y)) != nullptr)
			DragMode = EDragMode::Move;

		if (Selected)
		{
			DragStartX = X;
			DragStartY = Y;
			DragStartOffsets = Selected->Offsets;
			if (DragMode == EDragMode::Resize && Selected->bAutoSize)
			{
				// 자동 크기였다면 지금 보이는 크기에서 시작한다.
				const FUIRect& R = Selected->GetCachedRect();
				DragStartOffsets.Z = R.W;
				DragStartOffsets.W = R.H;
				Selected->bAutoSize = false;
			}
		}
	}

	if (DragMode == EDragMode::None || !Selected) return;
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		DragMode = EDragMode::None;   // 놓는 순간 CommitUndoIfChanged가 스냅샷을 쌓는다
		return;
	}

	float DX = X - DragStartX;
	float DY = Y - DragStartY;
	if (ImGui::GetIO().KeyShift)   // Shift: 8픽셀 격자에 맞춤
	{
		DX = std::round(DX / 8.0f) * 8.0f;
		DY = std::round(DY / 8.0f) * 8.0f;
	}
	ApplyDrag(DX, DY);
}

void FUIEditorPanel::ApplyDrag(float DX, float DY)
{
	FVector4 O = DragStartOffsets;
	const FVector4& A = Selected->Anchors;
	const bool bStretchX = A.X != A.Z;
	const bool bStretchY = A.Y != A.W;

	if (DragMode == EDragMode::Move)
	{
		O.X += DX; if (bStretchX) O.Z -= DX;
		O.Y += DY; if (bStretchY) O.W -= DY;
	}
	else
	{
		// 늘어나는 축은 오른쪽/아래 여백이 줄고, 점 앵커 축은 폭/높이가 는다.
		O.Z += bStretchX ? -DX : DX;
		O.W += bStretchY ? -DY : DY;
	}
	Selected->Offsets = O;
}

// 앵커 프리셋을 바꿔도 화면 위 위치가 그대로 남도록 오프셋을 다시 계산한다.
void FUIEditorPanel::SetAnchorsKeepRect(UWidget* Widget, const FVector4& NewAnchors)
{
	const FUIRect R = Widget->GetCachedRect();
	const FUIRect P = Widget->GetParent() ? Widget->GetParent()->GetCachedRect()
		: FUIRect{ 0.0f, 0.0f, PreviewSize.X, PreviewSize.Y };

	Widget->Anchors = NewAnchors;
	const float A0X = P.X + P.W * NewAnchors.X, A1X = P.X + P.W * NewAnchors.Z;
	const float A0Y = P.Y + P.H * NewAnchors.Y, A1Y = P.Y + P.H * NewAnchors.W;

	if (NewAnchors.X == NewAnchors.Z) { Widget->Offsets.X = R.X - A0X + R.W * Widget->AlignmentX; Widget->Offsets.Z = R.W; }
	else { Widget->Offsets.X = R.X - A0X; Widget->Offsets.Z = A1X - (R.X + R.W); }
	if (NewAnchors.Y == NewAnchors.W) { Widget->Offsets.Y = R.Y - A0Y + R.H * Widget->AlignmentY; Widget->Offsets.W = R.H; }
	else { Widget->Offsets.Y = R.Y - A0Y; Widget->Offsets.W = A1Y - (R.Y + R.H); }
}

void FUIEditorPanel::DrawPalette()
{
	ImGui::TextDisabled("Palette");
	TArray<UClass*> Classes;
	GetDerivedClasses(UWidget::StaticClass(), Classes);

	for (UClass* Class : Classes)
	{
		// 생성할 수 없는 클래스, 베이스 패널, UUserWidget 계열은 뺀다.
		if (!Class->Constructor || Class == UPanelWidget::StaticClass() || Class->IsChildOf(UUserWidget::StaticClass()))
			continue;
		if (!ImGui::Selectable(Class->Name.c_str())) continue;

		UPanelWidget* Target = Cast<UPanelWidget>(Selected);
		if (!Target) Target = (Selected && Selected->GetParent()) ? Selected->GetParent() : Root;

		UWidget* NewWidget = NewObject<UWidget>(nullptr, Class);
		NewWidget->WidgetName = Class->Name.substr(1) + std::to_string(NewWidget->GetUUID());
		NewWidget->Offsets.X = 40.0f;
		NewWidget->Offsets.Y = 40.0f;
		Target->AddChild(NewWidget);
		Selected = NewWidget;
	}
}

namespace
{
	bool IsSelfOrAncestor(const UWidget* Candidate, const UWidget* Widget)
	{
		for (const UWidget* It = Widget; It; It = It->GetParent())
			if (It == Candidate) return true;
		return false;
	}
}

void FUIEditorPanel::DrawHierarchy(UWidget* Widget)
{
	UPanelWidget* Panel = Cast<UPanelWidget>(Widget);

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (!Panel || Panel->GetChildCount() == 0) Flags |= ImGuiTreeNodeFlags_Leaf;
	if (Widget == Selected) Flags |= ImGuiTreeNodeFlags_Selected;

	const FString Name = Widget->WidgetName.empty() ? Widget->GetClass()->Name : Widget->WidgetName;
	const FString Label = Name + "##" + std::to_string(Widget->GetUUID());
	const bool bOpen = ImGui::TreeNodeEx(Label.c_str(), Flags);
	if (ImGui::IsItemClicked()) Selected = Widget;

	// 끌어서 다른 패널 아래로 옮긴다.
	if (Widget != Root && ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("UI_WIDGET", &Widget, sizeof(UWidget*));
		ImGui::TextUnformatted(Name.c_str());
		ImGui::EndDragDropSource();
	}
	if (Panel && ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("UI_WIDGET"))
		{
			UWidget* Dropped = *static_cast<UWidget* const*>(Payload->Data);
			if (!IsSelfOrAncestor(Dropped, Panel))   // 자기 자손 아래로는 못 옮긴다
				PendingReparent = { Dropped, Panel };
		}
		ImGui::EndDragDropTarget();
	}

	if (bOpen)
	{
		if (Panel)
			for (UWidget* Child : Panel->GetChildren())
				DrawHierarchy(Child);
		ImGui::TreePop();
	}
}

void FUIEditorPanel::DrawDetails()
{
	if (!Selected)
	{
		ImGui::TextDisabled("Select a widget in the canvas or hierarchy.");
		return;
	}

	ImGui::TextUnformatted(Selected->GetClass()->Name.c_str());

	struct FAnchorPreset { const char* Label; FVector4 Anchors; };
	static const FAnchorPreset Presets[] = {
		{ "Top Left", {0, 0, 0, 0} }, { "Top Center", {0.5f, 0, 0.5f, 0} }, { "Top Right", {1, 0, 1, 0} },
		{ "Center Left", {0, 0.5f, 0, 0.5f} }, { "Center", {0.5f, 0.5f, 0.5f, 0.5f} }, { "Center Right", {1, 0.5f, 1, 0.5f} },
		{ "Bottom Left", {0, 1, 0, 1} }, { "Bottom Center", {0.5f, 1, 0.5f, 1} }, { "Bottom Right", {1, 1, 1, 1} },
		{ "Top Stretch", {0, 0, 1, 0} }, { "Bottom Stretch", {0, 1, 1, 1} }, { "Fill", {0, 0, 1, 1} },
	};
	if (Selected != Root && ImGui::BeginCombo("Anchors", "Select Preset"))
	{
		for (const FAnchorPreset& Preset : Presets)
			if (ImGui::Selectable(Preset.Label))
				SetAnchorsKeepRect(Selected, Preset.Anchors);
		ImGui::EndCombo();
	}

	FPropertyEditor::DrawObjectProperties(Selected);
}

void FUIEditorPanel::DeleteSelected()
{
	if (!Selected || Selected == Root) return;
	UWidget* Target = Selected;
	Selected = Target->GetParent();
	Target->RemoveFromParent();
	delete Target;   // 패널이면 자식도 함께 지워진다
}

FString FUIEditorPanel::Snapshot() const
{
	return FWidgetSerializer::SaveWidget(Root).dump();
}

void FUIEditorPanel::RestoreSnapshot(const FString& Data)
{
	const FString SelectedName = Selected ? Selected->WidgetName : FString();
	delete Root;
	Root = Cast<UPanelWidget>(FWidgetSerializer::LoadWidget(json::parse(Data)));
	Selected = Root ? (Root->WidgetName == SelectedName ? Root : Root->FindWidget(SelectedName)) : nullptr;
	DragMode = EDragMode::None;
}

void FUIEditorPanel::CommitUndoIfChanged()
{
	if (!Root || ImGui::IsAnyItemActive() || DragMode != EDragMode::None) return;   // 편집이 끝날 때까지 기다린다

	FString Now = Snapshot();
	if (Now == LastSnapshot) return;
	UndoStack.Add(LastSnapshot);
	RedoStack.Reset();
	LastSnapshot = std::move(Now);
	bDirty = true;
}

void FUIEditorPanel::Undo()
{
	if (UndoStack.IsEmpty()) return;
	RedoStack.Add(LastSnapshot);
	LastSnapshot = UndoStack.Last();
	UndoStack.RemoveLast();
	RestoreSnapshot(LastSnapshot);
	bDirty = true;
}

void FUIEditorPanel::Redo()
{
	if (RedoStack.IsEmpty()) return;
	UndoStack.Add(LastSnapshot);
	LastSnapshot = RedoStack.Last();
	RedoStack.RemoveLast();
	RestoreSnapshot(LastSnapshot);
	bDirty = true;
}

void FUIEditorPanel::HandleShortcuts()
{
	if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || ImGui::IsAnyItemActive()) return;
	const ImGuiIO& IO = ImGui::GetIO();
	if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) Save();
	if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) Undo();
	if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) Redo();
	if (ImGui::IsKeyPressed(ImGuiKey_Delete)) DeleteSelected();
}

void FUIEditorPanel::NewAsset(const FString& Path)
{
	UCanvasPanel* NewRoot = NewObject<UCanvasPanel>();
	NewRoot->WidgetName = "Root";
	NewRoot->Anchors = FVector4(0.0f, 0.0f, 1.0f, 1.0f);   // 화면 전체
	NewRoot->Offsets = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
	NewRoot->Visibility = 0;   // HitTestInvisible이면 자식 버튼이 클릭을 못 받는다

	delete Root;
	Root = NewRoot;
	Selected = nullptr;
	AssetPath = Path;
	ParentClassName = "UUserWidget";
	PreviewSize = FVector2(1920.0f, 1080.0f);

	UndoStack.Reset();
	RedoStack.Reset();
	LastSnapshot = Snapshot();

	Save();          // 디스크에 바로 만든다 (콘텐츠 드로어에 곧장 보이도록)
	SetOpen(true);
}

void FUIEditorPanel::OpenAsset(const FString& Path)
{
	std::ifstream In(Path);
	json File = In ? json::parse(In, nullptr, false) : json();
	if (File.is_discarded() || !File.contains("Root"))
	{
		HTR_LOG(Error, "Cannot open widget asset: {}", Path);
		return;
	}

	delete Root;
	Root = Cast<UPanelWidget>(FWidgetSerializer::LoadWidget(File["Root"]));
	Selected = nullptr;
	AssetPath = Path;
	ParentClassName = File.value("ParentClass", std::string("UUserWidget"));
	if (File.contains("DesignSize"))
		PreviewSize = FVector2(File["DesignSize"][0].get<float>(), File["DesignSize"][1].get<float>());

	UndoStack.Reset();
	RedoStack.Reset();
	LastSnapshot = Snapshot();
	bDirty = false;
	SetOpen(true);
}

void FUIEditorPanel::Save()
{
	if (!Root || AssetPath.empty()) return;

	json File;
	File["Version"] = 1;
	File["ParentClass"] = ParentClassName;
	File["DesignSize"] = { 1920, 1080 };   // 레이아웃 기준은 항상 1080 높이
	File["Root"] = FWidgetSerializer::SaveWidget(Root);

	std::ofstream Out(AssetPath);
	Out << File.dump(2);

	FWidgetAssetCache::Invalidate(AssetPath);   // 다음 CreateWidget이 새 파일을 읽는다
	bDirty = false;
}