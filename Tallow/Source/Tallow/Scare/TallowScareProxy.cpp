#include "Scare/TallowScareProxy.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

ATallowScareProxy::ATallowScareProxy()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
    Pivot->SetMobility(EComponentMobility::Movable);
    SetRootComponent(Pivot);
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(Pivot);
    Mesh->SetMobility(EComponentMobility::Movable);
    // Only seen: nothing walks into it, it pushes nothing, and the navigation never notices it.
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCanEverAffectNavigation(false);
}
void ATallowScareProxy::CopyLook(const UStaticMeshComponent* Source)
{
    if (!Source) return;
    Mesh->SetStaticMesh(Source->GetStaticMesh());
    for (int32 I = 0; I < Source->GetNumMaterials(); ++I) Mesh->SetMaterial(I, Source->GetMaterial(I));
    Mesh->SetCastShadow(Source->CastShadow);
    Mesh->SetWorldTransform(Source->GetComponentTransform());
}
void ATallowScareProxy::Swing(const FVector& Nail, const FVector& InAxis, float InDegrees, float SettleDegrees, float InSeconds)
{
    // Hang the copy from the nail: the pivot moves to the nail and the picture keeps its place in the world.
    const FTransform Picture = Mesh->GetComponentTransform();
    SetActorLocationAndRotation(Nail, FQuat::Identity);
    Mesh->SetWorldTransform(Picture);
    Rest = FQuat::Identity; Axis = InAxis.GetSafeNormal(); Degrees = InDegrees; Settle = SettleDegrees; Seconds = FMath::Max(InSeconds, .1f); Age = 0.f;
    Motion = EMotion::Swing; SetActorTickEnabled(true);
}
void ATallowScareProxy::Run(const FVector& InFrom, const FVector& InTo, float InSeconds, float FloorToPivot)
{
    From = InFrom; To = InTo; Seconds = FMath::Max(InSeconds, .1f); Age = 0.f; Lift = FloorToPivot; FloorZ = InFrom.Z;
    const FRotator Heading(0.f, (To - From).Rotation().Yaw, 0.f);
    Mesh->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
    SetActorLocationAndRotation(From + FVector(0.f, 0.f, Lift), Heading);
    Motion = EMotion::Run; SetActorTickEnabled(true);
}
void ATallowScareProxy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Age += DeltaSeconds;
    const float T = FMath::Clamp(Age / Seconds, 0.f, 1.f);
    if (Motion == EMotion::Swing)
    {
        // A knock sets it swinging; it dies away and is left hanging off true.
        const float Angle = Settle * FMath::SmoothStep(0.f, 1.f, FMath::Min(1.f, T * 2.5f))
            + Degrees * FMath::Exp(-3.2f * T) * FMath::Sin(Age * 2.f * PI * .85f) * (1.f - T);
        SetActorRotation(FQuat(Axis, FMath::DegreesToRadians(Angle)) * Rest);
    }
    else if (Motion == EMotion::Run)
    {
        // The spring lets go at once and runs down: quick away, then slower and slower until it stops.
        const float Along = 1.f - FMath::Square(1.f - T) * (1.f - .35f * T);
        FVector At = FMath::Lerp(From, To, FMath::Clamp(Along, 0.f, 1.f));
        FHitResult Floor;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ScareProxyFloor), false, this);
        if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0)) Query.AddIgnoredActor(Pawn);
        if (GetWorld()->LineTraceSingleByChannel(Floor, At + FVector(0.f, 0.f, 30.f), At - FVector(0.f, 0.f, 40.f), ECC_Visibility, Query) && Floor.ImpactNormal.Z > .9f)
            FloorZ = Floor.ImpactPoint.Z;
        At.Z = FloorZ + Lift;
        SetActorLocation(At);
    }
    if (Age >= Seconds) SetActorTickEnabled(false);
}
