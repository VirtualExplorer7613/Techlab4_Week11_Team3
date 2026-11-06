#pragma once

enum class ERasterizerState : uint8 { SolidBack, SolidNone, SolidFront, Wireframe, LightShadow, DirectionalLightShadow, Decal, Count };
enum class EDepthStencilState : uint8 { Default, ReadOnly, Disabled, StencilMask, StencilOutline, PrepassReadOnly, AlwaysWrite, Count };
// ShadowMask*: 그림자 마스크의 한 채널만 쓴다 (R, G, B, A 순서 유지). Fade는 알파로 섞는다 (CSM 캐스케이드 경계).
enum class EBlendState : uint8 { Opaque, AlphaBlend, NoColorWrite, Additive,
	ShadowMaskR, ShadowMaskG, ShadowMaskB, ShadowMaskA, ShadowMaskFadeR, ShadowMaskFadeG, ShadowMaskFadeB, ShadowMaskFadeA, Count };
enum class ESamplerState : uint8 { LinearClamp, LinearWrap, PointClamp, ShadowComparison, LinearWrapUClampV, Count };
