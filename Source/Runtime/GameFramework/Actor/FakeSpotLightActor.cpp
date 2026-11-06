#include "EnginePCH.h"
#include "FakeSpotLightActor.h"
#include "Asset/AssetManager.h"

AFakeSpotLightActor::AFakeSpotLightActor()
{
	constexpr float LightReach = 10.f;   // 빛이 닿는 거리
	constexpr float LightRadius = 3.f;    // 바닥에서 원 반경

	DecalComponent = CreateDefaultSubobject<UDecalComponent>("UDecalComponent");
	SetRootComponent(DecalComponent);
	DecalComponent->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	DecalComponent->SetDecalSize(FVector(LightReach * 0.5f, LightRadius, LightRadius));
	DecalComponent->SetDecalTexturePath("Assets/textures/SpotLight.png");
	DecalComponent->SetAdditiveBlend(true);
	DecalComponent->SetDecalColor(FColor(255, 245, 225, 255));
	DecalComponent->SetIntensity(1.f);

	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	BillboardComponent->SetEditorIcon(true);
	BillboardComponent->SetHiddenInGame(true);
	BillboardComponent->SetIconSize(2.0f);
	BillboardComponent->SetupAttachment(DecalComponent);
	BillboardComponent->SetHiddenInDetails(true);
	BillboardComponent->SetCastShadow(false);
	BillboardComponent->SetReceiveShadow(false);
	BillboardComponent->SetRelativeLocation(FVector(-LightReach * 0.5f, 0.f, 0.f));

	UMaterial* Base = UAssetManager::GetAssetByPath<UMaterial>("BillboardMaterial");
	UTexture2D* Icon = UAssetManager::Get().LoadTexture("Assets/Icons/Spot.png");
	if (Base && Icon)
	{
		UMaterial* IconMat = UMaterial::CreateInstance(Base);
		IconMat->Textures[0] = Icon;
		BillboardComponent->SetMaterial(0, IconMat);
	}

	ArrowComponent = CreateDefaultSubobject<UStaticMeshComponent>("UArrowComponent");
	ArrowComponent->SetEditorIcon(true);
	ArrowComponent->SetHiddenInGame(true);
	ArrowComponent->SetupAttachment(BillboardComponent);
	ArrowComponent->SetStaticMesh(UAssetManager::GetAssetByPath<UStaticMesh>("Arrow"));
	ArrowComponent->SetMaterial(0, UAssetManager::GetAssetByPath<UMaterial>("EditorArrowMaterial"));
	ArrowComponent->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	ArrowComponent->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.5f));
	ArrowComponent->SetHiddenInDetails(true);
	ArrowComponent->SetCastShadow(false);
	ArrowComponent->SetReceiveShadow(false);
}