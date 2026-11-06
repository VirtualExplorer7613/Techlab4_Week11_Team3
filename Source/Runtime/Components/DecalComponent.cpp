#include "EnginePCH.h"
#include "DecalComponent.h"
#include "Render/LineBatcher.h"
#include "Asset/AssetManager.h"

UDecalComponent::UDecalComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

FBox UDecalComponent::CalcLocalBounds() const
{
	return FBox{DecalSize * -1.f, DecalSize};
}

void UDecalComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	LineBatcher->AddOrientedBox(CalcLocalBounds(), GetWorldMatrix(), FVector4(0.2f, 0.8f, 1.f, 1.f));
}

FMatrix UDecalComponent::GetWorldToDecal() const
{
	FMatrix ScaleMatrix = FMatrix::Identity;
	ScaleMatrix[0][0] = DecalSize.X;
	ScaleMatrix[1][1] = DecalSize.Y;
	ScaleMatrix[2][2] = DecalSize.Z;

	FMatrix DecalToWorld = ScaleMatrix * GetWorldMatrix();
	return DecalToWorld.Inverse();
}

void UDecalComponent::ResolveTexture()
{
	if (DecalTexturePath.empty())
	{
		DecalTexture = nullptr;
		return;
	}
	DecalTexture = UAssetManager::Get().LoadTexture(DecalTexturePath);
}

void UDecalComponent::OnRegister()
{
	Super::OnRegister();
	ResolveTexture();
}

void UDecalComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "DecalTexturePath")
	{
		ResolveTexture();
	}
}

void UDecalComponent::Serialize(FStructuredArchive::FRecord Record)
{
	// 불러오기는 스폰(OnRegister에서 기본 텍스처를 찾음) 뒤에 경로 문자열만 덮어쓴다. 불러온 경로로 텍스처를 다시 찾는다
	Super::Serialize(Record);

	if (Record.GetUnderlyingArchive().IsLoading())
		ResolveTexture();
}

FOBB UDecalComponent::GetDecalOBB() const
{
	return FOBB::FromLocalBox(CalcLocalBounds(), GetWorldMatrix());
}

void UDecalComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	
	ElapsedTime += DeltaTime;
}

float UDecalComponent::GetFadeAlpha() const
{
	float FadeIn = 1.f;
	if (ElapsedTime < FadeInStartDelay)
	{
		FadeIn = 0.f;
	}
	else if (FadeInDuration > 0.f)
	{
		FadeIn = std::clamp((ElapsedTime - FadeInStartDelay) / FadeInDuration, 0.f, 1.f);
	}

	float FadeOut = 1.f;
	if (FadeDuration > 0.f)
	{
		FadeOut = std::clamp(1.f - (ElapsedTime - FadeStartDelay) / FadeDuration, 0.f, 1.f);
	}

	return FadeIn * FadeOut;
}