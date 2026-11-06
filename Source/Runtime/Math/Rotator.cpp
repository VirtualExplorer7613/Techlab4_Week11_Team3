#include "EnginePCH.h"
#include "Rotator.h"
#include "Math/EngineMath.h"

FRotator::FRotator()
{
	Pitch = 0; Yaw = 0; Roll = 0;
}

FRotator::FRotator(float P, float Y, float R)
{
	Pitch = P; Yaw = Y; Roll = R;
}

FRotator::FRotator(FVector V)
{
	Pitch = V.X; Yaw = V.Y; Roll = V.Z;
}


FQuat FRotator::Quaternion() const
{
	// Degree를 Radian으로 변환
	float PRad = Pitch * (PI / 180.0f);
	float YRad = Yaw * (PI / 180.0f);
	float RRad = Roll * (PI / 180.0f);

	return FQuat::MakeFromEuler(RRad, PRad, YRad);
}

FRotator FRotator::Identitiy = FRotator(0, 0, 0);

void FRotator::LookDirection(const FVector& Direction)
{
	const float HorizontalSpeed =
		std::sqrt(Direction.X * Direction.X + Direction.Y * Direction.Y);

	Pitch = FMath::RadiansToDegrees(std::atan2(-Direction.Z, HorizontalSpeed));
	Yaw = FMath::RadiansToDegrees(std::atan2(Direction.Y, Direction.X));
	Roll = 0.0f;
}

FRotator operator+(FRotator Rot, const FVector& Vec)
{
	Rot.Roll += Vec.X;
	Rot.Pitch += Vec.Y;
	Rot.Yaw += Vec.Z;

	return Rot;
}