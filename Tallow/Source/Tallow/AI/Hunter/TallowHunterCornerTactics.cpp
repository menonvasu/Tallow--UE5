#include "AI/Hunter/TallowHunterAIController.h"
#include "AI/Hunter/TallowHunterCreature.h"
#include "AI/Hunter/TallowHunterMotionComponent.h"
#include "AI/Hunter/TallowHunterTraversalComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Player/TallowCharacter.h"
#include "HAL/IConsoleManager.h"

namespace
{
// The looser brace (7 of 9 grips within 16 cm) let her take corners, but a 17-minute soak measured every one of its
// 14 climbs with arms or body 27-38 cm into the walls on the way up or down. Off until the climb itself is clean.
TAutoConsoleVariable<int32> CVarMimLooseCornerBrace(TEXT("tallow.Mim.LooseCornerBrace"), 0,
    TEXT("1: a corner brace may stand on 7 of 9 grips per wall within 16 cm. 0: all 9 within 10 cm (the old, strict test)."));
}

bool ATallowHunterAIController::UpdateCornerTactics(float Now)
{
    if (!Creature || !Player || !Creature->HunterTraversal) return false;
    auto* Traversal = Creature->HunterTraversal.Get();
    const FVector Up = FVector::UpVector;
    if (bCornerAmbush)
    {
        // This tactic runs before the ordinary hold branch. Preserve the same
        // physical, revalidated grab rather than suppressing capture updates.
        UpdateAttack(Now);
        if (HuntMode == ETallowHuntMode::Captured) return true;
        const FVector Ear = HeadLocation();
        const float HeardDistance = FVector::Dist(Ear, Player->GetActorLocation());
        const bool bSensed = bVisible || (SensesPlayerClose(HeardDistance)
            && !BoardsBetween(Ear, Player->GetPawnViewLocation())
            && HasClearTrace(Ear, Player->GetPawnViewLocation(), Player));
        if (bSensed)
        {
            Observe(Player->GetActorLocation(), bVisible ? 1.f : .8f, bVisible);
            if (bVisible) LastSeenAt = Now; else LastHeardAt = Now;
            LastEvidenceType = bVisible ? TEXT("Sight") : TEXT("CornerSound");
        }
        if (bCornerAttackPending)
        {
            if (Traversal->IsTraversing()) return true;
            bCornerAmbush = bCornerAttackPending = false; NextCornerProbe = Now + 15.f;
            if (bHasEvidence && Now - FMath::Max(LastSeenAt, LastHeardAt) < 6.f)
            {
                FireEvent(ETallowKeeperEvent::Reveal); Rush(EvidenceLocation, 2.5f);
                LastDecision = TEXT("CornerAmbush: down from the corner toward sensed evidence");
            }
            return false;
        }
        if (!Traversal->IsTraversing())
        { bCornerAmbush = false; NextCornerProbe = Now + 8.f; return false; }
        if (Creature->HunterMotion) Creature->HunterMotion->LookAt(EvidenceLocation + Up * 55.f, .4f);
        if (!Traversal->IsHolding()) return true;
        StopMovement();
        // The looser grip test lets a frame or a rail sit under two hands; if the body itself ends up in the wall,
        // measured, it lets go and never takes that corner again (no clipping, ever).
        if (Now >= NextCornerClipAudit && Creature->HunterMotion)
        {
            NextCornerClipAudit = Now + .5f;
            const TArray<FTallowClipHit> Clips = Creature->HunterMotion->AuditClipping(2.f);
            if (Clips.Num())
            {
                ClippedCorners.Add(CornerAmbushAnchor);
                bCornerAmbush = false; NextCornerProbe = Now + 8.f;
                Traversal->RequestGroundExit();
                UE_LOG(LogTemp, Display, TEXT("TALLOW_CORNER clipped at %s: %s in %s by %.1f cm, leaving"),
                    *CornerAmbushAnchor.ToCompactString(), *Clips[0].Part, *Clips[0].Against, Clips[0].Depth);
                LastDecision = TEXT("CornerExit: the brace put it into the wall, leaving");
                return true;
            }
        }
        const bool bFresh = bHasEvidence && Now - FMath::Max(LastSeenAt, LastHeardAt) < 5.f;
        if (bFresh && FVector::Dist2D(EvidenceLocation, CornerAmbushAnchor) < 600.f)
        {
            // A corner brace is not a top-facing launch. Keep its real grips
            // and climb down the checked history before the ground rush.
            bCornerAttackPending = true; Traversal->RequestGroundExit();
            LastDecision = TEXT("CornerAmbush: heard her below, unfolding down the walls");
        }
        else if (Now >= CornerHoldUntil || !bUseAmbushes
            || GetLightExposureAt(Ear) > 7.f)
        {
            bCornerAmbush = false; NextCornerProbe = Now + 15.f;
            Traversal->RequestGroundExit();
            LastDecision = TEXT("CornerExit: leaving the lit or exhausted ambush by its climb route");
        }
        return true;
    }
    if (!bUseAmbushes || !bUseDirector || bVisible || !bHasEvidence || EvidenceStrength < .2f
        || Now < NextCornerProbe || Now - FMath::Max(LastSeenAt, LastHeardAt) > 35.f
        || Traversal->IsTraversing() || bDisguised || bDisguisePending || bHoldPending
        || PlannedHold != EHold::None || PendingLink != INDEX_NONE || bBooPending || bWatchPending || bWatching
        || bRecoveryDetour || bRecoveryTraversal || bRecoveryWaiting) return false;
    NextCornerProbe = Now + 4.f;
    ++CornerProbes;
    // Why a probe finds no corner, for the log: walls seen, two-wall corners, then each test that turned one down.
    int32 Walls = 0, Pairs = 0, TooFar = 0, NoFloor = 0, Low = 0, Lit = 0, NoBrace = 0, RouteFailed = 0;
    const FVector Here = Creature->GetActorLocation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MimCornerRoom), true, Creature);
    Query.AddIgnoredActor(Player);
    TArray<AActor*> Attached; Creature->GetAttachedActors(Attached); Query.AddIgnoredActors(Attached);
    auto Trace = [&](const FVector& A, const FVector& B, FHitResult& Hit)
    { return GetWorld()->LineTraceSingleByChannel(Hit, A, B, ECC_Visibility, Query); };
    FHitResult Floor;
    if (!Trace(Here, Here - Up * 500.f, Floor) || Floor.ImpactNormal.Z < .8f) return false;
    const float FloorZ = Floor.ImpactPoint.Z;
    const FVector Origin(Here.X, Here.Y, FloorZ + 130.f);
    const float CurrentLight = GetLightExposureAt(HeadLocation());
    struct FCandidate { FVector Corner, A, B; float Score; };
    TArray<FCandidate> Candidates;
    for (int32 Ray = 0; Ray < 12 && Candidates.Num() < 8; ++Ray)
    {
        const float Angle = Ray * 2.f * PI / 12.f;
        FHitResult Wall;
        if (!Trace(Origin, Origin + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * 600.f, Wall)
            || FMath::Abs(Wall.ImpactNormal.Z) > .12f) continue;
        ++Walls;
        const FVector A = Wall.ImpactNormal.GetSafeNormal2D();
        const FVector Tangent = FVector::CrossProduct(Up, A);
        for (float Side : {-1.f, 1.f})
        {
            FHitResult Other;
            const FVector From = Wall.ImpactPoint + A * 40.f;
            if (!Trace(From, From + Tangent * Side * 350.f, Other)
                || FMath::Abs(Other.ImpactNormal.Z) > .12f) continue;
            const FVector B = Other.ImpactNormal.GetSafeNormal2D();
            const float Dot = FVector::DotProduct(A, B), Det = A.X * B.Y - A.Y * B.X;
            if (Dot < -.005f || Dot > .12f || FMath::Abs(Det) < .8f) continue;
            const float PA = FVector::DotProduct(A, Wall.ImpactPoint), PB = FVector::DotProduct(B, Other.ImpactPoint);
            FVector Corner((PA * B.Y - A.Y * PB) / Det, (A.X * PB - PA * B.X) / Det, FloorZ);
            const FVector Inward = (A + B).GetSafeNormal();
            if (Candidates.ContainsByPredicate([&](const FCandidate& C) { return FVector::Dist2D(C.Corner, Corner) < 60.f; })
                || ClippedCorners.ContainsByPredicate([&](const FVector& C) { return FVector::Dist(C, Corner) < 80.f; })) continue;
            ++Pairs;
            if (FVector::Dist2D(Here, Corner) > 600.f || FVector::Dist2D(EvidenceLocation, Corner) > 1200.f) { ++TooFar; continue; }
            FHitResult LocalFloor, Roof;
            if (!Trace(Corner + Inward * 140.f + Up * 160.f, Corner + Inward * 140.f - Up * 120.f, LocalFloor)
                || LocalFloor.ImpactNormal.Z < .8f) { ++NoFloor; continue; }
            const float Base = LocalFloor.ImpactPoint.Z;
            const bool bRoof = Trace(Corner + Inward * 140.f + Up * (Base - FloorZ + 180.f),
                Corner + Inward * 140.f + Up * (Base - FloorZ + 650.f), Roof);
            const float Height = bRoof ? Roof.ImpactPoint.Z - Base : 450.f;
            if (Height < 290.f) { ++Low; continue; }
            Corner.Z = FMath::Min(Base + 240.f, Base + Height - 115.f);
            const FVector Head = Corner + Inward * 140.f + Up * 70.f;
            const float Exposure = FMath::Max(GetLightExposureAt(Head), GetLightExposureAt(Corner + Inward * 140.f));
            // An already dark room may have no darker corner. Only require
            // relative improvement when leaving a visibly lit position.
            const float LightLimit = CurrentLight <= 5.f ? 5.f
                : FMath::Min(5.f, FMath::Max(.2f, CurrentLight * .65f));
            if (Exposure > LightLimit) { ++Lit; continue; }
            // Six hands want real plaster on both walls, but a picture rail, a skirting or a frame edge under two of
            // the nine grips on a wall is not a reason to give the corner up (under the old all-nine, 10 cm test no
            // brace was ever logged in a playtest of the furnished house).
            bool bWalls = true;
            for (const FVector& Normal : {A, B})
            {
                const FVector Along = FVector::VectorPlaneProject(Inward, Normal).GetSafeNormal();
                const bool bLoose = CVarMimLooseCornerBrace.GetValueOnGameThread() != 0;
                int32 Grips = 0;
                for (float Span : {45.f, 110.f, 175.f}) for (float Z : {-145.f, -45.f, 75.f})
                {
                    const FVector Surface = Corner + Along * Span + Up * Z;
                    FHitResult Brace;
                    if (Trace(Surface + Normal * 35.f, Surface - Normal * 25.f, Brace)
                        && FVector::DotProduct(Brace.ImpactNormal, Normal) >= .9f
                        && FVector::Dist(Brace.ImpactPoint, Surface) <= (bLoose ? 16.f : 10.f)) ++Grips;
                }
                bWalls &= Grips >= (bLoose ? 7 : 9);
            }
            if (bWalls) Candidates.Add({Corner, A, B, Exposure + .001f * FVector::Dist2D(Here, Corner)
                + .0005f * FVector::Dist2D(EvidenceLocation, Corner)});
            else ++NoBrace;
        }
    }
    Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Score < B.Score; });
    for (const FCandidate& Candidate : Candidates)
    {
        if (Traversal->StartCornerAmbushRoute(Candidate.Corner, Candidate.A, Candidate.B))
        {
            StopMovement(); bHasGoal = false; bCornerAmbush = true; bCornerAttackPending = false;
            CornerAmbushAnchor = Candidate.Corner; AmbushFocus = EvidenceLocation;
            CornerHoldUntil = Now + Random.FRandRange(12.f, 20.f);
            LastCornerProbe = FString::Printf(TEXT("braced at %s"), *Candidate.Corner.ToCompactString());
            UE_LOG(LogTemp, Display, TEXT("TALLOW_CORNER probe=%d walls=%d pairs=%d candidates=%d braced at %s"),
                CornerProbes, Walls, Pairs, Candidates.Num(), *Candidate.Corner.ToCompactString());
            LastDecision = TEXT("CornerClimb: taking a dark two-wall brace near remembered evidence");
            return true;
        }
        ++RouteFailed;
    }
    LastCornerProbe = FString::Printf(TEXT("walls=%d pairs=%d far=%d nofloor=%d low=%d lit=%d nobrace=%d route=%d %s"),
        Walls, Pairs, TooFar, NoFloor, Low, Lit, NoBrace, RouteFailed, *Traversal->LastRouteFailure.Left(80));
    UE_LOG(LogTemp, Display, TEXT("TALLOW_CORNER probe=%d none: %s"), CornerProbes, *LastCornerProbe);
    return false;
}
