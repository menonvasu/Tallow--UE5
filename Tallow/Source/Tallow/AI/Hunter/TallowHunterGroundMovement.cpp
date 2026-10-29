#include "AI/Hunter/TallowHunterAIController.h"
#include "AI/Hunter/TallowHunterCreature.h"
#include "AI/Hunter/TallowHunterMotionComponent.h"
#include "AI/Hunter/TallowHunterTraversalComponent.h"
#include "AI/Hunter/TallowMimMovementContract.h"
#include "AI/Hunter/TallowMimCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Engine/World.h"

/**
 * Ordinary ground travel under the movement contract, in one readable flow:
 *
 *   1. Find a complete nav path to the goal and pick the next corner (Ahead).
 *   2. Pick a posture for the space: Compact when the fit layer measures a low or
 *      narrow opening ahead, Normal again only once well clear of it.
 *   3. Measure a short step toward Ahead through the contract.
 *   4. Issue the move. The measurement never vetoes it (travel policy in
 *      TallowMimMovementContract.h); capsule-blocked results start the blocked
 *      clock that hands over to the fit assist if they sustain.
 *
 * Speed itself is owned by UTallowHunterMotionComponent (fit speeds, rhythm);
 * physical execution by UTallowMimCharacterMovementComponent.
 */
namespace
{
/** Fit-clearance (world cm, floor->ceiling / wall->wall) below which she folds to
 *  Compact before she arrives, instead of discovering the squeeze by blocking. */
constexpr float CompactEntryClearance = 125.f;
/** Hysteresis: only unfold again once this clear, so doorways do not flap posture. */
constexpr float NormalRestoreClearance = 160.f;

bool ClearanceWantsCompact(const ATallowHunterCreature* Creature)
{
    if (!Creature || !Creature->HunterMotion) return false;
    const FVector Space = Creature->HunterMotion->GetFitClearance();
    return Space.Y < CompactEntryClearance || Space.Z < CompactEntryClearance;
}

bool ClearanceAllowsNormal(const ATallowHunterCreature* Creature)
{
    if (!Creature || !Creature->HunterMotion) return true;
    const FVector Space = Creature->HunterMotion->GetFitClearance();
    return Space.Y >= NormalRestoreClearance && Space.Z >= NormalRestoreClearance;
}
}

bool ATallowHunterAIController::ReviewContractMoveTo(const FVector& Destination)
{
    if (!Creature || Destination.ContainsNaN()) return false;
    SetHuntEnabled(false); StopMovement();
    bUseMovementContract = true; bContractReview = true; ContractReviewReached = false;
    bAllowRecoveryFitAssist = false; bRecoveryFitAssist = false;
    bRecoveryWaiting = bRecoveryDetour = bRecoveryTraversal = bRecoveryCheckedTranslation = false;
    PendingLink = INDEX_NONE; bLinkFinalGoal = false;
    bAllowCapture = false;
    const TGuardValue<bool> Internal(bRecoveryGoalRequest, true);
    if (!SetGoal(Destination)) { MovementContractStatus = TEXT("review path unavailable"); return false; }
    HuntMode = ETallowHuntMode::Pursue; ContractBlockedSince = -1.f;
    ProgressAnchor = Creature->GetActorLocation(); ProgressAt = GetWorld()->GetTimeSeconds();
    MovementContractStatus = TEXT("review following validated ordinary movement");
    return true;
}

void ATallowHunterAIController::UpdateContractMovement(float Now)
{
    if (!Creature || !bHasGoal || AttackStartedAt >= 0.f || Creature->HunterTraversal->IsTraversing()) return;
    const FVector Here = Creature->GetActorLocation();
    // Progress watch runs in every mode (also under the fit assist below, which
    // used to be able to hold her in one pocket forever).
    if (!bContractReview && (ContractProgressAt < 0.f || FVector::Dist2D(Here, ContractProgressAnchor) > 25.f))
    {
        ContractProgressAnchor = Here; ContractProgressAt = Now;
    }
    if (bRecoveryFitAssist)
    {
        // Approved fallback: compact capsule + ordinary swept path following own
        // collision; visible body overlap must not cancel physical movement.
        // If even that makes no progress, hand back to the contract planner (the
        // capsule is already compact-sized) so the wedge escape can run.
        if (!bContractReview && ContractProgressAt > 0.f && Now - ContractProgressAt > 2.5f)
        {
            bRecoveryFitAssist = false; RecoveryAssistStalledSince = -1.f;
            MovementContractStatus = TEXT("fit assist made no progress; contract planner resumes");
        }
        else
        {
            MovementContractStatus = TEXT("temporary compact capsule recovery; visual fit is diagnostic");
            if (Now >= NextMoveRequest)
            {
                MoveToLocation(Goal, 4.f, false, true, false, true, nullptr, false);
                NextMoveRequest = Now + .3f;
            }
            return;
        }
    }

    // Surface escape, shared by the failure paths below: when the floor route
    // cannot be used, climb instead (my stair case: she could see the player
    // above her and squeezed at the flight instead of going up the wall).
    auto TrySurfaceRoute = [&]() -> bool
    {
        if (!Creature->HunterTraversal || Creature->HunterTraversal->IsTraversing() || Now < NextSurfaceTry) return false;
        if (FVector::Dist(Here, Goal) < 250.f) return false;
        NextSurfaceTry = Now + 4.f;
        if (!Creature->HunterTraversal->StartChaseSurfaceRoute(Goal)) return false;
        StopMovement();
        MovementContractStatus = TEXT("ground route blocked; climbing instead");
        LastDecision = FName(*MovementContractStatus);
        return true;
    };

    // 1. Route: complete, permitted path and its next corner.
    const auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(this, Here, Goal, this);
    if (!Path || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.Num() < 2 || PathCrossesShut(Path->PathPoints))
    {
        // She may be standing off the mesh (capture staging teleports her, or a
        // low ceiling the nav agent does not cover). Walk back onto the mesh
        // instead of stopping forever, and climb if even that is unavailable.
        bool bExtracting = false;
        if (const auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld()))
        {
            FNavLocation ProjectedLocation;
            if (Nav->ProjectPointToNavigation(Here, ProjectedLocation, FVector(150.f, 150.f, 250.f))
                && FVector::Dist(Here, ProjectedLocation.Location) > 25.f && Now >= NextMoveRequest)
            {
                MoveToLocation(ProjectedLocation.Location, 4.f, false, true, false, true, nullptr, false);
                NextMoveRequest = Now + .3f;
                MovementContractStatus = TEXT("navigation: off mesh; walking back onto it");
                bExtracting = true;
            }
        }
        if (!bExtracting && TrySurfaceRoute()) return;
        if (!bExtracting) { StopMovement(); MovementContractStatus = TEXT("navigation: no complete permitted path"); }
        return;
    }
    FVector Ahead = Goal;
    for (const FVector& Point : Path->PathPoints)
        if (FVector::Dist2D(Here, Point) > 12.f) { Ahead = Point; break; }
    const float DesiredYaw = (Ahead - Here).Rotation().Yaw;
    const float Turn = FMath::Clamp(FMath::FindDeltaAngleDegrees(Creature->GetActorRotation().Yaw, DesiredYaw),
        -110.f * GetWorld()->GetDeltaSeconds(), 110.f * GetWorld()->GetDeltaSeconds());
    const float NextYaw = Creature->GetActorRotation().Yaw + Turn;
    const float Travel = FMath::Clamp(FMath::Max(12.f,
        float(Creature->GetCharacterMovement()->MaxWalkSpeed * GetWorld()->GetDeltaSeconds() + 2.f)), 12.f, 32.f);

    // 2. Posture: restore Normal only well clear of the opening that folded her;
    //    fold to Compact early when the measured opening ahead is low/narrow.
    auto Current = UTallowMimMovementContract::GetConfiguration(Creature);
    if (Current.Posture == ETallowMimPosture::Compact && Now >= NextContractRestoreTry
        && FVector::Dist2D(Here, ContractCompactAt) > 100.f && ClearanceAllowsNormal(Creature))
    {
        NextContractRestoreTry = Now + .5f;
        const auto Restore = UTallowMimMovementContract::TryChangePosture(Creature, ETallowMimPosture::Normal);
        if (Restore.bValid) Current = UTallowMimMovementContract::GetConfiguration(Creature);
    }
    auto TryFoldToCompact = [&]() -> bool
    {
        const auto Change = UTallowMimMovementContract::TryChangePosture(Creature, ETallowMimPosture::Compact);
        if (!Change.bValid) return false;
        Current = UTallowMimMovementContract::GetConfiguration(Creature);
        ContractCompactAt = Creature->GetActorLocation(); NextContractRestoreTry = Now + 1.f;
        return true;
    };
    if (Current.Posture == ETallowMimPosture::Normal && ClearanceWantsCompact(Creature)) TryFoldToCompact();

    // 3. Measure the next step; if Normal does not fit it, fold and re-measure.
    MovementContractResult = UTallowMimMovementContract::ValidateGroundMove(Creature, Ahead, NextYaw, Current.Posture, Travel);
    if (!MovementContractResult.bValid && Current.Posture == ETallowMimPosture::Normal
        && TryFoldToCompact())
        MovementContractResult = UTallowMimMovementContract::ValidateGroundMove(Creature, Ahead, NextYaw, ETallowMimPosture::Compact, Travel);

    // 4. Travel policy (TallowMimMovementContract.h): capsule and floor results
    //    never veto the move -- CharacterMovement and the navmesh are the physical
    //    authority, and capsule-blocked results just start the blocked clock that
    //    hands over to the fit assist if they sustain. The torso has one narrow
    //    gate, because on clean ground there is no reason to accept a fresh body
    //    collision: from a currently-clean pose, hold and let the gait cycle to a
    //    phase that fits (in the furnished house she usually starts already
    //    brushing, and there walking out folded is the escape, not a freeze).
    bool bHoldForBody = false;
    if (!MovementContractResult.bValid
        && !UTallowMimMovementContract::IsCapsuleBlocked(MovementContractResult)
        && !MovementContractResult.bBodyClear)
    {
        const auto Endpoint = UTallowMimMovementContract::ValidateTransition(Creature, Current, Current, false);
        bHoldForBody = Endpoint.bBodyClear;
    }
    if (UTallowMimMovementContract::IsCapsuleBlocked(MovementContractResult) || bHoldForBody)
    {
        if (ContractBlockedSince < 0.f) ContractBlockedSince = Now;
    }
    else ContractBlockedSince = -1.f;
    // Sustained physical block with a distant goal: climb instead of squeezing.
    if (UTallowMimMovementContract::IsCapsuleBlocked(MovementContractResult)
        && ContractBlockedSince > 0.f && Now - ContractBlockedSince > 2.5f && TrySurfaceRoute()) return;
    // Hold only long enough for the gait to cycle to a fitting phase. If every
    // exit brushes (a long body in a tight landing), brushing through is the
    // escape -- holding forever is a trap. Review mode stays strict.
    if (bHoldForBody && (bContractReview || Now - ContractBlockedSince < .6f))
    {
        StopMovement();
        MovementContractStatus = TEXT("torso ahead blocked; holding a clean pose: ") + MovementContractResult.Reason;
        LastDecision = FName(*MovementContractStatus);
        return;
    }
    MovementContractStatus = MovementContractResult.bValid
        ? Current.Posture == ETallowMimPosture::Normal ? TEXT("normal transition validated") : TEXT("compact transition validated")
        : FString::Printf(TEXT("%s, travelling folded on navmesh: %s"),
            UTallowMimMovementContract::IsCapsuleBlocked(MovementContractResult) ? TEXT("capsule blocked") : TEXT("torso/floor unproven"),
            *MovementContractResult.Reason);
    if (MovementContractResult.bValid || !UTallowMimMovementContract::IsCapsuleBlocked(MovementContractResult))
        LastDecision = FName(*MovementContractStatus);

    // Wedge escape: a validated step can still pocket the capsule (corner point
    // reachable on the navmesh, unreachable for the body). If issued moves make
    // no progress, reverse toward the navmesh behind her, then approach afresh.
    if (!bContractReview)
    {
        if (ContractBacktrackUntil > Now)
        {
            if (FVector::Dist2D(Here, ContractBacktrackTarget) < 30.f)
            {
                ContractBacktrackUntil = -1.f; ContractProgressAnchor = Here; ContractProgressAt = Now;
            }
            else
            {
                if (Now >= NextMoveRequest)
                {
                    MoveToLocation(ContractBacktrackTarget, 4.f, false, true, false, true, nullptr, false);
                    NextMoveRequest = Now + .3f;
                }
                MovementContractStatus = TEXT("wedged; reversing out of the pocket");
                return;
            }
        }
        if (ContractProgressAt < 0.f || FVector::Dist2D(Here, ContractProgressAnchor) > 25.f)
        {
            ContractProgressAnchor = Here; ContractProgressAt = Now;
        }
        else if (Now - ContractProgressAt > 1.5f && Now >= NextContractBacktrackTry)
        {
            const auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
            FNavLocation ProjectedLocation;
            const FVector Retreat = Here - Creature->GetActorForwardVector() * 90.f;
            const bool bProjected = Nav && Nav->ProjectPointToNavigation(Retreat, ProjectedLocation, FVector(60.f, 60.f, 120.f));
            const FVector Projected = bProjected ? ProjectedLocation.Location : FVector::ZeroVector;
            if (bProjected && FVector::Dist2D(Here, Projected) > 25.f)
            {
                ContractBacktrackTarget = Projected; ContractBacktrackUntil = Now + 2.5f;
                NextContractBacktrackTry = Now + 6.f; ContractProgressAt = Now;
                MovementContractStatus = TEXT("wedged; reversing out of the pocket");
                MoveToLocation(ContractBacktrackTarget, 4.f, false, true, false, true, nullptr, false);
                NextMoveRequest = Now + .3f;
                return;
            }
            NextContractBacktrackTry = Now + 2.f;
        }
    }
    if (Now >= NextMoveRequest)
    {
        MoveToLocation(Ahead, 4.f, false, true, false, true, nullptr, false);
        NextMoveRequest = Now + .2f;
    }
}
