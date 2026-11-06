#include "EnginePCH.h"
#include "Render/ShadowRendering.h"
#include "Render/SceneRenderer.h"
#include "Render/LightSceneInfo.h"
#include "Engine/LightSceneProxy.h"
#include "Engine/Scene.h"
#include "Engine/World.h"
#include "Math/QuadTree.h"
#include "Render/RenderResourceManager.h"
#include "Render/RenderCommand.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

FShadowConsoleVariables GShadowCVars;

namespace
{
	uint32 RoundDownToPowerOfTwo(uint32 Value)
	{
		uint32 Result = 1;
		while ((Result << 1) <= Value) Result <<= 1;
		return Result;
	}

	// 구가 화면에서 차지하는 반지름(픽셀). 카메라가 구 안에 있으면 FLT_MAX.
	float ComputeScreenRadius(const FSceneView& View, const FSphere& Bounds)
	{
		// 엔진 투영 행렬은 X가 앞쪽이라 화면 X/Y 배율이 M[1][0], M[2][1]에 있다.
		const float ScreenScale = std::max(0.5f * View.ViewRect.Width * std::abs(View.ProjectionMatrix.M[1][0]),
			0.5f * View.ViewRect.Height * std::abs(View.ProjectionMatrix.M[2][1]));
		if (!View.bIsPerspective) return ScreenScale * Bounds.Radius;

		const float Distance = (Bounds.Center - View.ViewLocation).Length();
		if (Distance <= Bounds.Radius) return FLT_MAX;
		return ScreenScale * Bounds.Radius / Distance;
	}

	// 언리얼 CalculateShadowFadeAlpha. FadeResolution 위는 1, MinResolution 아래는 0.
	float CalculateShadowFadeAlpha(float MaxUnclampedResolution, uint32 ShadowFadeResolution, uint32 MinShadowResolution)
	{
		if (MaxUnclampedResolution > ShadowFadeResolution) return 1.0f;
		if (MaxUnclampedResolution <= MinShadowResolution || ShadowFadeResolution <= MinShadowResolution) return 0.0f;
		const float T = (MaxUnclampedResolution - MinShadowResolution) / static_cast<float>(ShadowFadeResolution - MinShadowResolution);
		return std::pow(T, GShadowCVars.FadeExponent);
	}

	// 타일 하나의 최소 크기. 테두리를 빼고도 그릴 영역이 남아야 한다.
	constexpr uint32 MinTileSize = ShadowBorder * 4;

	// Spot/Point 그림자 타일 크기(테두리 포함, 2의 거듭제곱). 0이면 너무 작아서 그림자를 만들지 않는다.
	uint32 ComputeWholeSceneShadowResolution(const FSceneView& View, const FSphere& LightBounds, float ResolutionScale, float& OutFadeAlpha)
	{
		const FShadowConsoleVariables& CVars = GShadowCVars;
		const float ScreenRadius = ComputeScreenRadius(View, LightBounds);
		const float Unclamped = ScreenRadius >= FLT_MAX ? FLT_MAX : ScreenRadius * CVars.TexelsPerPixel * ResolutionScale;

		OutFadeAlpha = CalculateShadowFadeAlpha(Unclamped, CVars.FadeResolution, CVars.MinResolution);
		if (OutFadeAlpha < 1.0f / 256.0f) return 0;

		const uint32 MaxResolution = std::min(CVars.MaxResolution, ShadowAtlasSize);
		const float Clamped = std::clamp(Unclamped, static_cast<float>(CVars.MinResolution), static_cast<float>(MaxResolution));
		return std::max(RoundDownToPowerOfTwo(static_cast<uint32>(Clamped)), MinTileSize);
	}

	// 캐스케이드 타일 크기(테두리 포함, 2의 거듭제곱). 카메라를 따라가므로 화면 크기와 무관하다.
	uint32 ComputeCSMResolution(float ResolutionScale)
	{
		const FShadowConsoleVariables& CVars = GShadowCVars;
		const float Desired = CVars.MaxCSMResolution * ResolutionScale;
		const float Clamped = std::clamp(Desired, static_cast<float>(CVars.MinResolution), static_cast<float>(ShadowAtlasSize));
		return std::max(RoundDownToPowerOfTwo(static_cast<uint32>(Clamped)), MinTileSize);
	}
}

void FProjectedShadowInfo::SetupWholeSceneProjection(FLightSceneInfo* InLightSceneInfo, const FWholeSceneProjectedShadowInitializer& Initializer,
	uint32 InResolutionX, uint32 InResolutionY, uint32 InBorderSize)
{
	LightSceneInfo = InLightSceneInfo;
	PreShadowTranslation = Initializer.PreShadowTranslation;
	ResolutionX = InResolutionX;
	ResolutionY = InResolutionY;
	BorderSize = InBorderSize;
	bWholeSceneShadow = true;
	bOnePassPointLightShadow = Initializer.bOnePassPointLightShadow;
	CascadeSettings = Initializer.CascadeSettings;

	// 깊이 범위: near = MinLightW, far = MaxDistanceToCastInLightW
	MinSubjectZ = Initializer.MinLightW;
	MaxSubjectZ = Initializer.MaxDistanceToCastInLightW;

	bDirectionalLight = Initializer.WAxis.W == 1.0f;
	if (bDirectionalLight)
	{
		// 그림자 카메라를 텍셀 한 칸 단위로만 움직이게 해서, 텍셀 격자가 항상 월드의 같은 자리에 놓이게 한다.
		const float Radius = 1.0f / Initializer.Scales.X;   // 정사영 폭의 절반
		const float TexelWorld = 2.0f * Radius / static_cast<float>(std::max(ResolutionX, 1u));

		// 월드 -> 라이트 공간 (회전만). X가 깊이, Y/Z가 그림자 맵의 가로/세로
		FVector LightSpace = Initializer.WorldToLight.TransformPosition(PreShadowTranslation * -1.0f);
		LightSpace.Y = std::floor(LightSpace.Y / TexelWorld) * TexelWorld;
		LightSpace.Z = std::floor(LightSpace.Z / TexelWorld) * TexelWorld;

		// 라이트 -> 월드 (회전의 역 = 전치)
		PreShadowTranslation = Initializer.WorldToLight.GetTransposed().TransformPosition(LightSpace) * -1.0f;
	}

	// View: 라이트를 원점으로 옮기고(PreShadowTranslation) 라이트 방향으로 돌린다(WorldToLight).
	// 엔진 투영 행렬이 X를 앞쪽으로 받으므로 언리얼의 FaceMatrix는 곱하지 않는다.
	ShadowView = FMatrix::MakeTranslation(PreShadowTranslation) * Initializer.WorldToLight;

	// Scales = 1 / tan(반각). Week9 Perspective는 FOV를 라디안으로 받고, Aspect = 가로 / 세로.
	const float Aspect = Initializer.Scales.Y / Initializer.Scales.X;
	if (bDirectionalLight)
	{
		ShadowProjection = Ortho(2.0f / Initializer.Scales.X, Aspect, MinSubjectZ, MaxSubjectZ);
	}
	else
	{
		const float FovY = 2.0f * std::atan(1.0f / Initializer.Scales.Y);
		ShadowProjection = Perspective(FovY, Aspect, MinSubjectZ, MaxSubjectZ);
	}

	ShadowViewProjection = ShadowView * ShadowProjection;   // 카메라와 같은 View * Proj 순서
	CasterFrustum = ExtractFrustumPlanes(ShadowViewProjection);

	// 언리얼(Directional이 아닐 때): 라이트 위치 중심, SubjectBounds 반지름
	ShadowBounds = FSphere(PreShadowTranslation * -1.0f, Initializer.SubjectBounds.SphereRadius);

	UpdateShaderDepthBias();
}

void FProjectedShadowInfo::UpdateShaderDepthBias()
{
	const FLightSceneProxy* Proxy = LightSceneInfo->Proxy;
	const float DepthRange = std::max(MaxSubjectZ - MinSubjectZ, 1.0e-4f);
	float DepthBias = 0.0f;
	float SlopeScaleDepthBias = 1.0f;

	if (IsWholeSceneDirectionalShadow())
	{
		// 깊이 범위로 나눠 정규화한 뒤, 텍셀 하나의 월드 크기(반지름 / 해상도)에 비례하게 한다.
		DepthBias = GShadowCVars.CSMDepthBias / DepthRange;
		const float WorldSpaceTexelScale = ShadowBounds.Radius / static_cast<float>(std::max(ResolutionX, 1u));
		const float Distribution = CascadeSettings.CascadeBiasDistribution;   // 기본 1
		DepthBias = DepthBias + (DepthBias * WorldSpaceTexelScale - DepthBias) * Distribution;
		DepthBias *= Proxy->GetUserShadowBias();

		SlopeScaleDepthBias = GShadowCVars.CSMSlopeScaleDepthBias * Proxy->GetUserShadowSlopeBias();
	}
	else
	{
		const bool bPoint = IsWholeScenePointLightShadow();
		const float Resolution = static_cast<float>(std::max(std::max(ResolutionX, ResolutionY), 1u));
		const float LightTypeDepthBias = bPoint ? GShadowCVars.PointLightDepthBias : GShadowCVars.SpotLightDepthBias;

		DepthBias = LightTypeDepthBias * UnrealUnitsToWorld * 512.0f / (DepthRange * Resolution);
		DepthBias *= 2.0f * Proxy->GetUserShadowBias();   // UE: "* 2.0f to be compatible with the system we had before ShadowBias"
		DepthBias = std::min(DepthBias, 0.1f);            // 저해상도에서 bias가 커져 근평면을 넘지 않게

		SlopeScaleDepthBias = (bPoint ? GShadowCVars.PointLightSlopeScaleDepthBias : GShadowCVars.SpotLightSlopeDepthBias)
			* Proxy->GetUserShadowSlopeBias();
	}

	ShaderDepthBias = std::max(DepthBias, 0.0f);
	ShaderSlopeDepthBias = std::max(DepthBias * SlopeScaleDepthBias, 0.0f);   // 기울기 bias = 상수 bias × 배율
	ShaderMaxSlopeDepthBias = GShadowCVars.ShadowMaxSlopeScaleDepthBias;
}

void FSceneRenderer::InitDynamicShadows(const FSceneRenderOptions& Options)
{
	RenderData.VisibleLightInfos.Reset();
	RenderData.ProjectedShadows.Reset();
	RenderData.ShadowAtlasSliceCount = 0;
	if (!World) return;

	FScene& Scene = World->GetScene();
	RenderData.VisibleLightInfos.SetNum(Scene.Lights.Num());

	// Unlit은 조명을 계산하지 않으므로 그림자도 만들지 않는다 (언리얼 EngineShowFlags.Lighting).
	if (Options.LightingModel == ELightingModel::Unlit) return;
	// r.ShadowQuality 0: 그림자를 끈다
	if (GShadowCVars.ShadowQuality == 0) return;

	for (FLightSceneInfo* LightSceneInfo : Scene.Lights)
	{
		if (!LightSceneInfo || !LightSceneInfo->Proxy) continue;
		const FLightSceneProxy* Proxy = LightSceneInfo->Proxy;

		// CastShadows 꺼짐, Intensity 0, 숨김
		if (!Proxy->CastsDynamicShadow() || !Proxy->IsEnabled() || !Proxy->IsShown(View.bGameView)) continue;

		// 영향 범위가 카메라 프러스텀 밖
		if (!LightSceneInfo->IsDirectional())
		{
			const FSphere Bounds = LightSceneInfo->GetBoundingSphere();
			if (!IntersectSphereFrustum(FVector4(Bounds.Center, Bounds.Radius), View.Frustum.Planes)) { ++RenderData.RenderStats.ShadowLightsSkippedOutsideView; continue; }
		}

		// 영향 범위 안에 화면에 보이는 Receiver가 없음
		if (!HasVisibleReceiverInBounds(LightSceneInfo)) { ++RenderData.RenderStats.ShadowLightsSkippedNoReceiver; continue; }

		if (Proxy->GetNumViewDependentWholeSceneShadows(View) > 0)
			AddViewDependentWholeSceneShadowsForView(LightSceneInfo);   // Directional CSM
		else
			CreateWholeSceneProjectedShadow(LightSceneInfo);            // Spot 1장, Point 6장
	}

	GatherShadowPrimitives();
	AllocateShadowDepthTargets();
}

void FSceneRenderer::GatherShadowPrimitives()
{
	FScene& Scene = World->GetScene();
	for (FVisibleLightInfo& VisibleLightInfo : RenderData.VisibleLightInfos)
	{
		const bool bHadShadows = !VisibleLightInfo.AllProjectedShadows.IsEmpty();
		for (FProjectedShadowInfo* Shadow : VisibleLightInfo.AllProjectedShadows)
			AddInteractingPrimitives(Scene, *Shadow);

		// 그릴 Caster가 없는 그림자는 버린다 (Point는 면 단위로 빠질 수 있다).
		auto RemoveShadowsWithoutSubject = [](TArray<FProjectedShadowInfo*>& Shadows)
		{
			TArray<FProjectedShadowInfo*> Kept;
			for (FProjectedShadowInfo* Shadow : Shadows)
				if (!Shadow->DynamicSubjectPrimitives.IsEmpty()) Kept.Add(Shadow);
			Shadows = std::move(Kept);
		};
		RemoveShadowsWithoutSubject(VisibleLightInfo.AllProjectedShadows);
		RemoveShadowsWithoutSubject(VisibleLightInfo.ShadowsToProject);
		if (bHadShadows && VisibleLightInfo.AllProjectedShadows.IsEmpty()) ++RenderData.RenderStats.ShadowLightsSkippedNoCaster;
	}
}

void FSceneRenderer::AddInteractingPrimitives(FScene& Scene, FProjectedShadowInfo& Shadow)
{
	const FFrustumPlanes& Frustum = Shadow.CasterFrustum;
	const float RadiusThresholdSquared = GShadowCVars.RadiusThreshold * GShadowCVars.RadiusThreshold;

	// 그림자 카메라 프러스텀 안의 물체만 BVH에서 꺼낸다.
	Scene.BVH.QueryCull(FrustumAllPlanesMask,
		[&Frustum](const FBox& Bounds, uint32& Mask)
		{
			return static_cast<EBVHCullResult>(ClassifyBoxInFrustum(Bounds, Frustum, Mask));
		},
		[&](FPrimitiveSceneProxy* Proxy)
		{
			if (!Proxy->bCastsShadow || !Proxy->IsShown(View.bGameView)) return;
			// 메시 종류와 상관없이 그릴 버퍼와 섹션이 있으면 Caster다 (스켈레탈은 스키닝 결과 버퍼)
			if (!Proxy->GetRenderState() || !Proxy->GetVertexBuffer(0)) return;

			// 카메라에서 너무 작게 보이는 Caster는 그림자도 눈에 띄지 않는다 (r.Shadow.RadiusThreshold).
			const FAABB& Bounds = Proxy->GetBounds();
			const float RadiusSquared = Bounds.Extent.SizeSquared();
			const float DistanceSquared = (Bounds.Center - View.ViewLocation).SizeSquared();
			if (RadiusSquared < RadiusThresholdSquared * DistanceSquared) { ++RenderData.RenderStats.ShadowCastersSkippedSmall; return; }

			Shadow.AddSubjectPrimitive(Proxy);
		});
}

void FSceneRenderer::AllocateShadowDepthTargets()
{
	// 큰 타일부터 넣어야 QuadTree가 덜 쪼개진다. 같은 크기끼리는 라이트 Id·캐스케이드/면 순서를 유지해 프레임마다 배치가 같게 한다.
	TArray<FProjectedShadowInfo*> Shadows;
	for (FVisibleLightInfo& VisibleLightInfo : RenderData.VisibleLightInfos)
		for (FProjectedShadowInfo* Shadow : VisibleLightInfo.ShadowsToProject)
			Shadows.Add(Shadow);
	std::stable_sort(Shadows.begin(), Shadows.end(),
		[](const FProjectedShadowInfo* A, const FProjectedShadowInfo* B) { return A->GetTileSize() > B->GetTileSize(); });

	TArray<FQuadTree> AtlasSlices;
	for (FProjectedShadowInfo* Shadow : Shadows)
	{
		const uint32 TileSize = Shadow->GetTileSize();
		FRect Rect{};
		bool bPlaced = false;
		for (int32 Slice = 0; Slice < AtlasSlices.Num() && !bPlaced; ++Slice)
		{
			if (AtlasSlices[Slice].InsertSquare(static_cast<uint16>(TileSize), Rect))
			{
				Shadow->AtlasIndex = static_cast<uint32>(Slice);
				bPlaced = true;
			}
		}
		if (!bPlaced && AtlasSlices.Num() < static_cast<int32>(MaxShadowAtlasSlices))
		{
			AtlasSlices.Add(FQuadTree(ShadowAtlasSize));
			if (AtlasSlices.Last().InsertSquare(static_cast<uint16>(TileSize), Rect))
			{
				Shadow->AtlasIndex = static_cast<uint32>(AtlasSlices.Num() - 1);
				bPlaced = true;
			}
		}
		// 아틀라스가 꽉 차면 이 그림자는 bAllocated = false로 남고 그리지 않는다 (언리얼과 같음).
		if (!bPlaced) continue;

		Shadow->X = static_cast<uint32>(Rect.X) + Shadow->BorderSize;
		Shadow->Y = static_cast<uint32>(Rect.Y) + Shadow->BorderSize;
		Shadow->bAllocated = true;
	}
	RenderData.ShadowAtlasSliceCount = static_cast<uint32>(AtlasSlices.Num());
}

void FSceneRenderer::RenderShadowDepthMaps(FRenderer* Renderer, FTexture2DArray* Atlas)
{
	FConstantBuffer* PassCB = RenderData.ViewLightResources.GetShadowDepthPassCB();
	if (!bCorePassSucceeded || !Atlas || !PassCB) return;

	for (const TUniquePtr<FProjectedShadowInfo>& ShadowPtr : RenderData.ProjectedShadows)
	{
		const FProjectedShadowInfo& Shadow = *ShadowPtr;
		if (!Shadow.bAllocated || Shadow.DynamicSubjectPrimitives.IsEmpty()) continue;

		TArray<FMeshDrawBatch> Batches;
		FMeshPassProcessor Processor;
		// World 이동에 PreShadowTranslation을 미리 더해 그림자 원점 기준 좌표로 그린다 (원점에서 먼 곳의 float 정밀도).
		if (!Processor.Build(Shadow.DynamicSubjectPrimitives, EPSOType::ShadowDepth, Batches, Shadow.PreShadowTranslation)) continue;

		// 테두리(BorderSize)는 Clear 값 1을 유지하도록 그리는 영역만 뷰포트로 잡는다.
		RenderCommand::SetRenderTargets(0, nullptr, Atlas, Shadow.AtlasIndex);
		RenderCommand::SetViewport(Shadow.X, Shadow.Y, Shadow.ResolutionX, Shadow.ResolutionY);

		FShadowDepthPassConstants Constants{};
		Constants.ShadowParams = FVector4(Shadow.ShaderDepthBias, Shadow.ShaderSlopeDepthBias, Shadow.ShaderMaxSlopeDepthBias,
			1.0f / std::max(Shadow.MaxSubjectZ - Shadow.MinSubjectZ, 1.0e-4f));
		Constants.LightDirection = Shadow.GetLightDirection();
		Constants.ShadowNear = Shadow.MinSubjectZ;
		Constants.bPerspective = Shadow.IsPerspective() ? 1u : 0u;
		RenderCommand::UpdateBufferData(PassCB, &Constants, sizeof(Constants));
		RenderCommand::BindConstantBuffer(3, PassCB, EShaderBindFlagBits::Vertex);

		// 위치가 이미 그림자 원점 기준이므로 이동 없는 회전 x 투영만 쓴다.
		FMatrix ShadowRotation = Shadow.ShadowView;
		ShadowRotation.M[3][0] = ShadowRotation.M[3][1] = ShadowRotation.M[3][2] = 0.0f;
		Renderer->UploadViewMatrix(ShadowRotation * Shadow.ShadowProjection);
		Renderer->DrawMeshes(Batches);
		++RenderData.RenderStats.ShadowDepthDraws;   // 단계 9 stat에 추가할 필드
	}
	RenderCommand::BindConstantBuffer(3, nullptr, EShaderBindFlagBits::Vertex);
}

void FSceneRenderer::RenderShadowProjection(const FProjectedShadowInfo& Shadow, uint32 Channel, uint32 Width, uint32 Height)
{
	FConstantBuffer* ProjectionCB = RenderData.ViewLightResources.GetShadowProjectionCB();
	FPipelineState* Pipeline = FRenderResourceManager::GetPSO(EPSOType::ShadowProjection);
	if (!Shadow.bAllocated || !ProjectionCB || !Pipeline || !Pipeline->Shader || !Pipeline->Shader->VertexShader || !Pipeline->Shader->PixelShader)
		return;
	if (Channel > 3 || Width == 0 || Height == 0) return;

	const FLightSceneProxy* Proxy = Shadow.LightSceneInfo->Proxy;
	FLightRenderParameters LightParameters{};
	Proxy->GetLightShaderParameters(LightParameters);

	const float AtlasTexel = 1.0f / static_cast<float>(ShadowAtlasSize);
	const float DepthRange = std::max(Shadow.MaxSubjectZ - Shadow.MinSubjectZ, 1.0e-4f);
	const FShadowCascadeSettings& Cascade = Shadow.CascadeSettings;
	const bool bUseFadePlane = Shadow.IsWholeSceneDirectionalShadow() && Cascade.FadePlaneLength > 0.0f;

	// 카메라 기준(Translated World) 좌표로 계산한다 (언리얼 GetScreenToShadowMatrix와 같은 이유).
	// 원점에서 먼 곳(예: Y 14000)에서 깊이로 월드 좌표를 그대로 복원하면 float 정밀도가 모자라 수신점 깊이가
	// 카메라가 움직일 때마다 흔들리고, 그림자 가장자리가 떨린다. 큰 좌표끼리의 뺄셈은 CPU에서 한 번만 한다.
	FMatrix ViewRotation = View.ViewMatrix;
	ViewRotation.M[3][0] = ViewRotation.M[3][1] = ViewRotation.M[3][2] = 0.0f;
	FMatrix ShadowRotation = Shadow.ShadowView;
	ShadowRotation.M[3][0] = ShadowRotation.M[3][1] = ShadowRotation.M[3][2] = 0.0f;
	// 카메라 기준 위치 + ShadowOffset = 그림자 원점(PreShadowTranslation) 기준 위치
	const FVector ShadowOffset = View.ViewLocation + Shadow.PreShadowTranslation;

	FShadowProjectionConstants Constants{};
	Constants.InvViewProjection = (ViewRotation * View.ProjectionMatrix).Inverse();
	Constants.ShadowViewProjection = FMatrix::MakeTranslation(ShadowOffset) * ShadowRotation * Shadow.ShadowProjection;
	Constants.AtlasRect = FVector4(Shadow.X * AtlasTexel, Shadow.Y * AtlasTexel, Shadow.ResolutionX * AtlasTexel, Shadow.ResolutionY * AtlasTexel);
	Constants.ViewportSize = FVector4(static_cast<float>(Width), static_cast<float>(Height), 1.0f / Width, 1.0f / Height);
	Constants.LightPosition = ShadowOffset * -1.0f;   // 카메라 기준 광원(그림자 원점) 위치
	Constants.ShadowNear = Shadow.MinSubjectZ;
	Constants.LightDirection = Shadow.GetLightDirection();
	Constants.InvDepthRange = 1.0f / DepthRange;
	Constants.ViewOrigin = FVector::ZeroVector;      // 셰이더의 위치가 이미 카메라 기준
	Constants.FadeAlpha = Shadow.FadeAlpha;
	Constants.ViewForward = View.ViewForward;
	Constants.ShadowSharpen = Proxy->GetShadowSharpen() * 7.0f + 1.0f;   // UE와 같은 변환
	Constants.SplitNear = Cascade.SplitNear;
	Constants.SplitFar = Cascade.SplitFar;
	Constants.FadePlaneOffset = Cascade.FadePlaneOffset;
	Constants.InvFadePlaneLength = bUseFadePlane ? 1.0f / Cascade.FadePlaneLength : 0.0f;
	Constants.AtlasIndex = Shadow.AtlasIndex;
	Constants.bPerspective = Shadow.IsPerspective() ? 1u : 0u;
	Constants.bUseFadePlane = bUseFadePlane ? 1u : 0u;
	Constants.PcfQuality = std::clamp(GShadowCVars.ShadowQuality, 1u, 5u);
	// UE처럼 PCSS는 Directional·Spot만. Point 면은 PCF
	Constants.FilterMethod = GShadowCVars.FilterMethod == 1 && !Shadow.IsWholeScenePointLightShadow() ? 1u : 0u;
	Constants.LightSourceSize = LightParameters.SourceSize;
	Constants.MaxSoftKernelTexels = static_cast<float>(std::max(GShadowCVars.MaxSoftKernelSize, 1u));
	// 투영 행렬의 가로 배율(M[1][0])은 Ortho면 2 / 폭, Perspective면 1 / tan(반각). 둘 다 2 / (배율 x 해상도)가 텍셀 하나의 크기다.
	Constants.WorldTexelScale = 2.0f / std::max(std::abs(Shadow.ShadowProjection.M[1][0]) * Shadow.ResolutionX, 1.0e-6f);

	// 캐스케이드는 알파로 섞는다. A 채널은 알파 자체가 그림자 값이라 Fade를 쓸 수 없어 덮어쓴다.
	const bool bFade = bUseFadePlane && Channel < 3;
	FPipelineState State = *Pipeline;
	State.BlendState = static_cast<EBlendState>(static_cast<uint8>(bFade ? EBlendState::ShadowMaskFadeR : EBlendState::ShadowMaskR) + Channel);

	RenderCommand::UpdateBufferData(ProjectionCB, &Constants, sizeof(Constants));
	RenderCommand::BindConstantBuffer(0, ProjectionCB, EShaderBindFlagBits::Pixel);
	RenderCommand::BindSamplerState(2, ESamplerState::PointClamp, EShaderBindFlagBits::Pixel);
	RenderCommand::BindSamplerState(3, ESamplerState::ShadowComparison, EShaderBindFlagBits::Pixel);
	RenderCommand::BindPipelineState(State);
	RenderCommand::BindVertexBuffer(nullptr);
	RenderCommand::Draw(3);
}

void FSceneRenderer::RenderShadowProjectionsForLight(const FVisibleLightInfo& VisibleLightInfo, uint32 Channel, uint32 Width, uint32 Height)
{
	TArray<const FProjectedShadowInfo*> Shadows;
	for (const FProjectedShadowInfo* Shadow : VisibleLightInfo.ShadowsToProject)
		if (Shadow && Shadow->bAllocated) Shadows.Add(Shadow);

	// 먼 캐스케이드(큰 SplitIndex)부터. Spot·Point는 순서와 상관없다.
	std::stable_sort(Shadows.begin(), Shadows.end(), [](const FProjectedShadowInfo* A, const FProjectedShadowInfo* B)
		{
			return A->CascadeSettings.ShadowSplitIndex > B->CascadeSettings.ShadowSplitIndex;
		});
	for (const FProjectedShadowInfo* Shadow : Shadows)
		RenderShadowProjection(*Shadow, Channel, Width, Height);
}

void FSceneRenderer::RenderShadowMaskVisualizationProjections(uint32 Width, uint32 Height)
{
	if (!bCorePassSucceeded || !World) return;
	const FScene& Scene = World->GetScene();

	// Directional을 R에 먼저, 나머지는 광원 순서대로 G, B, A
	TArray<int32> LightIds;
	for (int32 Id = 0; Id < RenderData.VisibleLightInfos.Num(); ++Id)
	{
		if (RenderData.VisibleLightInfos[Id].ShadowsToProject.IsEmpty() || Id >= Scene.Lights.Num() || !Scene.Lights[Id]) continue;
		if (Scene.Lights[Id]->IsDirectional()) LightIds.Insert(Id, 0);
		else LightIds.Add(Id);
	}
	const uint32 Count = std::min<uint32>(static_cast<uint32>(LightIds.Num()), 4u);
	for (uint32 Channel = 0; Channel < Count; ++Channel)
		RenderShadowProjectionsForLight(RenderData.VisibleLightInfos[LightIds[Channel]], Channel, Width, Height);
	RenderCommand::BindConstantBuffer(0, nullptr, EShaderBindFlagBits::Pixel);
}

bool FSceneRenderer::HasVisibleReceiverInBounds(const FLightSceneInfo* LightSceneInfo) const
{
	for (const FPrimitiveSceneProxy* Proxy : RenderData.VisibleProxies)
	{
		if (!Proxy || !Proxy->bReceiveShadow) continue;
		const FAABB& Bounds = Proxy->GetBounds();
		if (LightSceneInfo->AffectsBounds(FSphere(Bounds.Center, Bounds.Extent.Length()))) return true;
	}
	return false;
}

void FSceneRenderer::CreateWholeSceneProjectedShadow(FLightSceneInfo* LightSceneInfo)
{
	const FLightSceneProxy* Proxy = LightSceneInfo->Proxy;

	TArray<FWholeSceneProjectedShadowInitializer> Initializers;
	if (!Proxy->GetWholeSceneProjectedShadowInitializer(View, Initializers)) return;

	float FadeAlpha = 1.0f;
	const uint32 Resolution = ComputeWholeSceneShadowResolution(View, LightSceneInfo->GetBoundingSphere(),
		Proxy->GetShadowResolutionScale(), FadeAlpha);
	if (Resolution == 0) return;   // 화면에서 너무 작음

	FVisibleLightInfo& VisibleLightInfo = RenderData.VisibleLightInfos[LightSceneInfo->Id];
	for (const FWholeSceneProjectedShadowInitializer& Initializer : Initializers)
	{
		FProjectedShadowInfo* Shadow = RenderData.ProjectedShadows.Emplace_GetRef(MakeUnique<FProjectedShadowInfo>()).get();
		Shadow->SetupWholeSceneProjection(LightSceneInfo, Initializer, Resolution - ShadowBorder * 2, Resolution - ShadowBorder * 2, ShadowBorder);
		Shadow->FadeAlpha = FadeAlpha;

		VisibleLightInfo.AllProjectedShadows.Add(Shadow);
		VisibleLightInfo.ShadowsToProject.Add(Shadow);
	}
}

void FSceneRenderer::AddViewDependentWholeSceneShadowsForView(FLightSceneInfo* LightSceneInfo)
{
	const FLightSceneProxy* Proxy = LightSceneInfo->Proxy;
	const uint32 Resolution = ComputeCSMResolution(Proxy->GetShadowResolutionScale());

	FVisibleLightInfo& VisibleLightInfo = RenderData.VisibleLightInfos[LightSceneInfo->Id];
	const uint32 NumCascades = Proxy->GetNumViewDependentWholeSceneShadows(View);
	for (uint32 Cascade = 0; Cascade < NumCascades; ++Cascade)
	{
		FWholeSceneProjectedShadowInitializer Initializer;
		if (!Proxy->GetViewDependentWholeSceneProjectedShadowInitializer(View, static_cast<int32>(Cascade), Initializer)) continue;

		FProjectedShadowInfo* Shadow = RenderData.ProjectedShadows.Emplace_GetRef(MakeUnique<FProjectedShadowInfo>()).get();
		Shadow->SetupWholeSceneProjection(LightSceneInfo, Initializer, Resolution - ShadowBorder * 2, Resolution - ShadowBorder * 2, ShadowBorder);

		VisibleLightInfo.AllProjectedShadows.Add(Shadow);
		VisibleLightInfo.ShadowsToProject.Add(Shadow);
	}
}

void FSceneRenderer::FinalizeShadowLights(const FSceneRenderOptions& Options)
{
	ShadowedLights.Reset();
	RenderData.LightShadows.Reset();
	RenderData.CascadeShadowCount = 0;
	const int32 LightCount = RenderData.VisibleLights.Num();
	RenderData.VisibleLightShadowViews.SetNum(LightCount);
	for (int32 i = 0; i < LightCount; ++i) RenderData.VisibleLightShadowViews[i] = FSceneRenderData::InvalidShadowView;

	const bool bForward = Options.RenderingPath != ERenderingPath::Deferred;
	// Gouraud는 정점에서 조명을 끝내므로 화면 마스크를 광원별로 곱할 수 없다.
	const bool bAllowChannels = !(bForward && Options.LightingModel == ELightingModel::Gouraud);

	// Forward 채널 배정 (언리얼 AssignAvailableShadowMapChannelForLight): 영향 범위가 겹치는 광원끼리만 다른 채널을 받는다.
	struct FAssignedChannel { FSphere Bounds; bool bDirectional; uint32 Channel; };
	TArray<FAssignedChannel> Assigned;
	bool bCascadesFilled = false;
	float UsedTexels = 0.0f;

	for (int32 i = 0; i < LightCount; ++i)
	{
		const FLightSceneProxy* Proxy = i < RenderData.VisibleLightProxies.Num() ? RenderData.VisibleLightProxies[i] : nullptr;
		const FLightSceneInfo* Info = Proxy ? Proxy->GetLightSceneInfo() : nullptr;
		if (!Info || Info->Id < 0 || Info->Id >= RenderData.VisibleLightInfos.Num()) continue;

		// 그림자 맵 목록: Directional은 캐스케이드 순서, Point는 면 순서
		TArray<const FProjectedShadowInfo*> Shadows;
		for (const FProjectedShadowInfo* Shadow : RenderData.VisibleLightInfos[Info->Id].ShadowsToProject)
			if (Shadow && Shadow->bAllocated) Shadows.Add(Shadow);
		if (Shadows.IsEmpty()) continue;
		std::stable_sort(Shadows.begin(), Shadows.end(), [](const FProjectedShadowInfo* A, const FProjectedShadowInfo* B)
			{
				return A->CascadeSettings.ShadowSplitIndex < B->CascadeSettings.ShadowSplitIndex;
			});

		// CPU 미리보기 목록 (Details Shadow Maps, 캐스케이드 경계 선, stat)
		const uint32 FirstView = static_cast<uint32>(RenderData.LightShadows.Num());
		RenderData.VisibleLightShadowViews[i] = FirstView;
		if (i == RenderData.PreviewLightIndex) RenderData.PreviewShadowRange = { FirstView, static_cast<uint32>(Shadows.Num()) };
		const bool bFillCascades = Info->IsDirectional() && !bCascadesFilled;
		for (const FProjectedShadowInfo* Shadow : Shadows)
		{
			FLightShadowRenderData ShadowView{};
			ShadowView.ViewProjection = Shadow->ShadowViewProjection;
			ShadowView.AtlasRect = FRect{ static_cast<float>(Shadow->X), static_cast<float>(Shadow->Y),
				static_cast<float>(Shadow->ResolutionX), static_cast<float>(Shadow->ResolutionY) };
			ShadowView.AtlasIndex = Shadow->AtlasIndex;
			ShadowView.Near = Shadow->MinSubjectZ;
			ShadowView.Far = Shadow->MaxSubjectZ;
			ShadowView.WorldUnitsPerTexel = 2.0f / std::max(std::abs(Shadow->ShadowProjection.M[1][0]) * Shadow->ResolutionX, 1.0e-6f);
			RenderData.LightShadows.Add(ShadowView);
			UsedTexels += static_cast<float>(Shadow->GetTileSize()) * static_cast<float>(Shadow->GetTileSize());

			if (bFillCascades && RenderData.CascadeShadowCount < FSceneRenderData::MaxCascadeShadowCount)
				RenderData.CascadeShadowDistances[RenderData.CascadeShadowCount++] = Shadow->CascadeSettings.SplitFar;
			if (Shadow->IsWholeSceneDirectionalShadow()) ++RenderData.RenderStats.CascadeViewsRedrawn;
			++RenderData.RenderStats.ShadowViewsRedrawn;
		}
		bCascadesFilled |= bFillCascades;

		uint32 Channel = FLightRenderParameters::InvalidShadowMapChannel;
		if (bAllowChannels && !bForward)
		{
			Channel = 0;   // Deferred: 광원마다 마스크를 새로 만드므로 항상 R
		}
		else if (bAllowChannels)
		{
			const FSphere Bounds = Info->GetBoundingSphere();
			uint32 UsedMask = 0;
			for (const FAssignedChannel& Other : Assigned)
				if (Info->IsDirectional() || Other.bDirectional || Info->AffectsBounds(Other.Bounds))
					UsedMask |= 1u << Other.Channel;
			for (uint32 Candidate = 0; Candidate < 4; ++Candidate)
				if (!(UsedMask & (1u << Candidate))) { Channel = Candidate; break; }
			if (Channel != FLightRenderParameters::InvalidShadowMapChannel)
				Assigned.Add({ Bounds, Info->IsDirectional(), Channel });
		}

		RenderData.VisibleLights[i].ShadowMapChannel = Channel;
		if (Channel != FLightRenderParameters::InvalidShadowMapChannel)
			ShadowedLights.Add({ i, Info->Id, Channel });
	}

	// Commit이 올린 광원 데이터에는 채널이 없으므로 다시 올린다.
	if (bLightUploadSucceeded && LightCount > 0)
		bLightUploadSucceeded = RenderData.ViewLightResources.UploadViewLights(RenderData.VisibleLights);

	RenderData.RenderStats.AtlasCapacity = static_cast<float>(ShadowAtlasSize) * ShadowAtlasSize * std::max(RenderData.ShadowAtlasSliceCount, 1u) * 4.0f;
	RenderData.RenderStats.ShadowTiles = UsedTexels * 4.0f;   // R32 = 4바이트
}
