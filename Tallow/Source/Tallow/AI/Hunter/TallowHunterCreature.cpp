#include "AI/Hunter/TallowHunterCreature.h"
#include "AI/Hunter/TallowMimCharacterMovementComponent.h"
#include "AI/Hunter/TallowHunterAIController.h"
#include "AI/Hunter/TallowHunterMotionComponent.h"
#include "AI/Hunter/TallowHunterTraversalComponent.h"
#include "AI/Hunter/TallowMimAnimInstance.h"
#include "AI/Hunter/TallowMimVisualScaleComponent.h"
#include "AI/Traversal/TallowSurfaceTraversalComponent.h"
#include "Animation/TallowCreatureAnimationComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ATallowHunterCreature::ATallowHunterCreature(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UTallowMimCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    AIControllerClass = ATallowHunterAIController::StaticClass();
    HunterMotion = CreateDefaultSubobject<UTallowHunterMotionComponent>(TEXT("HunterMotion"));
    HunterTraversal = CreateDefaultSubobject<UTallowHunterTraversalComponent>(TEXT("HunterTraversal"));
    MimVisual = CreateDefaultSubobject<UTallowMimVisualScaleComponent>(TEXT("MimVisual"));
    // Mim (25 Oct) is huge, but only its body is: collision stays what the navmesh can path (the runtime agent is
    // the engine default, radius 35) and well under the 1.9 m door frames. The body fits itself round the world
    // (HunterMotion's fit layer); the capsule flattens further to squeeze through crawls.
    GetCapsuleComponent()->InitCapsuleSize(34.f, 50.f);
    BaseEyeHeight = 30.f;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 110.f, 0.f);
    GetCharacterMovement()->MaxWalkSpeed = 115.f;
    GetCharacterMovement()->MaxAcceleration = 230.f;
    GetCharacterMovement()->BrakingDecelerationWalking = 350.f;
    // Keep its existing capture/duration/reset interface. Ordinary playback
    // tick is disabled after initialization so HunterMotion owns locomotion.
    KeeperAnimation->bEnableAuthoredPlayback = true;
    SurfaceTraversal->bAutomaticTraversal = false;
    SurfaceTraversal->bAutomaticSurfaceRoute = false;
    SurfaceTraversal->bChaseSurfaceRoutes = false;
    SurfaceTraversal->PrimaryComponentTick.bStartWithTickEnabled = false;
}

void ATallowHunterCreature::BeginPlay()
{
    const FString Rig = FPlatformMisc::GetEnvironmentVariable(TEXT("TALLOW_KEEPER"));
    if (!UTallowHunterMotionComponent::IsProxyMode() && (Rig.IsEmpty() || Rig.Equals(TEXT("MIM"), ESearchCase::IgnoreCase)))
    {
        KeeperAnimation->bEnableAuthoredPlayback = false;
        const FVector Foot = GetActorLocation() - GetActorUpVector() * GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        SetActorScale3D(GetActorScale3D() * FMath::Clamp(MimBodyScale, .8f, 1.f));
        SetActorLocation(Foot + GetActorUpVector() * GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), false, nullptr, ETeleportType::TeleportPhysics);
    }
    Super::BeginPlay();
    KeeperAnimation->SetComponentTickEnabled(false);
    SurfaceTraversal->SetComponentTickEnabled(false);
    HunterMotion->InitializeMesh();
}

void ATallowHunterCreature::PrepareCapturePresentation()
{
    if (HunterMotion && HunterMotion->UsesMim()) HunterMotion->BeginMimCapture();
    else Super::PrepareCapturePresentation();
}

float ATallowHunterCreature::GetCapturePresentationDuration() const
{
    // 28 Oct: no slow fade after the strike. The shot ends on a hard cut while the face
    // and hands are still on the lens; the game mode cuts to black at the end of this.
    return HunterMotion && HunterMotion->UsesMim() ? GetCaptureStrikeTime() + CaptureCutAfterStrike
        : Super::GetCapturePresentationDuration();
}

float ATallowHunterCreature::GetCaptureStrikeTime() const
{
    return HunterMotion && HunterMotion->UsesMim() ? HunterMotion->GetMimCaptureStrikeTime()
        : Super::GetCaptureStrikeTime();
}

float ATallowHunterCreature::GetCaptureStagingDistance() const
{
    // The floor and dark grabs are authored with the root 2.25 rig metres in front of the eye, which puts her
    // teeth 1.5 rig metres from it in the stare; the ceiling drop keeps its original staging. Both scale with the
    // body she is drawn at, so the shot keeps its framing at the lens.
    if (!HunterMotion || !HunterMotion->UsesMim()) return Super::GetCaptureStagingDistance();
    return (IsCeilingCapturePresentation() ? 223.f : 225.f) * GetVisibleBodyScale();
}

float ATallowHunterCreature::GetVisibleBodyScale() const
{
    return GetActorScale3D().GetAbsMin() * (MimVisual ? MimVisual->GetVisualScale() : 1.f);
}

bool ATallowHunterCreature::IsCeilingCapturePresentation() const
{
    return HunterMotion && HunterMotion->ShouldKeepMimCaptureRoot();
}

bool ATallowHunterCreature::UsesCanonicalCaptureBasis() const
{
    const auto* Instance = Cast<UTallowMimAnimInstance>(GetMesh()->GetAnimInstance());
    return Instance && Instance->HasAdaptiveFitLayer();
}

void ATallowHunterCreature::GetActorEyesViewPoint(FVector& Location, FRotator& Rotation) const
{
    if (HunterMotion && HunterMotion->HasInitialized())
    {
        Location = HunterMotion->GetEyeLocation();
        Rotation = HunterMotion->GetLookDirection().Rotation();
        return;
    }
    Super::GetActorEyesViewPoint(Location, Rotation);
}
