#include "EnginePCH.h"
#include "GameFramework/Actor/ProjectileActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/FireBallComponent.h"
#include "Components/ProjectileMovementComponent.h"
#include "Asset/AssetManager.h"
#include "Engine/World.h"

AProjectileActor::AProjectileActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    GetRootComponent()->SetCollisionRadiusOverride(0.5f);
    StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("ProjectileMeshComponent");
    // 이동용 루트는 표시 상태와 분리한다. 숨겨진 메쉬를 부모로 두면 FireBall 자식도 함께 숨겨진다.
    StaticMeshComponent->SetupAttachment(GetRootComponent());
    StaticMeshComponent->SetStaticMesh(UAssetManager::GetAssetByPath<UStaticMesh>("Sphere"));
    StaticMeshComponent->SetRelativeScale3D(FVector(0.5f));
    StaticMeshComponent->SetVisible(false);
    StaticMeshComponent->SetHiddenInGame(true);
    StaticMeshComponent->SetAutoActivate(false);
    FireBallComponent = CreateDefaultSubobject<UFireBallComponent>("FireBallComponent");
    FireBallComponent->SetupAttachment(GetRootComponent());
    FireBallComponent->SetVisibility(false);
    FireBallComponent->SetHiddenInGame(true);
    FireBallComponent->SetAutoActivate(false);
    // FireBall은 깊이 버퍼의 표면에만 색을 입히므로 전용 구체를 함께 움직인다.
    FireBallMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("FireBallMeshComponent");
    FireBallMeshComponent->SetupAttachment(GetRootComponent());
    FireBallMeshComponent->SetStaticMesh(UAssetManager::GetAssetByPath<UStaticMesh>("Sphere"));
    FireBallMeshComponent->SetRelativeScale3D(FVector(FireBallComponent->GetRadius() * 0.5f));
    FireBallMeshComponent->SetVisible(false);
    FireBallMeshComponent->SetHiddenInGame(true);
    FireBallMeshComponent->SetAutoActivate(false);
    ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>("ProjectileMovementComponent");
    ProjectileMovementComponent->SetAutoActivate(false);
    ProjectileMovementComponent->SetUpdatedComponent(GetRootComponent());
    ProjectileMovementComponent->InintialSpeed = 20.0f;
    ProjectileMovementComponent->MaxSpeed = 100.0f;
    ProjectileMovementComponent->ProjectileGravityScale = 1.0f;
    ProjectileMovementComponent->bShouldBounce = true;
    ProjectileMovementComponent->bRotationFollowsVelocity = true;
    ProjectileMovementComponent->bInitialVelocityInLocalSpace = false;
    ProjectileMovementComponent->bSimulationEnabled = false;
    ProjectileMovementComponent->PrimaryComponentTick.bStartWithTickEnabled = false;
}

void AProjectileActor::Launch(const FVector& Position, const FVector& Direction, bool bUseStaticMesh)
{
    if (!GetWorld() || !GetWorld()->IsGameWorld() || Direction.Dot(Direction) <= 0.0001f)
        return;
    ReturnToPool();
    const FVector Forward = Direction.Normalized();
    GetRootComponent()->SetWorldLocation(Position, false);
    if (bUseStaticMesh)
    {
        StaticMeshComponent->SetVisible(true);
        StaticMeshComponent->Activate();
        StaticMeshComponent->SetHiddenInGame(false);
    }
    else
    {
        FireBallMeshComponent->SetRelativeScale3D(FVector(FireBallComponent->GetRadius() * 0.5f));
        FireBallMeshComponent->SetVisible(true);
        FireBallMeshComponent->Activate();
        FireBallMeshComponent->SetHiddenInGame(false);
        FireBallComponent->SetVisibility(true);
        FireBallComponent->Activate();
        FireBallComponent->SetHiddenInGame(false);
    }
    ProjectileMovementComponent->SetUpdatedComponent(GetRootComponent());
    ProjectileMovementComponent->Velocity = Forward * ProjectileMovementComponent->InintialSpeed;
    ProjectileMovementComponent->bSimulationEnabled = true;
    ProjectileMovementComponent->Activate(true);
    RemainingLife = LifeTime;
    bInFlight = true;
    PrimaryActorTick.SetTickFunctionEnable(true);
}

void AProjectileActor::ReturnToPool()
{
    bInFlight = false;
    RemainingLife = 0.0f;
    if (ProjectileMovementComponent)
    {
        ProjectileMovementComponent->Velocity = FVector::ZeroVector;
        ProjectileMovementComponent->bSimulationEnabled = false;
        ProjectileMovementComponent->Deactivate();
    }
    if (StaticMeshComponent)
    {
        StaticMeshComponent->Deactivate();
        StaticMeshComponent->SetHiddenInGame(true);
        StaticMeshComponent->SetVisible(false);
    }
    if (FireBallComponent)
    {
        FireBallComponent->Deactivate();
        FireBallComponent->SetHiddenInGame(true);
        FireBallComponent->SetVisibility(false);
    }
    if (FireBallMeshComponent)
    {
        FireBallMeshComponent->Deactivate();
        FireBallMeshComponent->SetHiddenInGame(true);
        FireBallMeshComponent->SetVisible(false);
    }
    PrimaryActorTick.SetTickFunctionEnable(false);
}

void AProjectileActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!bInFlight) return;
    RemainingLife -= DeltaTime;
    if (RemainingLife <= 0.0f || !ProjectileMovementComponent->bSimulationEnabled) ReturnToPool();
}
