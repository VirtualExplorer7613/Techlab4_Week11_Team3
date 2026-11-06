#include "EnginePCH.h"
#include "DecalActor.h"
#include "Asset/AssetManager.h"

ADecalActor::ADecalActor()
{
	DecalComponent = CreateDefaultSubobject<UDecalComponent>("UDecalComponent");
	SetRootComponent(DecalComponent);

	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	BillboardComponent->SetEditorIcon(true);
	BillboardComponent->SetHiddenInGame(true);
	BillboardComponent->SetIconSize(2.0f);
	BillboardComponent->SetupAttachment(DecalComponent);
	BillboardComponent->SetHiddenInDetails(true);
	BillboardComponent->bCastShadow = false;
	BillboardComponent->bReceiveShadow = false;

	UMaterial* Base = UAssetManager::GetAssetByPath<UMaterial>("BillboardMaterial");
	UTexture2D* Icon = UAssetManager::Get().LoadTexture("Assets/Icons/Decal.png");
	if (Base && Icon)
	{
		UMaterial* IconMat = UMaterial::CreateInstance(Base);
		IconMat->Textures[0] = Icon;
		BillboardComponent->SetMaterial(0, IconMat);
	}

	ArrowComponent = CreateDefaultSubobject<UStaticMeshComponent>("UArrowComponent");
	ArrowComponent->SetEditorIcon(true);
	ArrowComponent->SetHiddenInGame(true);
	ArrowComponent->SetupAttachment(DecalComponent);
	ArrowComponent->SetStaticMesh(UAssetManager::GetAssetByPath<UStaticMesh>("Arrow"));
	ArrowComponent->SetMaterial(0, UAssetManager::GetAssetByPath<UMaterial>("EditorArrowMaterial"));
	ArrowComponent->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	ArrowComponent->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.5f));
	ArrowComponent->SetHiddenInDetails(true);
	ArrowComponent->SetCastShadow(false);
	ArrowComponent->SetReceiveShadow(false);
}