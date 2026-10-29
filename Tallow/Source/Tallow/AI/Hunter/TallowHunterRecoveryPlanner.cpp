#include "AI/Hunter/TallowHunterAIController.h"
#include "AI/Hunter/TallowHunterCreature.h"
#include "AI/Hunter/TallowHunterMotionComponent.h"
#include "AI/Hunter/TallowHunterTraversalComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Environment/TallowDoor.h"
#include "Presentation/TallowRouteGate.h"
#include "Player/TallowCharacter.h"

// One search owns floor, posture/heading and surface alternatives. A surface
// edge is optimistic until its existing complete executor preflight succeeds.
void ATallowHunterAIController::ResetRecoveryPlanner(bool bForgetFailures)
{
    RecoverySearchPoints.Reset(); RecoverySearchNodes.Reset(); RecoverySearchLookup.Reset();
    RecoveryExpanding = RecoveryBestPartial = INDEX_NONE; RecoveryNeighbor = 0;
    bRecoverySearchActive = bRecoverySearchFinished = false;
    if (bForgetFailures) RecoveryFailedEdges.Reset();
    RecoveryPlannerExpansions = RecoveryPlannerEdgeChecks = 0;
    RecoveryPlannerCPUms = RecoveryPlannerSliceMs = RecoveryPlannerWallMs = RecoveryPlannerExecutorMs = 0.f;
    RecoveryPlannerStatus = TEXT("idle");
    RecoveryFailureReasons.Reset(); RecoveryPlannerFailureSummary.Reset();
    if (bForgetFailures) bRecoveryCheckedTranslation = false;
}

FString ATallowHunterAIController::RecoveryConnectionKey(FName Mode, const FVector& From,
    const FVector& To, float Heading) const
{
    const FVector Center = RecoverySearchMass.IsValid ? RecoverySearchMass.GetCenter() : FVector::ZeroVector;
    const float Cell = Mode == TEXT("Retreat") ? 10.f : 60.f;
    return FString::Printf(TEXT("%s:%d,%d,%d>%d,%d,%d:h%d:p%d,%d,%d:c%d,%d,%d:g%u"), *Mode.ToString(),
        FMath::RoundToInt(From.X / Cell), FMath::RoundToInt(From.Y / Cell), FMath::RoundToInt(From.Z / 45.f),
        FMath::RoundToInt(To.X / Cell), FMath::RoundToInt(To.Y / Cell), FMath::RoundToInt(To.Z / 45.f),
        FMath::RoundToInt(Heading / 45.f), FMath::RoundToInt(RecoverySearchPoseSize.X / 12.f),
        FMath::RoundToInt(RecoverySearchPoseSize.Y / 12.f), FMath::RoundToInt(RecoverySearchPoseSize.Z / 12.f),
        FMath::RoundToInt(Center.X / 15.f), FMath::RoundToInt(Center.Y / 15.f),
        FMath::RoundToInt(Center.Z / 15.f), RecoverySceneSignature);
}

void ATallowHunterAIController::RejectRecoveryConnection(const FString& Reason)
{
    if (!ActiveRecoveryKey.IsEmpty()) RecoveryFailedEdges.Add(ActiveRecoveryKey, Reason);
    RecoveryPlannerFailedEdges = RecoveryFailedEdges.Num();
    bRecoverySearchActive = bRecoverySearchFinished = false;
    NoteRecovery(TEXT("planner invalidated executed edge: ") + Reason);
}

bool ATallowHunterAIController::CheckRecoveryFloorEdge(const FVector& From, float FromYaw,
    const FVector& To, float& Seconds, FName& Posture, FString& Reason, bool bKeepHeading)
{
    ++RecoveryPlannerEdgeChecks;
    auto* Motion = Creature->HunterMotion.Get();
    const auto* Capsule = Creature->GetCapsuleComponent();
    const auto* Movement = Creature->GetCharacterMovement();
    const FVector Direction = (To - From).GetSafeNormal2D();
    if (Direction.IsNearlyZero() || !RecoverySearchMass.IsValid) return false;
    const float Yaw = bKeepHeading ? FromYaw : Direction.Rotation().Yaw;
    const FString Key = RecoveryConnectionKey(bKeepHeading ? TEXT("Retreat") : TEXT("Floor"), From, To, Yaw);
    auto Fail = [&](const FString& Why)
    {
        Reason = Why; RecoveryFailedEdges.Add(Key, Why);
        const FString Label = bKeepHeading
            ? FString::Printf(TEXT("retreat%.0f: %s"), Direction.Rotation().Yaw, *Why.Left(110))
            : FString(TEXT("floor: ")) + Why.Left(110);
        ++RecoveryFailureReasons.FindOrAdd(Label);
        return false;
    };
    if (const FString* Cached = RecoveryFailedEdges.Find(Key))
    {
        const FString Why = *Cached;
        return Fail(Why);
    }
    FVector NavHit;
    if ((!bKeepHeading && UNavigationSystemV1::NavigationRaycast(this, From, To, NavHit, DefaultNavigationFilterClass, this))
        || PathCrossesShut(TArray<FVector>{From, To})) return Fail(TEXT("nav/closed-door connection"));
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HunterRecoverySearch), false, Creature);
    Query.AddIgnoredActor(Player); Query.AddIgnoredActors(Capsule->GetMoveIgnoreActors());
    TArray<AActor*> Attached; Creature->GetAttachedActors(Attached); Query.AddIgnoredActors(Attached);
    const float HH = Capsule->GetScaledCapsuleHalfHeight(), Radius = Capsule->GetScaledCapsuleRadius();
    const FVector MassExtent = RecoverySearchMass.GetExtent().ComponentMax(FVector(1.f));
    const FVector MassOffset = RecoverySearchMass.GetCenter();
    TMap<UPrimitiveComponent*, float> PreviousContacts;
    auto MassContacts = [&](const FVector& Floor, float AtYaw, TMap<UPrimitiveComponent*, float>& Contacts)
    {
        const FQuat Rotation(FVector::UpVector, FMath::DegreesToRadians(AtYaw));
        const FVector Center = Floor + FVector::UpVector * (HH + 2.f) + Rotation.RotateVector(MassOffset);
        TArray<FOverlapResult> Hits;
        const auto Box = FCollisionShape::MakeBox(MassExtent);
        GetWorld()->OverlapMultiByChannel(Hits, Center, Rotation, ECC_Pawn, Box, Query);
        for (const auto& Hit : Hits)
        {
            auto* Part = Hit.GetComponent(); FMTDResult MTD;
            if (Hit.bBlockingHit && Part && Part->ComputePenetration(MTD, Box, Center, Rotation))
                Contacts.Add(Part, MTD.Distance);
        }
    };
    MassContacts(From, FromYaw, PreviousContacts);
    const TMap<UPrimitiveComponent*, float> InitialContacts = PreviousContacts;
    FString MassBlock;
    auto MassClear = [&](const FVector& Floor, float AtYaw)
    {
        TMap<UPrimitiveComponent*, float> Contacts;
        MassContacts(Floor, AtYaw, Contacts);
        for (const auto& Contact : Contacts)
        {
            const float* PreviousDepth = PreviousContacts.Find(Contact.Key);
            const float* InitialDepth = InitialContacts.Find(Contact.Key);
            const float Allowed = FMath::Max(2.f, PreviousDepth && InitialDepth
                ? FMath::Min(*PreviousDepth, *InitialDepth) + .1f : 0.f);
            if (Contact.Value > Allowed)
            {
                MassBlock = FString::Printf(TEXT("%s depth%.1f>%.1f"),
                    *GetNameSafe(Contact.Key->GetOwner()), Contact.Value, Allowed);
                return false;
            }
        }
        PreviousContacts = MoveTemp(Contacts);
        return true;
    };
    if (bKeepHeading)
    {
        // This edge exists only at the actual root. The shared Motion checker
        // tests every capsule/body/arm sample without rotating or moving her.
        const FVector Center = To + FVector::UpVector * HH;
        if (FVector::Dist2D(Center, Creature->GetActorLocation()) > 120.1f)
            return Fail(TEXT("escape exceeds local translation bound"));
        FString Why;
        if (!Motion->CanCheckedRecoveryTranslate(Center, Why)) return Fail(Why);
        const int32 Count = FMath::Max(1, FMath::CeilToInt(FVector::Dist(From, To) / 2.f));
        for (int32 I = 1; I <= Count; ++I)
            if (!MassClear(FMath::Lerp(From, To, float(I) / Count), FromYaw))
                return Fail(TEXT("mass envelope ") + MassBlock);
        Seconds = FVector::Dist(From, To) / 60.f + .25f;
        Posture = TEXT("Retreat");
        return true;
    }
    // The current, measured structural torso/head must fit the turn itself.
    // Limbs retain their separate checked IK/plant execution gate.
    const float Turn = FMath::FindDeltaAngleDegrees(FromYaw, Yaw);
    const int32 Turns = FMath::Max(1, FMath::CeilToInt(FMath::Abs(Turn) / 15.f));
    for (int32 I = 1; I <= Turns; ++I)
        if (!MassClear(From, FromYaw + Turn * I / Turns)) return Fail(TEXT("body turn ") + MassBlock);
    const float Length = FVector::Dist(From, To);
    const int32 Samples = FMath::Max(1, FMath::CeilToInt(Length / 35.f));
    FVector Previous = From;
    float Width = 800.f, Height = 350.f;
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Direction);
    for (int32 I = 1; I <= Samples; ++I)
    {
        const FVector Wanted = FMath::Lerp(From, To, float(I) / Samples);
        FHitResult Floor;
        if (!GetWorld()->LineTraceSingleByChannel(Floor, Wanted + FVector::UpVector * (Movement->MaxStepHeight + 12.f),
            Wanted - FVector::UpVector * 120.f, ECC_Pawn, Query) || Floor.ImpactNormal.Z < Movement->GetWalkableFloorZ()
            || FMath::Abs(Floor.ImpactPoint.Z - Previous.Z) > Movement->MaxStepHeight + 2.f)
            return Fail(TEXT("floor/step support"));
        const FVector At = Floor.ImpactPoint;
        const FVector A = Previous + FVector::UpVector * (HH + 2.f), B = At + FVector::UpVector * (HH + 2.f);
        const auto Shape = FCollisionShape::MakeCapsule(Radius, HH);
        if (GetWorld()->SweepTestByProfile(A, B, FQuat::Identity, Capsule->GetCollisionProfileName(), Shape, Query))
        {
            // A real walkable riser may use the same up/forward/down envelope
            // as CharacterMovement. A wall or missing landing still rejects.
            const float Lift = FMath::Max(0.f, At.Z - Previous.Z) + 2.f;
            if (Lift <= 2.f || GetWorld()->SweepTestByProfile(A, A + FVector::UpVector * Lift,
                FQuat::Identity, Capsule->GetCollisionProfileName(), Shape, Query)
                || GetWorld()->SweepTestByProfile(A + FVector::UpVector * Lift, B + FVector::UpVector * 2.f,
                    FQuat::Identity, Capsule->GetCollisionProfileName(), Shape, Query)) return Fail(TEXT("capsule connection"));
        }
        if (!MassClear(At, Yaw)) return Fail(TEXT("torso/head connection ") + MassBlock);
        FHitResult Roof; const FVector Core = At + FVector::UpVector * 55.f;
        const float FreeHeight = GetWorld()->LineTraceSingleByChannel(Roof, At + FVector::UpVector * 3.f,
            At + FVector::UpVector * 350.f, ECC_Pawn, Query) ? Roof.Distance + 3.f : 350.f;
        float Span = 0.f;
        for (float Side : {-1.f, 1.f})
        {
            FHitResult Wall;
            Span += GetWorld()->LineTraceSingleByChannel(Wall, Core, Core + Right * Side * 400.f,
                ECC_Pawn, Query) ? Wall.Distance : 400.f;
        }
        Width = FMath::Min(Width, Span); Height = FMath::Min(Height, FreeHeight); Previous = At;
    }
    if (Width < 82.f || Height < 85.f) return Fail(TEXT("rigid minimum aperture"));
    const float Compression = FMath::Max(FMath::Clamp((160.f - Width) / 108.f, 0.f, 1.f),
        FMath::Clamp((180.f - Height) / 128.f, 0.f, 1.f));
    const float Side = FMath::Clamp((550.f - Width) / 405.f, 0.f, 1.f);
    const float Low = FMath::Clamp((180.f - Height) / 128.f, 0.f, 1.f);
    const float Haul = FMath::Lerp(1.f, .2f, FMath::SmoothStep(.4f, .9f, Side) * FMath::SmoothStep(.25f, .6f, Low));
    const float Fit = FMath::Lerp(1.f, .16f, FMath::Pow(Compression, 1.5f)) * Haul;
    const float Speed = FMath::Max(6.f, FMath::Min(Movement->MaxWalkSpeed, 230.f) * Fit);
    Seconds = Length / Speed + FMath::Abs(Turn) / 120.f;
    Posture = Height < 140.f ? TEXT("Low") : RecoverySearchPoseSize.Z > 230.f ? TEXT("Reared")
        : Width < 180.f ? TEXT("Folded") : TEXT("Ground");
    // Use the existing real light sampler, retaining sensed-goal semantics.
    if (GetWorld()->GetTimeSeconds() >= ShadowDirectUntil)
        Seconds += (1.f - GetShadowLevelAt(To + FVector::UpVector * 120.f)) * .3f;
    return Motion != nullptr;
}

bool ATallowHunterAIController::ExecuteRecoveryPlan(float Now, int32 NodeIndex, bool bPartial)
{
    if (!RecoverySearchNodes.IsValidIndex(NodeIndex)) return false;
    const FRecoverySearchNode Chosen = RecoverySearchNodes[NodeIndex];
    auto* Traversal = Creature->HunterTraversal.Get();
    const FVector Here = Creature->GetActorLocation();
    const FVector Destination = RecoverySearchTarget;
    FString Key = RecoveryConnectionKey(Chosen.Mode.IsNone() ? FName(TEXT("Floor")) : Chosen.Mode,
        RecoverySearchOrigin, Chosen.Mode.IsNone() ? RecoverySearchPoints[Chosen.Point] : Destination, Chosen.Yaw);
    if (RecoveryFailedEdges.Contains(Key)) return false;
    const double Began = FPlatformTime::Seconds();
    bool bStarted = false;
    if (!Chosen.Mode.IsNone())
    {
        if (Chosen.Mode == TEXT("Link"))
        {
            const FVector SavedGoal = Goal; const bool bSavedGoal = bHasGoal;
            Goal = Destination; bHasGoal = true;
            bStarted = TryLinkShortcut(Now, true);
            if (!bStarted) { Goal = SavedGoal; bHasGoal = bSavedGoal; }
        }
        else if (Chosen.Mode == TEXT("DirectCeiling")) bStarted = Traversal->StartDirectCeilingRoute(Destination);
        else if (Chosen.Mode == TEXT("Over")) bStarted = Traversal->StartOverRoute(Destination, true);
        else if (Chosen.Mode == TEXT("Wall")) bStarted = Traversal->StartWallWalk(Destination);
        else if (Chosen.Mode == TEXT("Ceiling")) bStarted = Traversal->StartChaseSurfaceRoute(Destination);
        else if (Chosen.Mode == TEXT("Crawl")) bStarted = Traversal->StartSqueezeToward(Destination);
    }
    else
    {
        TArray<FVector> Route;
        TArray<int32> RouteNodes;
        for (int32 I = NodeIndex; RecoverySearchNodes.IsValidIndex(I) && RecoverySearchNodes[I].Parent != INDEX_NONE;
            I = RecoverySearchNodes[I].Parent)
        { Route.Insert(RecoverySearchPoints[RecoverySearchNodes[I].Point], 0); RouteNodes.Insert(I, 0); }
        while (!Route.IsEmpty() && FVector::Dist2D(Route[0], Here) < 12.f)
        { Route.RemoveAt(0); RouteNodes.RemoveAt(0); }
        if (!Route.IsEmpty())
        {
            const bool bRetreat = RecoverySearchNodes[RouteNodes[0]].bKeepHeading;
            const FName EdgeMode = bRetreat ? TEXT("Retreat") : TEXT("Floor");
            Key = RecoveryConnectionKey(EdgeMode, RecoverySearchPoints[0], Route[0],
                bRetreat ? RecoverySearchYaw : (Route[0] - RecoverySearchPoints[0]).Rotation().Yaw);
            if (RecoveryFailedEdges.Contains(Key)) return false;
            const TGuardValue<bool> Internal(bRecoveryGoalRequest, true);
            StopMovement();
            if (bRetreat)
            {
                // This physical escape may re-enter navigation from outside
                // its polygon. Never ask MoveTo to rotate along this edge.
                Goal = Route[0]; bHasGoal = bStarted = true;
                bRecoveryCheckedTranslation = true; Route.SetNum(1);
            }
            else bStarted = SetGoal(Route[0]);
            if (bStarted)
            {
                Route.RemoveAt(0); RecoveryFloorQueue = MoveTemp(Route); bRecoveryDetour = true;
                bRecoveryTraversal = false;
            }
        }
    }
    RecoveryPlannerExecutorMs = (FPlatformTime::Seconds() - Began) * 1000.;
    ++RecoveryAttempts;
    if (!bStarted)
    {
        RecoveryFailedEdges.Add(Key, Chosen.Mode.IsNone() ? TEXT("floor goal rejected") : Traversal->LastRouteFailure);
        RecoveryPlannerFailedEdges = RecoveryFailedEdges.Num();
        return false;
    }
    RecoveryFinalGoal = Destination; RecoveryStartedAt = Here;
    RecoveryInitialDistance = FVector::Dist(Here, Destination);
    RecoveryMode = bRecoveryCheckedTranslation ? FName(TEXT("Retreat"))
        : Chosen.Mode.IsNone() ? FName(TEXT("Floor")) : Chosen.Mode;
    ActiveRecoveryKey = Key; bRecoveryWaiting = false;
    if (!Chosen.Mode.IsNone())
    {
        bRecoveryDetour = false; RecoveryFloorQueue.Reset();
        bRecoveryTraversal = Chosen.Mode != TEXT("Link");
        RecoveryTraversalAborts = Traversal->AbortedTransfers; StopMovement();
    }
    ProgressAt = Now; ProgressAnchor = Here; NextMoveRequest = Now;
    GoalDeadline = Now + FMath::Clamp(Chosen.Cost * 2.f + 8.f, 8.f, 180.f);
    RecoveryPlannerSeconds = Chosen.Cost;
    RecoveryPlannerStatus = FString::Printf(TEXT("executing %s%s heading%.0f posture%s; limbs retain live fit/plant gate"),
        *RecoveryMode.ToString(), bPartial ? TEXT(" partial") : TEXT(""), Chosen.Yaw, *Chosen.Posture.ToString());
    NoteRecovery(TEXT("shared search ") + RecoveryPlannerStatus);
    LastDecision = TEXT("Route recovery: executing checked movement plan");
    bRecoverySearchActive = false; bRecoverySearchFinished = true;
    return true;
}

bool ATallowHunterAIController::UpdateRecoveryPlanner(float Now, const FVector& Destination)
{
    auto* Motion = Creature->HunterMotion.Get();
    auto* Mesh = Creature->GetMesh();
    auto* Traversal = Creature->HunterTraversal.Get();
    auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!Motion || !Mesh || !Nav || AttackStartedAt >= 0.f) return false;
    const double SliceBegan = FPlatformTime::Seconds();
    const FVector Here = Creature->GetActorLocation();
    FBox Mass = Motion->MeasureAdaptiveFitBounds(Mesh->GetComponentSpaceTransforms(), INDEX_NONE, INDEX_NONE, true, true, false);
    if (Mass.IsValid) Mass = Mass.TransformBy(Mesh->GetRelativeTransform().ToMatrixWithScale());
    if (!Mass.IsValid) { RecoveryPlannerStatus = TEXT("waiting for evaluated structural pose"); return false; }
    uint32 Scene = 0;
    CacheDoors();
    for (const FDoorWatch& Door : Doors) if (Door.Door.IsValid())
        Scene = HashCombine(Scene, GetTypeHash(FMath::RoundToInt(DoorOpenness(Door) * 8.f) + (IsBarred(Door.Door.Get()) ? 100 : 0)));
    for (const FGateWatch& Gate : Gates) if (Gate.Gate.IsValid())
        Scene = HashCombine(Scene, GetTypeHash(FMath::RoundToInt(GateOpenness(Gate) * 8.f)));
    const uint32 WorldSignature = Scene;
    const FVector SensedSpace = Motion->GetFitPoseClearance();
    const FVector ContextSpace = SensedSpace.Equals(RecoverySearchClearance, 15.f)
        ? RecoverySearchClearance : SensedSpace;
    Scene = HashCombine(Scene, GetTypeHash(FIntVector(FMath::RoundToInt(ContextSpace.X / 15.f),
        FMath::RoundToInt(ContextSpace.Y / 15.f), FMath::RoundToInt(ContextSpace.Z / 15.f))));
    // A tucked arm can open a previously rejected escape without changing
    // the torso bounds. Include coarse actual articulation in failed edges.
    const FTransform ActorFrame = Creature->GetActorTransform();
    TArray<FVector> Arms;
    for (int32 Pair = 1; Pair <= 5; ++Pair) for (const TCHAR* Side : {TEXT("l"), TEXT("r")})
        for (const TCHAR* Joint : {TEXT("fore"), TEXT("hand")})
        {
            const FName Bone(*FString::Printf(TEXT("arm%02d_%s_%s"), Pair, Joint, Side));
            const FVector P = ActorFrame.InverseTransformPosition(Mesh->GetBoneLocation(Bone));
            Arms.Add(P);
        }
    bool bArmsChanged = Arms.Num() != RecoverySearchArms.Num();
    if (!bArmsChanged)
        for (int32 I = 0; I < Arms.Num(); ++I)
            bArmsChanged |= FVector::DistSquared(Arms[I], RecoverySearchArms[I]) > 25.f * 25.f;
    if (!bArmsChanged) Arms = RecoverySearchArms;
    for (const FVector& P : Arms)
        Scene = HashCombine(Scene, GetTypeHash(FIntVector(FMath::RoundToInt(P.X / 25.f),
            FMath::RoundToInt(P.Y / 25.f), FMath::RoundToInt(P.Z / 25.f))));
    const bool bPoseChanged = !RecoverySearchPoseSize.Equals(Mass.GetSize(), 14.f)
        || !RecoverySearchMass.IsValid || !RecoverySearchMass.GetCenter().Equals(Mass.GetCenter(), 14.f)
        || Scene != RecoverySceneSignature;
    const bool bChanged = FVector::Dist(RecoverySearchTarget, Destination) > 80.f
        || FVector::Dist(RecoverySearchOrigin, Here) > 60.f || WorldSignature != RecoveryWorldSignature
        || FMath::Abs(FMath::FindDeltaAngleDegrees(RecoverySearchYaw, Creature->GetActorRotation().Yaw)) > 25.f
        // Finish the bounded search through an idle/gait pose change. Every
        // selected executor rechecks live geometry before physical movement.
        || (!bRecoverySearchActive && bPoseChanged);
    if (bChanged || (!bRecoverySearchActive && !bRecoverySearchFinished))
    {
        // Bound long-session memory; stable exhausted searches do not rerun.
        if (RecoveryFailedEdges.Num() > 2048)
            for (auto It = RecoveryFailedEdges.CreateIterator(); It && RecoveryFailedEdges.Num() > 1024; ++It)
                if (It.Key() != ActiveRecoveryKey) It.RemoveCurrent();
        ResetRecoveryPlanner(); RecoverySceneSignature = Scene;
        RecoveryWorldSignature = WorldSignature; RecoverySearchClearance = ContextSpace;
        RecoverySearchArms = MoveTemp(Arms);
        RecoverySearchOrigin = Here; RecoverySearchTarget = Destination; RecoverySearchPoseSize = Mass.GetSize();
        RecoverySearchYaw = Creature->GetActorRotation().Yaw; RecoverySearchMass = Mass;
        RecoverySearchBegan = SliceBegan; bRecoverySearchActive = true;
        RecoveryFinalGoal = Destination; bRecoveryWaiting = true;
        RecoveryPlannerSourcePose = FString::Printf(TEXT("%s mass%s center%s heading%.1f fit%s"),
            *Motion->GetMimActionName().ToString(), *Mass.GetSize().ToCompactString(),
            *Mass.GetCenter().ToCompactString(), RecoverySearchYaw,
            *Motion->GetFitPoseClearance().ToCompactString());
        auto AddPoint = [&](const FVector& P)
        {
            for (const FVector& Existing : RecoverySearchPoints) if (FVector::DistSquared(P, Existing) < 8.f * 8.f) return;
            if (RecoverySearchPoints.Num() < 64) RecoverySearchPoints.Add(P);
        };
        const float HH = Creature->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        AddPoint(Here - FVector::UpVector * HH);
        for (float Radius : {20.f, 60.f, 120.f}) for (int32 Angle = 0; Angle < 8; ++Angle)
        {
            const float A = Angle * PI * .25f;
            AddPoint(Here - FVector::UpVector * HH + FVector(FMath::Cos(A), FMath::Sin(A), 0) * Radius);
        }
        const auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(this, Here, Destination, this);
        if (Path && Path->IsValid() && !Path->IsPartial() && !PathCrossesShut(Path->PathPoints))
        {
            AddPoint(Path->PathPoints[0]);
            for (int32 I = 0; I + 1 < Path->PathPoints.Num(); ++I)
            {
                const int32 Count = FMath::Max(1, FMath::CeilToInt(FVector::Dist(Path->PathPoints[I], Path->PathPoints[I + 1]) / 140.f));
                for (int32 J = 1; J <= Count; ++J) AddPoint(FMath::Lerp(Path->PathPoints[I], Path->PathPoints[I + 1], float(J) / Count));
            }
        }
        for (float Radius : {150.f, 300.f, 450.f}) for (int32 Angle = 0; Angle < 8; ++Angle)
        {
            const float A = Angle * PI * .25f; FNavLocation Projected;
            if (Nav->ProjectPointToNavigation(Here + FVector(FMath::Cos(A), FMath::Sin(A), 0) * Radius,
                Projected, FVector(60, 60, 180))) AddPoint(Projected.Location);
        }
        FRecoverySearchNode Start; Start.Yaw = RecoverySearchYaw;
        Start.Estimate = FVector::Dist(Here, Destination) / 400.f; RecoverySearchNodes.Add(Start);
        RankRecoveryRoutes(Destination); // Shared physical aperture/time observations.
        const float D = FVector::Dist(Here, Destination), H = GeometryPassage.Z;
        const float SurfaceSpeed = FMath::Max(20.f, Traversal->CrawlSpeed * .65f);
        const float Slow = FMath::Max(.03f, Motion->GetFitSpeed());
        const TPair<FName, float> Modes[] = {{TEXT("Link"), D / 200.f + 1.f},
            {TEXT("DirectCeiling"), (D + H * 1.5f) / SurfaceSpeed + 2.f},
            {TEXT("Over"), (D + 220.f) / SurfaceSpeed + 1.4f},
            {TEXT("Wall"), (D * 1.25f + 280.f) / SurfaceSpeed + 2.f},
            {TEXT("Ceiling"), (D + H * 2.f) / SurfaceSpeed + 2.8f},
            {TEXT("Crawl"), D / FMath::Max(6.f, Traversal->SqueezeSpeed * Slow * .65f) + .7f}};
        for (const auto& Mode : Modes)
        {
            if (!bUseSurfaceRoutes && Mode.Key != TEXT("Link") && Mode.Key != TEXT("Crawl")) continue;
            if (const float* Until = RecoveryCooldowns.Find(RecoveryKey(Mode.Key, Here, Destination)); Until && *Until > Now) continue;
            if (RecoveryFailedEdges.Contains(RecoveryConnectionKey(Mode.Key, Here, Destination, RecoverySearchYaw))) continue;
            FRecoverySearchNode Edge; Edge.Mode = Mode.Key; Edge.Parent = 0; Edge.Yaw = RecoverySearchYaw;
            Edge.Cost = Edge.Estimate = Mode.Value; RecoverySearchNodes.Add(Edge);
        }
        RecoveryPlannerStatus = TEXT("searching checked heading/posture graph");
    }
    auto FinishSlice = [&]()
    {
        const float Ms = (FPlatformTime::Seconds() - SliceBegan) * 1000.;
        RecoveryPlannerSliceMs = Ms; RecoveryPlannerCPUms += Ms;
        RecoveryPlannerWallMs = (FPlatformTime::Seconds() - RecoverySearchBegan) * 1000.;
        RecoveryPlannerFailedEdges = RecoveryFailedEdges.Num(); NextRecoveryTry = Now + .001f;
        RecoveryPlannerFailureSummary.Reset();
        for (const auto& Failure : RecoveryFailureReasons)
        {
            RecoveryPlannerFailureSummary += FString::Printf(TEXT("%d %s; "), Failure.Value, *Failure.Key);
            if (RecoveryPlannerFailureSummary.Len() > 1000) break;
        }
    };
    if (!bRecoverySearchActive) { NextRecoveryTry = Now + .5f; return false; }
    while ((FPlatformTime::Seconds() - SliceBegan) * 1000. < 2.f)
    {
        // Editor/frame stalls must not discard unexamined escape directions.
        // The hard budget counts work; the wall target may select a checked
        // partial result, but cannot turn an incomplete search into no route.
        if (RecoveryPlannerCPUms >= 100.f
            || (RecoveryBestPartial != INDEX_NONE && (FPlatformTime::Seconds() - RecoverySearchBegan) > .1)) break;
        if (RecoveryExpanding == INDEX_NONE)
        {
            float Best = MAX_flt;
            for (int32 I = 0; I < RecoverySearchNodes.Num(); ++I)
                if (!RecoverySearchNodes[I].bClosed && RecoverySearchNodes[I].Estimate < Best)
                { Best = RecoverySearchNodes[I].Estimate; RecoveryExpanding = I; }
            if (RecoveryExpanding == INDEX_NONE) break;
            RecoverySearchNodes[RecoveryExpanding].bClosed = true; RecoveryNeighbor = 0; ++RecoveryPlannerExpansions;
            const FRecoverySearchNode Node = RecoverySearchNodes[RecoveryExpanding];
            if (!Node.Mode.IsNone())
            {
                RecoveryExpanding = INDEX_NONE;
                if (ExecuteRecoveryPlan(Now, RecoverySearchNodes.IndexOfByPredicate([&](const FRecoverySearchNode& N)
                    { return N.Mode == Node.Mode && N.Parent == Node.Parent; }), false)) { FinishSlice(); return true; }
                FinishSlice(); return true; // At most one full executor preflight per slice.
            }
            const FVector At = RecoverySearchPoints[Node.Point];
            if (Node.Parent != INDEX_NONE && FVector::Dist2D(At, Destination) < 85.f && FMath::Abs(At.Z - Destination.Z) < 150.f)
            { const bool Started = ExecuteRecoveryPlan(Now, RecoveryExpanding, false); FinishSlice(); return Started; }
            if (Node.Parent != INDEX_NONE && (RecoveryBestPartial == INDEX_NONE
                || FVector::Dist(At, Destination) < FVector::Dist(RecoverySearchPoints[RecoverySearchNodes[RecoveryBestPartial].Point], Destination)))
                RecoveryBestPartial = RecoveryExpanding;
        }
        const int32 FromIndex = RecoveryExpanding;
        const FRecoverySearchNode From = RecoverySearchNodes[FromIndex];
        if (RecoveryNeighbor >= RecoverySearchPoints.Num()) { RecoveryExpanding = INDEX_NONE; continue; }
        const int32 Point = RecoveryNeighbor++;
        const FVector A = RecoverySearchPoints[From.Point], B = RecoverySearchPoints[Point];
        const float Length = FVector::Dist(A, B);
        if (Point == From.Point || Length > 260.f || Length < 18.f || RecoverySearchNodes.Num() >= 256) continue;
        float Yaw = (B - A).Rotation().Yaw;
        float Seconds = 0.f; FName Posture; FString Reason;
        bool bKeepHeading = false;
        if (!CheckRecoveryFloorEdge(A, From.Yaw, B, Seconds, Posture, Reason))
        {
            if (FromIndex != 0 || Length > 120.1f
                || !CheckRecoveryFloorEdge(A, From.Yaw, B, Seconds, Posture, Reason, true)) continue;
            bKeepHeading = true; Yaw = From.Yaw;
        }
        const FIntVector Lookup(Point, FMath::RoundToInt(Yaw / 45.f), int32(GetTypeHash(Posture)));
        const float Cost = From.Cost + Seconds;
        if (const int32* Existing = RecoverySearchLookup.Find(Lookup))
        {
            if (RecoverySearchNodes[*Existing].Cost <= Cost) continue;
            auto& Node = RecoverySearchNodes[*Existing]; Node.Parent = FromIndex; Node.Cost = Cost;
            Node.bKeepHeading = bKeepHeading;
            Node.Estimate = Cost + FVector::Dist(B, Destination) / 400.f; Node.bClosed = false;
            if (RecoveryBestPartial == INDEX_NONE || FVector::Dist(B, Destination)
                < FVector::Dist(RecoverySearchPoints[RecoverySearchNodes[RecoveryBestPartial].Point], Destination))
                RecoveryBestPartial = *Existing;
        }
        else
        {
            FRecoverySearchNode Node; Node.Point = Point; Node.Parent = FromIndex; Node.Yaw = Yaw;
            Node.bKeepHeading = bKeepHeading;
            Node.Posture = Posture; Node.Cost = Cost; Node.Estimate = Cost + FVector::Dist(B, Destination) / 400.f;
            const int32 Added = RecoverySearchNodes.Add(Node);
            RecoverySearchLookup.Add(Lookup, Added);
            if (RecoveryBestPartial == INDEX_NONE || FVector::Dist(B, Destination)
                < FVector::Dist(RecoverySearchPoints[RecoverySearchNodes[RecoveryBestPartial].Point], Destination))
                RecoveryBestPartial = Added;
        }
    }
    FinishSlice();
    const bool bContinue = RecoveryPlannerCPUms < 100.f
        && (RecoveryPlannerWallMs < 100.f || RecoveryBestPartial == INDEX_NONE);
    if (RecoveryExpanding != INDEX_NONE && bContinue) return true;
    bool bOpen = false;
    for (const auto& Node : RecoverySearchNodes) bOpen |= !Node.bClosed;
    if (bOpen && bContinue) return true;
    if (RecoveryBestPartial != INDEX_NONE && ExecuteRecoveryPlan(Now, RecoveryBestPartial, true)) return true;
    bRecoverySearchActive = false; bRecoverySearchFinished = true; bRecoveryWaiting = true;
    RecoveryPlannerStatus = TEXT("no checked local connection; watching retained evidence until state changes");
    LastDecision = TEXT("Route recovery: watching after checked search exhaustion");
    NoteRecovery(RecoveryPlannerStatus); NextRecoveryTry = Now + .5f;
    return false;
}


void ATallowHunterAIController::UpdateRecoveryFitAssist(float Now)
{
    if (!Creature || !Creature->HunterMotion || !Creature->HunterMotion->UsesMim()) return;
    auto* Capsule = Creature->GetCapsuleComponent();
    auto* Movement = Creature->GetCharacterMovement();
    auto* Traversal = Creature->HunterTraversal.Get();
    const FVector Here = Creature->GetActorLocation();
    const bool Travelling = (bHasGoal && FVector::Dist2D(Here, Goal) > (bRecoveryDetour ? 10.f : 95.f))
        || (bRecoveryWaiting && FVector::Dist2D(Here, RecoveryFinalGoal) > 60.f);
    // Stay folded while still travelling through the tight place that folded her:
    // restoring Normal after any 100 cm just re-blocked her in the same room and
    // looped stall -> assist -> restore forever. Unfold on arrival, or once the
    // fit layer measures a roomy opening again.
    const FVector AssistSpace = Creature->HunterMotion->GetFitClearance();
    const bool bRoomyAgain = AssistSpace.Y >= 150.f && AssistSpace.Z >= 150.f;
    if (bRecoveryFitAssist && !Traversal->IsTraversing() && Movement->IsMovingOnGround()
        && (!Travelling || (bRoomyAgain && Now - RecoveryAssistStarted > 1.f
            && FVector::Dist2D(Here, RecoveryAssistOrigin) > 100.f)))
    {
        const FVector RestoreAt = Here + FVector::UpVector
            * (RecoveryAssistHalfHeight - Capsule->GetScaledCapsuleHalfHeight());
        FCollisionQueryParams Query(SCENE_QUERY_STAT(HunterRestoreRecoveryFit), false, Creature);
        Query.AddIgnoredActors(Capsule->GetMoveIgnoreActors()); Query.AddIgnoredActor(Player);
        if (!GetWorld()->OverlapBlockingTestByProfile(RestoreAt, Creature->GetActorQuat(),
            Capsule->GetCollisionProfileName(), FCollisionShape::MakeCapsule(RecoveryAssistRadius, RecoveryAssistHalfHeight), Query))
        {
            FHitResult Hit;
            Capsule->MoveComponent(RestoreAt - Here, Creature->GetActorQuat(), true, &Hit);
            if (!Hit.bBlockingHit)
            {
                Capsule->SetCapsuleSize(RecoveryAssistRadius, RecoveryAssistHalfHeight, true);
                Movement->bForceNextFloorCheck = true;
                bRecoveryFitAssist = false; RecoveryAssistStalledSince = -1.f; ContractBlockedSince = -1.f;
                NoteRecovery(TEXT("temporary fit assistance ended; normal capsule restored"));
            }
        }
    }
    if (bRecoveryFitAssist) return;
    if (!bAllowRecoveryFitAssist || !Travelling || Traversal->IsTraversing()
        || AttackStartedAt >= 0.f || HuntMode == ETallowHuntMode::Captured || !Movement->IsMovingOnGround())
    { RecoveryAssistStalledSince = -1.f; return; }
    // A sustained contract hard block is a stall even if she creeps centimetres
    // meanwhile; the progress anchor alone kept resetting and never fired.
    const bool bContractStalled = ContractBlockedSince >= 0.f && Now - ContractBlockedSince > 1.2f;
    if (!bContractStalled)
    {
        if (RecoveryAssistStalledSince < 0.f || FVector::Dist2D(Here, RecoveryAssistAnchor) > 20.f)
        {
            RecoveryAssistAnchor = Here; RecoveryAssistStalledSince = Now;
            RecoveryAssistAttemptsAtProgress = RecoveryAttempts;
            return;
        }
        const float Stalled = Now - RecoveryAssistStalledSince;
        if (Stalled < 3.f && !(Stalled >= 1.5f && RecoveryAttempts >= RecoveryAssistAttemptsAtProgress + 2)) return;
    }
    const FVector Final = (bRecoveryWaiting || bRecoveryDetour) ? RecoveryFinalGoal : Goal;
    RecoveryAssistRadius = FMath::Max(34.f, Capsule->GetUnscaledCapsuleRadius());
    RecoveryAssistHalfHeight = FMath::Max(50.f, Capsule->GetUnscaledCapsuleHalfHeight());
    const float PreviousHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
    Capsule->SetCapsuleSize(FMath::Min(24.f, RecoveryAssistRadius), FMath::Min(38.f, RecoveryAssistHalfHeight), true);
    const float LowerBy = PreviousHalfHeight - Capsule->GetUnscaledCapsuleHalfHeight();
    if (LowerBy > 0.f)
    {
        FHitResult ResizeHit;
        Capsule->MoveComponent(-FVector::UpVector * LowerBy, Creature->GetActorQuat(), true, &ResizeHit);
        Creature->GetMesh()->AddLocalOffset(FVector::UpVector * LowerBy);
    }
    Movement->bForceNextFloorCheck = true;
    bRecoveryFitAssist = true; ++RecoveryFitAssistCount;
    RecoveryAssistOrigin = Here; RecoveryAssistStarted = Now;
    StopMovement(); PendingLink = INDEX_NONE; bLinkFinalGoal = false;
    bRecoveryCheckedTranslation = bRecoveryDetour = bRecoveryWaiting = false;
    RecoveryFloorQueue.Reset(); ResetRecoveryPlanner();
    const TGuardValue<bool> Internal(bRecoveryGoalRequest, true);
    SetGoalNearest(Final); ProgressAt = Now; ProgressAnchor = Here;
    NextMoveRequest = Now; GoalDeadline = Now + 60.f; ContractBlockedSince = -1.f;
    // TODO: replace this bounded physical-capsule fallback with complete
    // articulated-body route validation; visual furniture overlap is allowed here.
    NoteRecovery(TEXT("temporary fit assistance: reduced capsule after stalled/repeated recovery; wall sweeps retained"));
}
