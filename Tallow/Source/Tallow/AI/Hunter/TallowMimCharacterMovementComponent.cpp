#include "AI/Hunter/TallowMimCharacterMovementComponent.h"
#include "AI/Hunter/TallowHunterCreature.h"
#include "AI/Hunter/TallowHunterAIController.h"
#include "AI/Hunter/TallowHunterMotionComponent.h"
#include "AI/Hunter/TallowHunterTraversalComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

/**
 * Physical executor for ordinary ground movement. It MEASURES the move through the
 * movement contract every step (so logs/tests always see torso/head clearance), but it
 * never vetoes the move: the root capsule is the hard body and Super (CharacterMovement)
 * owns sweeping, sliding and stepping up. Torso/head brushes are counted as diagnostics
 * for the planner/fit layer, which respond by folding (Compact posture, fit speeds) --
 * vetoing here with a zero-time hit used to freeze her mid-slide whenever the animated
 * pose brushed furniture, which in this house is most frames.
 */
bool UTallowMimCharacterMovementComponent::MoveUpdatedComponentImpl(const FVector& Delta,
    const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, ETeleportType Teleport)
{
    auto* Hunter = Cast<ATallowHunterCreature>(CharacterOwner);
    const auto* Brain = Hunter ? Cast<ATallowHunterAIController>(Hunter->GetController()) : nullptr;
    const bool bMeasureContract = Hunter && Hunter->HunterMotion && Hunter->HunterMotion->UsesMim()
        && bSweep && Teleport == ETeleportType::None && IsMovingOnGround()
        && Hunter->GetActorUpVector().Z > .95f
        && (!Hunter->HunterTraversal || !Hunter->HunterTraversal->IsTraversing())
        && (!Brain || (Brain->bUseMovementContract && !Brain->bRecoveryFitAssist
            && Brain->HuntMode != ETallowHuntMode::Captured));
    if (bMeasureContract && (!Delta.IsNearlyZero() || !NewRotation.Equals(UpdatedComponent->GetComponentQuat(), .0001f)))
    {
        ++ContractChecks;
        const auto* Capsule = Hunter->GetCapsuleComponent();
        FTallowMimConfiguration From = UTallowMimMovementContract::GetConfiguration(Hunter), To = From;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(MimMovementExecution));
        FCollisionResponseParams Responses;
        UTallowMimMovementContract::BuildCollisionPolicy(Capsule, Query, Responses);
        FHitResult CapsuleHit;
        const bool bCapsuleHit = GetWorld()->SweepSingleByChannel(CapsuleHit, From.Position,
            From.Position + Delta, NewRotation, Capsule->GetCollisionObjectType(),
            FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query, Responses);
        // Measure the body along the capsule's executable fraction; Super below still
        // receives its real hit and performs StepUp/Slide on the remainder.
        const float Fraction = bCapsuleHit && !CapsuleHit.bStartPenetrating
            ? FMath::Clamp(CapsuleHit.Time - .001f, 0.f, 1.f) : 1.f;
        To.Position += Delta * Fraction;
        To.Rotation = NewRotation.Rotator();
        LastContractResult = UTallowMimMovementContract::ValidateTransition(Hunter, From, To, false);
        if (UTallowMimMovementContract::IsTorsoBrush(LastContractResult)) ++ContractRejections;
        LastActualCapsuleBlocked = false;
    }
    FHitResult LocalHit;
    FHitResult* Hit = OutHit ? OutHit : &LocalHit;
    const bool bMoved = Super::MoveUpdatedComponentImpl(Delta, NewRotation, bSweep, Hit, Teleport);
    LastActualCapsuleBlocked = Hit->bBlockingHit;
    return bMoved;
}
