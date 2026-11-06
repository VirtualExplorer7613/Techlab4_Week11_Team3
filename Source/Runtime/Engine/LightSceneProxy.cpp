#include "EnginePCH.h"
#include "LightSceneProxy.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"

namespace
{
	// 캐스케이드 가중치가 1, e, e^2, ... 일 때 CascadeIndex 앞까지의 누적 비율 (0~1)
	float ComputeAccumulatedScale(float Exponent, int32 CascadeIndex, int32 CascadeCount)
	{
		if (CascadeIndex <= 0) return 0.0f;
		float CurrentScale = 1.0f;
		// 모든 캐스케이드 가중치의 합
		float TotalScale = 0.0f;
		// CascadeIndex 앞에 있는 캐스케이드 가중치의 합
		float Ret = 0.0f;
		for (int32 i = 0; i < CascadeCount; ++i)
		{
			if (i < CascadeIndex) Ret += CurrentScale;
			TotalScale += CurrentScale;
			CurrentScale *= Exponent;
		}
		return Ret / TotalScale;
	}

	// 카메라 프러스텀을 Distance(카메라 기준 깊이)에서 자른 단면의 네 코너
	void GetViewSliceCorners(const FSceneView& View, float Distance, FVector OutCorners[4])
	{
		const FFrustumCorners& C = View.FrustumCorners;
		const float T = (Distance - View.NearClip) / std::max(View.FarClip - View.NearClip, KINDA_SMALL_NUMBER);
		OutCorners[0] = C.NTL() + (C.FTL() - C.NTL()) * T;
		OutCorners[1] = C.NTR() + (C.FTR() - C.NTR()) * T;
		OutCorners[2] = C.NBL() + (C.FBL() - C.NBL()) * T;
		OutCorners[3] = C.NBR() + (C.FBR() - C.NBR()) * T;
	}
}

void FDirectionalLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& Out) const
{
	const UDirectionalLightComponent* Dir = static_cast<const UDirectionalLightComponent*>(LightComponent);

	FTransform Transform;
	Transform.Rotation = Dir->GetWorldRotation();
	const FLinearColor Color(Dir->GetLightColor());

	Out = FLightRenderParameters{};
	Out.Position = Dir->GetWorldLocation();
	Out.Type = static_cast<uint32>(ELightType::Directional);
	Out.Position = Dir->GetWorldLocation();
	Out.Direction = Transform.GetForward();
	Out.Color = FVector(Color.R, Color.G, Color.B);
	Out.Intensity = Dir->GetIntensity();
	Out.SourceSize = std::tan(FMath::DegreesToRadians(FMath::Clamp(Dir->GetAngularRadiusDegrees(), 0.0f, 10.0f)));
}

void FPointLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& Out) const
{
	const auto* PointLightComponent = static_cast<const UPointLightComponent*>(LightComponent);
	const FLinearColor Color(PointLightComponent->GetLightColor());

	Out = FLightRenderParameters{};
	Out.Type = static_cast<uint32>(ELightType::Point);
	Out.Position = PointLightComponent->GetWorldLocation();
	Out.Radius = PointLightComponent->GetAttenuationRadius();
	Out.Color = FVector(Color.R, Color.G, Color.B);
	Out.Intensity = PointLightComponent->GetIntensity();
	Out.FalloffExponent = PointLightComponent->GetFalloff();
	Out.SourceSize = PointLightComponent->GetSourceRadius();
}

void FSpotLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& Out) const
{
	const auto* SpotLightComponent = static_cast<const USpotLightComponent*>(LightComponent);
	const FLinearColor Color(SpotLightComponent->GetLightColor());

	FTransform Transform;
	Transform.Rotation = SpotLightComponent->GetWorldRotation();

	const float Outer = FMath::Clamp(SpotLightComponent->GetOuterConeAngle(), 0.0f, 89.0f);
	const float Inner = FMath::Clamp(SpotLightComponent->GetInnerConeAngle(), 0.0f, Outer);

	Out = FLightRenderParameters{};
	Out.Position = SpotLightComponent->GetWorldLocation();
	Out.Radius = SpotLightComponent->GetAttenuationRadius();
	Out.Color = FVector(Color.R, Color.G, Color.B);
	Out.Intensity = SpotLightComponent->GetIntensity();
	Out.FalloffExponent = SpotLightComponent->GetFalloff();

	Out.Type = static_cast<uint32>(ELightType::Spot);
	Out.Direction = Transform.GetForward();
	Out.InnerConeCos = std::cos(FMath::DegreesToRadians(Inner));
	Out.OuterConeCos = std::cos(FMath::DegreesToRadians(Outer));
	Out.SourceSize = SpotLightComponent->GetSourceRadius();
}

void FRectLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& Out) const
{
	const auto* RectLightComponent = static_cast<const URectLightComponent*>(LightComponent);
	const FLinearColor Color(RectLightComponent->GetLightColor());

	FTransform Transform;
	Transform.Rotation = RectLightComponent->GetWorldRotation();

	Out = FLightRenderParameters{};
	Out.Type = static_cast<uint32>(ELightType::Rect);
	Out.Position = RectLightComponent->GetWorldLocation();
	Out.Radius = RectLightComponent->GetAttenuationRadius();
	Out.Color = FVector(Color.R, Color.G, Color.B);
	Out.Intensity = RectLightComponent->GetIntensity();
	Out.FalloffExponent = 1.0f;
	Out.Direction = Transform.GetForward();
	Out.Right = Transform.GetRight();
	Out.HalfWidth = RectLightComponent->GetWidth() * 0.5f;
	Out.HalfHeight = RectLightComponent->GetHeight() * 0.5f;

	const float AngleRad = FMath::DegreesToRadians(FMath::Clamp(RectLightComponent->GetBarnDoorAngle(), 0.0f, 90.0f));
	Out.BarnDoorCos = std::cos(AngleRad);
	Out.BarnDoorSin = std::sin(AngleRad);
	Out.BarnDoorLength = std::max(RectLightComponent->GetBarnDoorLength(), 0.0f);
}

// ---------- 그림자 Initializer ----------

float FDirectionalLightSceneProxy::GetLightSourceAngle() const
{
	const UDirectionalLightComponent* Dir = static_cast<const UDirectionalLightComponent*>(LightComponent);
	return 2.0f * FMath::Clamp(Dir->GetAngularRadiusDegrees(), 0.0f, 10.0f);
}

uint32 FDirectionalLightSceneProxy::GetNumViewDependentWholeSceneShadows(const FSceneView& View) const
{
	const UDirectionalLightComponent* Dir = static_cast<const UDirectionalLightComponent*>(LightComponent);
	// 거리 0이면 동적 그림자 끔 (언리얼 주석: "A value of 0 disables the dynamic shadow")
	if (Dir->GetDynamicShadowDistanceMovableLight() <= 0.0f) return 0;
	return std::clamp(Dir->GetDynamicShadowCascades(), 1u, UDirectionalLightComponent::MaxDynamicShadowCascades);
}

bool FDirectionalLightSceneProxy::GetViewDependentWholeSceneProjectedShadowInitializer(const FSceneView& View, int32 InCascadeIndex,
	FWholeSceneProjectedShadowInitializer& OutInitializer) const
{
	const UDirectionalLightComponent* Dir = static_cast<const UDirectionalLightComponent*>(LightComponent);

	const FSphere Bounds = GetShadowSplitBounds(View, InCascadeIndex, &OutInitializer.CascadeSettings);
	const float R = Bounds.Radius;
	if (R <= 0.0f) return false;

	OutInitializer.PreShadowTranslation = Bounds.Center * -1.0f;
	OutInitializer.WorldToLight = Dir->GetWorldRotation().Quaternion().ToFMatrix().GetTransposed();
	OutInitializer.Scales = FVector2(1.0f / R, 1.0f / R);
	OutInitializer.SubjectBounds = FBoxSphereBounds(FVector::ZeroVector, FVector(R), R);
	OutInitializer.WAxis = FVector4(0, 0, 0, 1);
	// 구 뒤쪽(빛 쪽)으로 더 늘려 화면 밖에서 그림자를 드리우는 Caster도 담는다.
	OutInitializer.MinLightW = -R * 2.0f;
	OutInitializer.MaxDistanceToCastInLightW = R;
	OutInitializer.bOnePassPointLightShadow = false;
	return true;
}

FSphere FDirectionalLightSceneProxy::GetShadowSplitBounds(const FSceneView& View, int32 InCascadeIndex, FShadowCascadeSettings* OutCascadeSettings) const
{
	const UDirectionalLightComponent* Dir = static_cast<const UDirectionalLightComponent*>(LightComponent);
	const int32 NumCascades = static_cast<int32>(GetNumViewDependentWholeSceneShadows(View));

	const float SplitNear = GetSplitDistance(View, InCascadeIndex);
	float SplitFar = GetSplitDistance(View, InCascadeIndex + 1);
	const float FadePlane = SplitFar;

	// 마지막이 아니면 다음 캐스케이드와 섞을 구간만큼 뒤로 늘린다 (언리얼과 같은 방식).
	const float TransitionFraction = FMath::Clamp(Dir->GetCascadeTransitionFraction(), 0.0f, 0.3f);
	const float FadeExtension = (SplitFar - SplitNear) * TransitionFraction;
	const bool bLastCascade = InCascadeIndex >= NumCascades - 1;
	if (!bLastCascade) SplitFar += FadeExtension;

	FVector Center = FVector::ZeroVector;
	float RadiusSquared = 0.0f;
	if (View.bIsPerspective)
	{
		// 언리얼 GetShadowSplitBoundsDepthRange: FOV와 near/far만으로 구를 정한다.
		// 반지름이 카메라 위치·회전과 무관하게 항상 같아야 텍셀 스냅 격자가 프레임마다 흔들리지 않는다.
		// (월드 좌표 코너로 구하면 원점에서 먼 곳에서 float 오차로 반지름이 매 프레임 조금씩 바뀌어 그림자가 떨린다)
		// 엔진 투영 행렬은 X가 앞쪽이라 가로/세로 배율이 M[1][0], M[2][1]에 있다.
		const float TanX = 1.0f / std::max(std::abs(View.ProjectionMatrix.M[1][0]), KINDA_SMALL_NUMBER);
		const float TanY = 1.0f / std::max(std::abs(View.ProjectionMatrix.M[2][1]), KINDA_SMALL_NUMBER);
		const float DiagonalScaleSq = TanX * TanX + TanY * TanY;
		const float FarDiagonalSq = DiagonalScaleSq * SplitFar * SplitFar;
		const float NearDiagonalSq = DiagonalScaleSq * SplitNear * SplitNear;
		const float FrustumLength = std::max(SplitFar - SplitNear, KINDA_SMALL_NUMBER);
		const float OptimalOffset = (NearDiagonalSq - FarDiagonalSq) / (2.0f * FrustumLength) + FrustumLength * 0.5f;
		const float CentreZ = FMath::Clamp(SplitFar - OptimalOffset, SplitNear, SplitFar);
		Center = View.ViewLocation + View.ViewForward * CentreZ;
		RadiusSquared = std::max(FarDiagonalSq + (SplitFar - CentreZ) * (SplitFar - CentreZ),
			NearDiagonalSq + (SplitNear - CentreZ) * (SplitNear - CentreZ));
		RadiusSquared = std::max(RadiusSquared, 1.0f);
	}
	else
	{
		FVector Corners[8];
		GetViewSliceCorners(View, SplitNear, Corners);
		GetViewSliceCorners(View, SplitFar, Corners + 4);

		for (const FVector& P : Corners) Center += P;
		Center = Center * (1.0f / 8.0f);

		for (const FVector& P : Corners)
			RadiusSquared = std::max(RadiusSquared, (P - Center).SizeSquared());
	}

	if (OutCascadeSettings)
	{
		OutCascadeSettings->SplitNear = SplitNear;
		OutCascadeSettings->SplitFar = SplitFar;
		OutCascadeSettings->ShadowSplitIndex = InCascadeIndex;
		OutCascadeSettings->SplitFarFadeRegion = bLastCascade ? 0.0f : FadeExtension;
		// 앞 캐스케이드가 이 캐스케이드 안쪽으로 늘린 길이
		const float PrevNear = InCascadeIndex > 0 ? GetSplitDistance(View, InCascadeIndex - 1) : SplitNear;
		OutCascadeSettings->SplitNearFadeRegion = InCascadeIndex > 0 ? (SplitNear - PrevNear) * TransitionFraction : 0.0f;

		if (bLastCascade)
		{
			// 마지막 캐스케이드는 끝부분에서 그림자 자체를 서서히 끈다.
			const float Fadeout = (SplitFar - SplitNear) * FMath::Clamp(Dir->GetShadowDistanceFadeoutFraction(), 0.0f, 1.0f);
			OutCascadeSettings->FadePlaneOffset = SplitFar - Fadeout;
		}
		else
		{
			OutCascadeSettings->FadePlaneOffset = FadePlane;
		}
		OutCascadeSettings->FadePlaneLength = SplitFar - OutCascadeSettings->FadePlaneOffset;
	}

	return FSphere(Center, std::sqrt(RadiusSquared));
}

float FDirectionalLightSceneProxy::GetSplitDistance(const FSceneView& View, uint32 SplitIndex) const
{
	const UDirectionalLightComponent* Dir = static_cast<const UDirectionalLightComponent*>(LightComponent);
	const float ShadowNear = View.NearClip;
	const float ShadowFar = std::min(Dir->GetDynamicShadowDistanceMovableLight(), View.FarClip);
	const float Exponent = FMath::Clamp(Dir->GetCascadeDistributionExponent(), 1.0f, 4.0f);
	return ShadowNear + ComputeAccumulatedScale(Exponent, static_cast<int32>(SplitIndex),
		static_cast<int32>(GetNumViewDependentWholeSceneShadows(View))) * (ShadowFar - ShadowNear);
}

FSphere FPointLightSceneProxy::GetBoundingSphere() const
{
	const auto* Point = static_cast<const UPointLightComponent*>(LightComponent);
	return FSphere(Point->GetWorldLocation(), Point->GetAttenuationRadius());
}

bool FPointLightSceneProxy::GetWholeSceneProjectedShadowInitializer(const FSceneView& View,
	TArray<FWholeSceneProjectedShadowInitializer>& OutInitializers) const
{
	const auto* Point = static_cast<const UPointLightComponent*>(LightComponent);
	const FVector LightPos = Point->GetWorldLocation();
	const float Radius = Point->GetAttenuationRadius();
	if (Radius <= 0.0f) return false;

	// 큐브 6면 (+X, -X, +Y, -Y, +Z, -Z). 엔진 투영 행렬은 X를 앞쪽으로 받는다.
	const FRotator Faces[6] =
	{
		FRotator{ 0, 0, 0 },
		FRotator{ 0, 180, 0 },
		FRotator{ 0, 90, 0 },
		FRotator{ 0, -90, 0 },
		FRotator{ 90, 0, 0 },
		FRotator{ -90, 0, 0 },
	};

	for (int32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
	{
		FWholeSceneProjectedShadowInitializer& OutInitializer = OutInitializers.AddDefaulted_GetRef();
		OutInitializer.PreShadowTranslation = LightPos * -1.0f;
		OutInitializer.WorldToLight = Faces[FaceIndex].Quaternion().ToFMatrix().GetTransposed();
		OutInitializer.Scales = FVector2(1.0f, 1.0f);   // 90도 시야
		OutInitializer.SubjectBounds = FBoxSphereBounds(FVector::ZeroVector, FVector(Radius), Radius);
		OutInitializer.WAxis = FVector4(0, 0, 1, 0);
		OutInitializer.MinLightW = Radius * 0.02f;
		OutInitializer.MaxDistanceToCastInLightW = Radius;
		OutInitializer.CascadeSettings.ShadowSplitIndex = FaceIndex;
		OutInitializer.bOnePassPointLightShadow = false;
	}
	return true;
}

FSphere FSpotLightSceneProxy::GetBoundingSphere() const
{
	const auto* Spot = static_cast<const USpotLightComponent*>(LightComponent);
	const float Radius = Spot->GetAttenuationRadius();
	const float CosOuter = std::cos(FMath::DegreesToRadians(FMath::Clamp(Spot->GetOuterConeAngle(), 0.0f, 89.0f)));

	FTransform Transform;
	Transform.Rotation = Spot->GetWorldRotation();

	// 언리얼과 같은 근사: 원뿔 축 중간점에서 원뿔 가장자리 끝까지의 거리 (코사인 법칙)
	const float BoundsRadius = std::sqrt(1.25f * Radius * Radius - Radius * Radius * CosOuter);
	return FSphere(Spot->GetWorldLocation() + Transform.GetForward() * (0.5f * Radius), BoundsRadius);
}

bool FSpotLightSceneProxy::GetWholeSceneProjectedShadowInitializer(const FSceneView& View,
	TArray<FWholeSceneProjectedShadowInitializer>& OutInitializers) const
{
	const auto* Spot = static_cast<const USpotLightComponent*>(LightComponent);
	const FVector LightPos = Spot->GetWorldLocation();
	const float Radius = Spot->GetAttenuationRadius();
	if (Radius <= 0.0f) return false;

	const float Outer = FMath::Clamp(Spot->GetOuterConeAngle(), 1.0f, 89.0f);
	const float InvTan = 1.0f / std::tan(FMath::DegreesToRadians(Outer));

	FWholeSceneProjectedShadowInitializer& OutInitializer = OutInitializers.AddDefaulted_GetRef();
	OutInitializer.PreShadowTranslation = LightPos * -1.0f;
	OutInitializer.WorldToLight = Spot->GetWorldRotation().Quaternion().ToFMatrix().GetTransposed();
	OutInitializer.Scales = FVector2(InvTan, InvTan);
	OutInitializer.SubjectBounds = FBoxSphereBounds(FVector::ZeroVector, FVector(Radius), Radius);
	OutInitializer.WAxis = FVector4(0, 0, 1, 0);
	OutInitializer.MinLightW = Radius * 0.02f;
	OutInitializer.MaxDistanceToCastInLightW = Radius;
	OutInitializer.bOnePassPointLightShadow = false;
	return true;
}
