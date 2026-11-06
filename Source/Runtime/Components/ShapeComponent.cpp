#include "EnginePCH.h"
#include "ShapeComponent.h"
#include "Render/LineBatcher.h"
#include "UObject/UObjectIterator.h"

UShapeComponent::UShapeComponent()
{
	bCastShadow = false;
	bReceiveShadow = false;
}

FBox UShapeComponent::GetWorldAABB() const
{
	// 자식이 CalcLocalBounds를 채우면 월드 변환은 SceneComponent가 처리한다
	return CalcBounds();
}


FVector4 UShapeComponent::GetDebugColor() const
{
	if (!GetOverlapInfos().IsEmpty())  
		return FVector4(1.0f, 0.25f, 0.25f, 1.0f);
	return FVector4(ShapeColor.R / 255.0f, ShapeColor.G / 255.0f, ShapeColor.B / 255.0f, 1.0f);
}

void UShapeComponent::AddWireArc(FLineBatcher* LineBatcher, const FVector& Center, const FVector& AxisA, const FVector& AxisB,
	float Radius, float StartRad, float EndRad, const FVector4& Color, int32 Segments)
{
	auto PointAt = [&](float T) { return Center + AxisA * (std::cos(T) * Radius) + AxisB * (std::sin(T) * Radius); };
	FVector Prev = PointAt(StartRad);
	for (int32 i = 1; i <= Segments; ++i)
	{
		const FVector P = PointAt(StartRad + (EndRad - StartRad) * i / Segments);
		LineBatcher->AddLine(Prev, P, Color);
		Prev = P;
	}
}

void DrawCollisionShapes(UWorld* World, FLineBatcher* LineBatcher, const AActor* Selected)
{
	if (!World || !LineBatcher) return;
	for (TObjectIterator<UShapeComponent> It; It; ++It)
	{
		UShapeComponent* Shape = *It;
		if (!Shape || Shape->GetWorld() != World || !Shape->IsRegistered()) continue;
		if (Shape->bDrawOnlyIfSelected && Shape->GetOwner() != Selected) continue;
		Shape->DrawDebug(LineBatcher, Shape->GetDebugColor());
	}
}