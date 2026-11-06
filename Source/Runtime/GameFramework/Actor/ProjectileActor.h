#pragma once

#include "GameFramework/Actor.h"
#include "ProjectileActor.generated.h"

class UStaticMeshComponent;
class UFireBallComponent;
class UProjectileMovementComponent;

// Reusable PIE projectile launched by the default pawn.
UCLASS()
class AProjectileActor : public AActor
{
    GENERATED_BODY()
public:
    AProjectileActor();
    void Tick(float DeltaTime) override;
    void Launch(const FVector& Position, const FVector& Direction, bool bUseStaticMesh = false);
    void ReturnToPool();
    bool IsInFlight() const { return bInFlight; }
    UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }
    UStaticMeshComponent* GetFireBallMeshComponent() const { return FireBallMeshComponent; }
    UFireBallComponent* GetFireBallComponent() const { return FireBallComponent; }
    UProjectileMovementComponent* GetProjectileMovementComponent() const { return ProjectileMovementComponent; }
private:
    UStaticMeshComponent* StaticMeshComponent = nullptr;
    UStaticMeshComponent* FireBallMeshComponent = nullptr;
    UFireBallComponent* FireBallComponent = nullptr;
    UProjectileMovementComponent* ProjectileMovementComponent = nullptr;
    bool bInFlight = false;
    float RemainingLife = 0.0f;
    float LifeTime = 5.0f;
};
