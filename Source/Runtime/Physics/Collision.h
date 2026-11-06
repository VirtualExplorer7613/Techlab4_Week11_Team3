#pragma once

class UShapeComponent;
class USphereComponent;
class UCapsuleComponent;
class UBoxComponent;

struct FWorldSphere { FVector Center; float Radius; };
struct FWorldCapsule { FVector A; FVector B; float Radius; }; // A·B = 위·아래 반구 중심
struct FWorldBox { FVector Center; FVector Axis[3]; FVector Extent; };  // Axis는 정규화된 회전축
 
// 도형 간 교차 판정. 담당: D, 소비자: C (UWorld::UpdateOverlaps)
namespace FCollision
{
	FWorldSphere ToWorld(const USphereComponent* S);
	FWorldCapsule ToWorld(const UCapsuleComponent* C);
	FWorldBox ToWorld(const UBoxComponent* B);

	bool SphereSphere(const FWorldSphere& A, const FWorldSphere& B);
	bool SphereBox(const FWorldSphere& S, const FWorldBox& B);
	bool CapsuleSphere(const FWorldCapsule& C, const FWorldSphere& S);
	bool CapsuleCapsule(const FWorldCapsule& A, const FWorldCapsule& B);
	bool BoxBox(const FWorldBox& A, const FWorldBox& B);
	bool CapsuleBox(const FWorldCapsule& C, const FWorldBox& B);

	// 두 도형이 겹치면 true를 반환
	bool Overlap(const UShapeComponent* A, const UShapeComponent* B);

	// 테스트 코드 : 실패한 케이스 수를 돌려준다
	int32 RunSelfTest();
}
