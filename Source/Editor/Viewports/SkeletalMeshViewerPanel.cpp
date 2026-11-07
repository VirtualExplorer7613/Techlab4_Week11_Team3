#include "EnginePCH.h"
#include "Editor/Viewports/SkeletalMeshViewerPanel.h"
#include "Engine/World.h"
#include "GameFramework/Actor/SkeletalMeshActor.h"
#include "GameFramework/Actor/DirectionalLightActor.h"
#include "Components/PrimitiveComponent.h"
#include "Render/SkeletalMesh.h"
#include "Render/LineBatcher.h"
#include "Asset/AssetManager.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/AnimDataModel.h"

#include <algorithm>
#include <cmath>

namespace
{
	// 에디터 기본 들여쓰기(18px)는 깊은 계층에서 이름을 밀어내므로 좁힌다
	constexpr float BONE_TREE_INDENT = 8.0f;
}

bool FSkeletalMeshViewerPanel::Init()
{
	// 에디터 엔진이 EditorWorld로 쥐지 않으므로 아웃라이너·씬 저장·PIE 복제에 잡히지 않는다
	PreviewWorld = UWorld::CreateWorld(EWorldType::SkeletalMeshViewer, false, "SkeletalMeshPreview");
	PreviewActor = PreviewWorld->SpawnActor<ASkeletalMeshActor>();

	// 라이트 아이콘과 화살표 메시는 프리뷰에 방해만 되니 숨기고 빛만 남긴다
	PreviewLight = PreviewWorld->SpawnActor<ADirectionalLightActor>();
	for (UActorComponent* Component : PreviewLight->GetComponents())
	{
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
			Primitive->SetVisible(false);
	}

	// 기즈모가 잡을 보이지 않는 대리 컴포넌트. 루트라서 Relative가 곧 World다
	AActor* PivotActor = PreviewWorld->SpawnActor<AActor>();
	BonePivot = Cast<USceneComponent>(PivotActor->AddComponent(USceneComponent::StaticClass(), "BonePivot"));
	PivotActor->SetRootComponent(BonePivot);

	// Component Space를 채워 둔다. 피킹·본 편집이 부모 본 행렬을 읽는다
	PreviewActor->GetSkeletalMeshComponent()->ApplyPose();

	ViewportClient = MakeUnique<FEditorViewportClient>();
	ViewportClient->SetWorld(PreviewWorld);
	ViewportClient->SetViewIndex(4); // 0~3은 메인 뷰포트 몫
	ViewportClient->Resize(1, 1);
	FocusCamera();

	// 시작할 때는 닫아 두고, 콘텐츠 드로어 더블클릭(OpenAsset)이나 Window 메뉴로 연다
	SetOpen(false);
	return true;
}

void FSkeletalMeshViewerPanel::Tick(float DeltaTime)
{
	(void)DeltaTime;
	if (!ViewportClient)
		return;
	if (RequestedWidth != ViewportClient->GetWidth() || RequestedHeight != ViewportClient->GetHeight())
		ViewportClient->Resize(RequestedWidth, RequestedHeight);

	// 라이트 방향(전방)을 카메라 전방과 맞춰 카메라가 보는 면이 정면으로 빛을 받게 한다.
	// 카메라 입력은 이 Tick보다 먼저, 렌더는 뒤에 돌아서 이번 프레임 카메라와 어긋나지 않는다
	if (PreviewLight)
		PreviewLight->GetLightComponent()->SetRelativeRotation(ViewportClient->GetViewRotation());

	UpdateBoneGizmoAndPicking();
}

void FSkeletalMeshViewerPanel::OnRender()
{
	if (!IsOpen() || !ViewportClient)
	{
		bHovered = false;
		bCameraCaptured = false;
		if (ViewportClient)
			ViewportClient->SetActive(false);
		return;
	}

	bool bOpen = true;
	bHovered = false;
	if (ImGui::Begin(GetPanelName(), &bOpen))
	{
		DrawGizmoToolbar();
		// 왼쪽은 뷰포트 이미지, 오른쪽은 본 트리
		const ImVec2 Avail = ImGui::GetContentRegionAvail();
		// 트리는 가장 긴 이름에 맞추고, 창이 좁으면 절반까지만 쓰고 가로 스크롤로 본다
		const float TreeWidth = std::min(CalcBoneTreeWidth(), Avail.x * 0.5f);
		const ImVec2 Size(Avail.x - TreeWidth - ImGui::GetStyle().ItemSpacing.x, Avail.y);
		const bool bVisible = Size.x >= 1.0f && Size.y >= 1.0f;
		ViewportClient->SetActive(bVisible);
		if (bVisible)
		{
			// 렌더는 다음 Tick 뒤에 이 크기로 한다. 이번 프레임은 지난 프레임 결과를 늘려 보여 준다
			RequestedWidth = static_cast<uint32>(Size.x);
			RequestedHeight = static_cast<uint32>(Size.y);
			ViewportClient->SetRect({ 0.0f, 0.0f, Size.x, Size.y });
			// Image는 입력을 받지 않는 아이템이라 그 위에서 드래그하면 ImGui가 창을 옮긴다.
			// 보이지 않는 버튼이 왼쪽·오른쪽 클릭을 차지해 기즈모 드래그와 카메라 조작이 창 이동으로 새지 않게 한다
			ImGui::InvisibleButton("##ViewerViewport", Size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
			bHovered = ImGui::IsItemHovered();
			const ImVec2 Min = ImGui::GetItemRectMin();
			if (FTexture2D* Color = ViewportClient->GetColorTarget(); Color && Color->GetSRV())
				ImGui::GetWindowDrawList()->AddImage(Color->GetSRV(), Min, ImGui::GetItemRectMax());
			ImageMin = FVector2(Min.x, Min.y);
			ImGui::SameLine();
		}
		// 본 계층은 깊어서(Manny 손가락 끝은 15단계 안팎) 넘치는 이름은 가로 스크롤로 본다
		if (ImGui::BeginChild("BoneTree", ImVec2(0.0f, Avail.y), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar))
			DrawBoneTree();
		ImGui::EndChild();
	}
	else
	{
		// 탭이 가려졌거나 접힌 상태
		ViewportClient->SetActive(false);
	}
	GatherCameraInput();
	ImGui::End();
	if (!bOpen)
	{
		SetOpen(false);
		ViewportClient->SetActive(false);
	}
}

void FSkeletalMeshViewerPanel::OpenAsset(USkeletalMesh* Mesh)
{
	if (!PreviewActor || !Mesh)
		return;
	// 이전 메시의 본 인덱스가 새 메시에서 엉뚱한 본을 가리키지 않게 선택부터 비운다
	SelectedBone = INDEX_NONE;
	BoneGizmo.SetTarget(nullptr);
	PreviewActor->GetSkeletalMeshComponent()->SetSkeletalMesh(Mesh);
	PreviewActor->GetSkeletalMeshComponent()->ApplyPose();
	FocusCamera();
	SetOpen(true);
	ImGui::SetWindowFocus(GetPanelName());
}

void FSkeletalMeshViewerPanel::Shutdown()
{
	// UObject 해제는 FEngineLoop가 종료 때 일괄로 한다. 여기서는 컴포넌트 등록과 씬 프록시만 정리한다
	if (PreviewWorld)
		PreviewWorld->ClearWorld();
	PreviewWorld = nullptr;
	PreviewActor = nullptr;
	PreviewLight = nullptr;
	BonePivot = nullptr;
	BoneGizmo.SetTarget(nullptr);
	ViewportClient.reset();
}

namespace
{
	constexpr int32 JOINT_SEGMENT_COUNT = 16;
	const FVector4 BONE_COLOR(0.02f, 0.04f, 0.3f, 1.0f);           // 짙은 남색
	const FVector4 SELECTED_BONE_COLOR(0.55f, 1.0f, 0.2f, 1.0f);   // 연두색
	const FVector4 DESCENDANT_BONE_COLOR(1.0f, 1.0f, 1.0f, 1.0f);  // 고른 본 아래 전부
	const FVector4 PARENT_BONE_COLOR(1.0f, 0.55f, 0.0f, 1.0f);     // 고른 본의 바로 위 부모

	// 선 셰이더(BoundingBoxShader.hlsl)가 픽셀 색에 0.5를 더하므로 미리 빼서 위 색이 그대로 나오게 한다
	FVector4 ToLineColor(const FVector4& Color)
	{
		return FVector4(Color.X - 0.5f, Color.Y - 0.5f, Color.Z - 0.5f, Color.W);
	}

	void AddWireCircle(FLineBatcher* LineBatcher, const FVector& Center, const FVector& AxisA, const FVector& AxisB,
		float Radius, const FVector4& Color)
	{
		FVector Prev = Center + AxisA * Radius;
		for (int32 i = 1; i <= JOINT_SEGMENT_COUNT; ++i)
		{
			const float Angle = 2.0f * PI * i / JOINT_SEGMENT_COUNT;
			const FVector Point = Center + AxisA * (std::cos(Angle) * Radius) + AxisB * (std::sin(Angle) * Radius);
			LineBatcher->AddLine(Prev, Point, Color);
			Prev = Point;
		}
	}

	void AddWireJoint(FLineBatcher* LineBatcher, const FVector& Center, float Radius, const FVector4& Color)
	{
		AddWireCircle(LineBatcher, Center, FVector(1, 0, 0), FVector(0, 1, 0), Radius, Color);
		AddWireCircle(LineBatcher, Center, FVector(1, 0, 0), FVector(0, 0, 1), Radius, Color);
		AddWireCircle(LineBatcher, Center, FVector(0, 1, 0), FVector(0, 0, 1), Radius, Color);
	}

	// 밑면(정사각형)은 부모 관절, 꼭짓점은 자식 관절
	void AddWirePyramid(FLineBatcher* LineBatcher, const FVector& Base, const FVector& Apex, float Radius, const FVector4& Color)
	{
		const FVector Dir = (Apex - Base).Normalized();
		// 뼈와 거의 평행한 축으로 외적하면 0이 되므로 그때는 다른 축을 쓴다
		const FVector Ref = std::abs(Dir.Z) < 0.99f ? FVector(0, 0, 1) : FVector(1, 0, 0);
		const FVector U = FVector::Cross(Dir, Ref).Normalized() * Radius;
		const FVector V = FVector::Cross(Dir, U.Normalized()) * Radius;

		const FVector Corners[4] = { Base + U + V, Base - U + V, Base - U - V, Base + U - V };
		for (int32 i = 0; i < 4; ++i)
		{
			LineBatcher->AddLine(Corners[i], Corners[(i + 1) % 4], Color);
			LineBatcher->AddLine(Corners[i], Apex, Color);
		}
	}
}

bool FSkeletalMeshViewerPanel::CalcJointWorldPositions(TArray<FVector>& OutJoints) const
{
	const USkeletalMeshComponent* Component = PreviewActor ? PreviewActor->GetSkeletalMeshComponent() : nullptr;
	const USkeletalMesh* Mesh = Component ? Component->GetSkeletalMesh() : nullptr;
	if (!Mesh || Mesh->Data.Bones.IsEmpty())
		return false;
	const TArray<FBoneInfo>& Bones = Mesh->Data.Bones;

	// Component Space는 ApplyPose 전에는 비어 있으므로 그때는 바인드(Ref) 자세로 직접 쌓는다
	TArray<FMatrix> RefSpace;
	const TArray<FMatrix>* ComponentSpace = &Component->GetComponentSpaceTransforms();
	if (ComponentSpace->Num() != Bones.Num())
	{
		RefSpace.SetNum(Bones.Num());
		for (int32 i = 0; i < Bones.Num(); ++i)
		{
			const int32 Parent = Bones[i].ParentIndex;
			RefSpace[i] = Parent >= 0 ? Bones[i].RefLocal * RefSpace[Parent] : Bones[i].RefLocal;
		}
		ComponentSpace = &RefSpace;
	}

	// 행벡터 규약이라 월드 = Component Space × 컴포넌트 월드, 관절 위치는 4행(이동)
	const FMatrix ComponentToWorld = Component->GetWorldMatrix();
	OutJoints.SetNum(Bones.Num());
	for (int32 i = 0; i < Bones.Num(); ++i)
	{
		const FMatrix World = (*ComponentSpace)[i] * ComponentToWorld;
		OutJoints[i] = FVector(World.M[3][0], World.M[3][1], World.M[3][2]);
	}
	return true;
}

void FSkeletalMeshViewerPanel::DrawBones(FLineBatcher* LineBatcher) const
{
	TArray<FVector> Joints;
	if (!LineBatcher || !CalcJointWorldPositions(Joints))
		return;
	const USkeletalMeshComponent* Component = PreviewActor->GetSkeletalMeshComponent();
	const TArray<FBoneInfo>& Bones = Component->GetSkeletalMesh()->Data.Bones;

	// 굵기는 뼈 길이에 비례시키되(손가락이 뭉개지지 않게) 메시 크기에 맞춘 상한을 둔다
	const FBox Bounds = Component->CalcBounds();
	const FVector Size = Bounds.Max - Bounds.Min;
	const float MaxRadius = std::max({ Size.X, Size.Y, Size.Z }) * 0.01f;
	TArray<float> JointRadius;
	JointRadius.SetNum(Bones.Num());
	for (float& Radius : JointRadius)
		Radius = MaxRadius;
	for (int32 i = 0; i < Bones.Num(); ++i)
	{
		const int32 Parent = Bones[i].ParentIndex;
		if (!Bones.IsValidIndex(Parent))
			continue;
		// 관절 구는 이어진 뼈 중 가장 짧은 것에 맞춘다
		const float Radius = (Joints[i] - Joints[Parent]).Length() * 0.1f;
		JointRadius[i] = std::min(JointRadius[i], Radius);
		JointRadius[Parent] = std::min(JointRadius[Parent], Radius);
	}

	// 본별 색. 부모 인덱스가 항상 자기보다 작아서 앞에서부터 한 번 훑으면 자손 여부가 정해진다
	const int32 SelectedParent = Bones.IsValidIndex(SelectedBone) ? Bones[SelectedBone].ParentIndex : INDEX_NONE;
	// TArray<bool>은 std::vector<bool> 특수화라 원소 참조가 안 된다
	TArray<uint8> bDescendant;
	bDescendant.SetNum(Bones.Num());
	TArray<const FVector4*> Colors;
	Colors.SetNum(Bones.Num());
	// 깊이 테스트 없이 그려서 나중에 그린 선이 위에 덮인다. 고른 본의 관절에는 자식 사각뿔의 밑면이 겹치므로
	// 강조할수록 뒤에 그린다 (0: 기본, 1: 자손·부모, 2: 고른 본)
	TArray<uint8> DrawOrder;
	DrawOrder.SetNum(Bones.Num());
	for (int32 i = 0; i < Bones.Num(); ++i)
	{
		const int32 Parent = Bones[i].ParentIndex;
		bDescendant[i] = Bones.IsValidIndex(Parent) && (Parent == SelectedBone || bDescendant[Parent]);
		DrawOrder[i] = 1;
		if (i == SelectedBone)
		{
			Colors[i] = &SELECTED_BONE_COLOR;
			DrawOrder[i] = 2;
		}
		else if (bDescendant[i])
			Colors[i] = &DESCENDANT_BONE_COLOR;
		else if (i == SelectedParent)
			Colors[i] = &PARENT_BONE_COLOR;
		else
		{
			Colors[i] = &BONE_COLOR;
			DrawOrder[i] = 0;
		}
	}

	for (uint8 Order = 0; Order <= 2; ++Order)
	{
		for (int32 i = 0; i < Bones.Num(); ++i)
		{
			if (DrawOrder[i] != Order)
				continue;
			const int32 Parent = Bones[i].ParentIndex;
			const FVector4 Color = ToLineColor(*Colors[i]);
			AddWireJoint(LineBatcher, Joints[i], JointRadius[i], Color);

			if (!Bones.IsValidIndex(Parent))
				continue;
			const float Length = (Joints[i] - Joints[Parent]).Length();
			if (Length <= 1e-4f)
				continue;
			// 사각뿔(부모→자식)은 자식 본의 색을 따른다: 고른 본으로 들어오는 뼈가 연두색이 된다
			AddWirePyramid(LineBatcher, Joints[Parent], Joints[i], std::min(Length * 0.1f, MaxRadius), Color);
		}
	}
}

int32 FSkeletalMeshViewerPanel::PickBone(const FVector2& MousePos) const
{
	TArray<FVector> Joints;
	if (!ViewportClient || !CalcJointWorldPositions(Joints))
		return INDEX_NONE;
	const TArray<FBoneInfo>& Bones = PreviewActor->GetSkeletalMeshComponent()->GetSkeletalMesh()->Data.Bones;

	const FMatrix ViewProj = ViewportClient->GetViewProjectionMatrix();
	const int ScreenW = static_cast<int>(ViewportClient->GetWidth());
	const int ScreenH = static_cast<int>(ViewportClient->GetHeight());
	TArray<FVector2> Screen;
	Screen.SetNum(Bones.Num());
	for (int32 i = 0; i < Bones.Num(); ++i)
		Screen[i] = WorldToScreen(Joints[i], ViewProj, ScreenW, ScreenH);
	// WorldToScreen은 카메라 뒤를 -FLT_MAX로 돌려준다
	auto IsOnScreen = [&Screen](int32 i) { return Screen[i].X > -FLT_MAX; };

	// 관절 하나에 자기 뼈의 끝과 자식 뼈들의 시작이 겹친다. 같은 거리면 앞(부모 쪽) 인덱스가 남도록 <로 비교해
	// 관절을 클릭하면 그 관절의 본이 골라진다
	constexpr float PickPixels = 8.0f;
	int32 Best = INDEX_NONE;
	float BestDist = PickPixels;
	for (int32 i = 0; i < Bones.Num(); ++i)
	{
		if (!IsOnScreen(i))
			continue;
		const float DX = MousePos.X - Screen[i].X;
		const float DY = MousePos.Y - Screen[i].Y;
		float Dist = std::sqrt(DX * DX + DY * DY);
		const int32 Parent = Bones[i].ParentIndex;
		if (Bones.IsValidIndex(Parent) && IsOnScreen(Parent))
			Dist = std::min(Dist, DistanceToSegment(MousePos, Screen[Parent], Screen[i]));
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = i;
		}
	}
	return Best;
}

void FSkeletalMeshViewerPanel::UpdateBoneGizmoAndPicking()
{
	const USkeletalMeshComponent* Component = PreviewActor ? PreviewActor->GetSkeletalMeshComponent() : nullptr;
	const USkeletalMesh* Mesh = Component ? Component->GetSkeletalMesh() : nullptr;
	const bool bHasBone = BonePivot && Mesh && Mesh->Data.Bones.IsValidIndex(SelectedBone);
	BoneGizmo.SetTarget(bHasBone ? BonePivot : nullptr);

	if (!ViewportClient->IsActive())
	{
		bLeftClicked = false;
		return;
	}

	// 드래그 중이 아니면 기즈모 위치를 본에 맞춘다 (트리에서 다른 본을 골랐거나 부모 본이 움직였을 때)
	if (bHasBone && !BoneGizmo.IsUsing())
		SyncPivotFromBone();

	// 우클릭 카메라 조작 중에는 기즈모를 잡지 않는다. 드래그를 시작한 뒤에는 창 밖으로 나가도 이어간다
	const bool bMouseDown = bLeftDown && !bCameraCaptured && (bHovered || BoneGizmo.IsUsing());
	const FRay Ray = ViewportClient->DeprojectScreenToWorld(MouseLocal);
	BoneGizmo.Update(Ray, MouseLocal, ViewportClient->GetViewProjectionMatrix(),
		static_cast<int>(ViewportClient->GetWidth()), static_cast<int>(ViewportClient->GetHeight()),
		bMouseDown, ViewportClient->GetViewLocation(), !ViewportClient->IsPerspective());

	if (BoneGizmo.IsUsing())
		ApplyPivotToBone();
	else if (bLeftClicked && BoneGizmo.GetHoveredAxis() < 0)
		SelectedBone = PickBone(MouseLocal);   // 빈 곳이면 INDEX_NONE이라 선택이 풀린다
	bLeftClicked = false;
}

void FSkeletalMeshViewerPanel::SyncPivotFromBone()
{
	const USkeletalMeshComponent* Component = PreviewActor->GetSkeletalMeshComponent();
	const TArray<FMatrix>& ComponentSpace = Component->GetComponentSpaceTransforms();
	if (!ComponentSpace.IsValidIndex(SelectedBone))
		return;
	// 위치·회전·스케일 분해는 SetWorldTransform에 맡긴다
	BonePivot->SetWorldTransform(ComponentSpace[SelectedBone] * Component->GetWorldMatrix(), false);
}

void FSkeletalMeshViewerPanel::ApplyPivotToBone()
{
	USkeletalMeshComponent* Component = PreviewActor->GetSkeletalMeshComponent();
	const TArray<FMatrix>& ComponentSpace = Component->GetComponentSpaceTransforms();
	const TArray<FBoneInfo>& Bones = Component->GetSkeletalMesh()->Data.Bones;
	if (!ComponentSpace.IsValidIndex(SelectedBone))
		return;

	// 월드 → Component Space → 부모 기준 Local. 행벡터라 역행렬은 오른쪽에 곱한다
	const FMatrix BoneComponentSpace = BonePivot->GetWorldMatrix() * Component->GetWorldMatrix().Inverse();
	const int32 Parent = Bones[SelectedBone].ParentIndex;
	const FMatrix Local = ComponentSpace.IsValidIndex(Parent)
		? BoneComponentSpace * ComponentSpace[Parent].Inverse()
		: BoneComponentSpace;
	if (Component->SetBoneLocalTransform(SelectedBone, Local))
		Component->ApplyPose();
}

void FSkeletalMeshViewerPanel::DrawGizmoToolbar()
{
	int32 Mode = static_cast<int32>(BoneGizmo.GetMode());
	if (ImGui::RadioButton("Translate", Mode == static_cast<int32>(EGizmoMode::Location))) BoneGizmo.SetMode(EGizmoMode::Location);
	ImGui::SameLine();
	if (ImGui::RadioButton("Rotate", Mode == static_cast<int32>(EGizmoMode::Rotation))) BoneGizmo.SetMode(EGizmoMode::Rotation);
	ImGui::SameLine();
	if (ImGui::RadioButton("Scale", Mode == static_cast<int32>(EGizmoMode::Scale))) BoneGizmo.SetMode(EGizmoMode::Scale);
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	const bool bLocal = BoneGizmo.GetSpace() == EGizmoSpace::Local;
	if (ImGui::RadioButton("Local", bLocal)) BoneGizmo.SetSpace(EGizmoSpace::Local);
	ImGui::SameLine();
	if (ImGui::RadioButton("World", !bLocal)) BoneGizmo.SetSpace(EGizmoSpace::World);
	ImGui::SameLine();
	ImGui::TextDisabled("(Space: cycle mode)");

	// 메인 에디터처럼 Space로 모드 순환. 뷰어 창에 포커스가 있을 때만
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !bCameraCaptured && !BoneGizmo.IsUsing()
		&& !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space, false))
	{
		Mode = (Mode + 1) % 3;
		BoneGizmo.SetMode(static_cast<EGizmoMode>(Mode));
	}

	if (ImGui::Button("Load First Clip"))
	{
		USkeletalMeshComponent* Component = PreviewActor ? PreviewActor->GetSkeletalMeshComponent() : nullptr;
		const USkeletalMesh* Mesh = Component ? Component->GetSkeletalMesh() : nullptr;
		if (!Mesh)
		{
			return;
		}

		const FString& MeshKey = Mesh->GetPath();
		const FString Path = "Assets/" + MeshKey;
		UAnimSequence* Sequence = UAssetManager::Get().LoadAnimationSequence(MeshKey, Path);

		const UAnimDataModel* Model = Sequence ? Sequence->GetDataModel() : nullptr;
		if (!Model)
		{
			HTR_LOG(Error, "Failed to load animation from '{}'", Path);
			return;
		}

		HTR_LOG(Info, "Animation loaded: {} tracks, {} keys per track, {:.3f} seconds", Model->GetBoneAnimationTracks().Num(), Model->GetNumberOfKeys(), Model->GetPlayLength());
	}
}

void FSkeletalMeshViewerPanel::DrawBoneTree()
{
	const USkeletalMesh* Mesh = PreviewActor ? PreviewActor->GetSkeletalMeshComponent()->GetSkeletalMesh() : nullptr;
	if (!Mesh)
	{
		ImGui::TextDisabled("No skeletal mesh");
		return;
	}
	if (Mesh != BoneTreeMesh)
		RebuildBoneChildren(Mesh);

	ImGui::Text("Bones (%d)", Mesh->Data.Bones.Num());
	ImGui::Separator();
	// 에디터 기본 들여쓰기(18px)는 깊은 계층에서 이름을 밀어내므로 좁힌다. 계층은 연결선으로 읽힌다
	ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, BONE_TREE_INDENT);
	for (int32 Root : RootBones)
		DrawBoneNode(Mesh, Root);
	ImGui::PopStyleVar();

	// 빈 곳을 클릭하면 선택 해제 (Outliner와 같은 동작)
	if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		SelectedBone = INDEX_NONE;
}

void FSkeletalMeshViewerPanel::DrawBoneNode(const USkeletalMesh* Mesh, int32 BoneIndex)
{
	const TArray<int32>& Children = BoneChildren[BoneIndex];
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
		| ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_DrawLinesToNodes;
	if (Children.IsEmpty())
		Flags |= ImGuiTreeNodeFlags_Leaf;
	if (BoneIndex == SelectedBone)
		Flags |= ImGuiTreeNodeFlags_Selected;

	// ID는 본 인덱스. 이름이 겹쳐도 노드가 섞이지 않는다
	const bool bOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(BoneIndex)), Flags,
		"%s", Mesh->Data.Bones[BoneIndex].Name.c_str());
	// 화살표로 접고 펼 때는 선택하지 않는다
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
		SelectedBone = BoneIndex;

	if (bOpen)
	{
		for (int32 Child : Children)
			DrawBoneNode(Mesh, Child);
		ImGui::TreePop();
	}
}

void FSkeletalMeshViewerPanel::RebuildBoneChildren(const USkeletalMesh* Mesh)
{
	BoneTreeMesh = Mesh;
	SelectedBone = INDEX_NONE;
	RootBones.Reset();
	BoneChildren.Reset();
	const TArray<FBoneInfo>& Bones = Mesh->Data.Bones;
	BoneChildren.SetNum(Bones.Num());
	BoneDepth.SetNum(Bones.Num());
	for (int32 Index = 0; Index < Bones.Num(); ++Index)
	{
		const int32 Parent = Bones[Index].ParentIndex;
		if (Bones.IsValidIndex(Parent))
		{
			BoneChildren[Parent].Add(Index);
			BoneDepth[Index] = BoneDepth[Parent] + 1;
		}
		else
		{
			RootBones.Add(Index);
			BoneDepth[Index] = 0;
		}
	}
}

float FSkeletalMeshViewerPanel::CalcBoneTreeWidth()
{
	const ImGuiStyle& Style = ImGui::GetStyle();
	const USkeletalMesh* Mesh = PreviewActor ? PreviewActor->GetSkeletalMeshComponent()->GetSkeletalMesh() : nullptr;
	float Content = ImGui::CalcTextSize("No skeletal mesh").x;
	if (Mesh)
	{
		if (Mesh != BoneTreeMesh)
			RebuildBoneChildren(Mesh);
		// 노드 한 줄 = 들여쓰기(깊이 × 8, DrawBoneTree와 같은 값) + 화살표 칸(FontSize + FramePadding × 2) + 이름
		const float ArrowWidth = ImGui::GetFontSize() + Style.FramePadding.x * 2.0f;
		const TArray<FBoneInfo>& Bones = Mesh->Data.Bones;
		for (int32 i = 0; i < Bones.Num(); ++i)
		{
			const float Row = BoneDepth[i] * BONE_TREE_INDENT + ArrowWidth + ImGui::CalcTextSize(Bones[i].Name.c_str()).x;
			Content = std::max(Content, Row);
		}
	}
	// 자식 창 안쪽 여백(양쪽)과 세로 스크롤바, 테두리를 더한다
	return Content + Style.WindowPadding.x * 2.0f + Style.ScrollbarSize + Style.ChildBorderSize * 2.0f;
}

void FSkeletalMeshViewerPanel::GatherCameraInput()
{
	// FInputSystem은 메인 창 메시지만 받아서, 메인 창 밖으로 꺼낸 ImGui 창에서는 입력이 오지 않는다.
	// ImGui IO는 모든 ImGui 창의 입력을 받으므로 뷰어 카메라는 이것으로 움직인다
	const ImGuiIO& IO = ImGui::GetIO();
	if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		bCameraCaptured = true;
		ImGui::SetWindowFocus(GetPanelName());
	}
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) || !ViewportClient->IsActive())
		bCameraCaptured = false;

	// 왼쪽 버튼은 본 피킹·기즈모용. 뷰포트 이미지 좌상단 기준 픽셀로 바꿔 둔다
	bLeftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	if (bHovered && !bCameraCaptured && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		bLeftClicked = true;
	MouseLocal = FVector2(IO.MousePos.x - ImageMin.X, IO.MousePos.y - ImageMin.Y);

	CameraInput.bCaptured = bCameraCaptured;
	CameraInput.bAllowWheel = bCameraCaptured || bHovered;
	if (bCameraCaptured)
	{
		CameraInput.MouseX += IO.MouseDelta.x;
		CameraInput.MouseY += IO.MouseDelta.y;
	}
	// ImGui 휠은 한 칸이 1.0, FInputSystem 휠은 한 칸이 WHEEL_DELTA(120)라 메인 뷰포트와 속도를 맞춘다
	if (CameraInput.bAllowWheel)
		CameraInput.Wheel += IO.MouseWheel * static_cast<float>(WHEEL_DELTA);
	CameraInput.bForward = ImGui::IsKeyDown(ImGuiKey_W);
	CameraInput.bBackward = ImGui::IsKeyDown(ImGuiKey_S);
	CameraInput.bRight = ImGui::IsKeyDown(ImGuiKey_D);
	CameraInput.bLeft = ImGui::IsKeyDown(ImGuiKey_A);
	CameraInput.bUp = ImGui::IsKeyDown(ImGuiKey_E);
	CameraInput.bDown = ImGui::IsKeyDown(ImGuiKey_Q);
}

FViewportCameraInput FSkeletalMeshViewerPanel::ConsumeCameraInput()
{
	const FViewportCameraInput Result = CameraInput;
	CameraInput.MouseX = 0.0f;
	CameraInput.MouseY = 0.0f;
	CameraInput.Wheel = 0.0f;
	return Result;
}

void FSkeletalMeshViewerPanel::FocusCamera()
{
	if (!PreviewActor || !ViewportClient)
		return;
	const FBox Bounds = PreviewActor->GetSkeletalMeshComponent()->CalcBounds();
	const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
	const FVector Size = Bounds.Max - Bounds.Min;
	const float Radius = std::max({ Size.X, Size.Y, Size.Z, 1.0f });
	// X가 앞이라 -X 쪽에서 +X를 바라본다. 거리는 화각을 따지지 않은 어림값
	ViewportClient->SetViewLocation(Center - FVector(Radius * 1.5f, 0.0f, 0.0f));
	ViewportClient->SetViewRotation(FRotator(0.0f, 0.0f, 0.0f));
}
