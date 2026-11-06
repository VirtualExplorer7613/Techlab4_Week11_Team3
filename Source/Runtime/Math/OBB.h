#pragma once

#include "Box.h"
#include "Frustum.h"
#include "Vector.h"
#include "Vector4.h"

struct FOBB
{
	FVector Center;
	FVector Axis[3];
	FVector Extent;

	// 로컬 박스를 월드행렬로 옮긴 OBB
	static FOBB FromLocalBox(const FBox& Local, const FMatrix& LocalToWorld)
	{
		FOBB Out;
		const FVector LocalCenter = (Local.Max + Local.Min) * 0.5f;
		const FVector LocalExtent = (Local.Max - Local.Min) * 0.5f;

		const FVector4 C = FVector4(LocalCenter, 1.f) * LocalToWorld;
		Out.Center = FVector{ C.X, C.Y, C.Z };

		for (int32 i = 0; i < 3; ++i)
		{
			const FVector Row{LocalToWorld[i][0], LocalToWorld[i][1], LocalToWorld[i][2]};
			const float Length = Row.Size();
			const float SafeLength = Length != 0 ? Length : 0.001f;
			Out.Extent[i] = LocalExtent[i] * SafeLength;
			Out.Axis[i] = Row / SafeLength;
		}
		return Out;
	}

	// 월드 AABB를 회전 없는 OBB로 본다.
	static FOBB FromAABB(const FAABB& Bounds)
	{
		FOBB Out;
		Out.Center = Bounds.Center;
		Out.Extent = Bounds.Extent;
		Out.Axis[0] = FVector{ 1.f, 0.f, 0.f };
		Out.Axis[1] = FVector{ 0.f, 1.f, 0.f };
		Out.Axis[2] = FVector{ 0.f, 0.f, 1.f };
		return Out;
	}
};

// 축 L 위로 투영한 OBB의 반경
inline float ProjectOBBRadius(const FOBB& Box, const FVector& L)
{
	float Result = 0.f;
	for (int32 i = 0; i < 3; ++i)
	{
		Result += Box.Extent[i] * std::abs(L.Dot(Box.Axis[i]));
	}
	return Result;
}

// SAT: 15개 후보 축 중 하나라도 분리축이면 겹치지 않는다.
inline bool IntersectOBB(const FOBB& A, const FOBB& B)
{
	const FVector T = B.Center - A.Center;

	auto IsSeparated = [&](const FVector& L)
	{
		if (L.Dot(L) < 1e-6f)
		{
			return false;
		}
		const float Distance = std::abs(T.Dot(L));
		const float RadiusA = ProjectOBBRadius(A, L);
		const float RadiusB = ProjectOBBRadius(B, L);
		return Distance > RadiusA + RadiusB;
	};

	for (int32 i = 0; i < 3; ++i)
	{
		if (IsSeparated(A.Axis[i]) || IsSeparated(B.Axis[i]))
		{
			return false;
		}
		for (int32 j = 0; j < 3; ++j)
		{
			if (IsSeparated(A.Axis[i].Cross(B.Axis[j])))
			{
				return false;
			}
		}
	}
	return true;
}