#include "AI/Hunter/TallowHunterAIController.h"
#include "AI/Hunter/TallowHunterCreature.h"
#include "AI/Hunter/TallowHunterTraversalComponent.h"
#include "Components/LightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

float ATallowHunterAIController::GetLightExposureAt(const FVector& Point)
{
    if (!GetWorld()) return 0.f;
    const float Now = GetWorld()->GetTimeSeconds();
    const FIntVector Cell(FMath::FloorToInt(Point.X / 20.f), FMath::FloorToInt(Point.Y / 20.f), FMath::FloorToInt(Point.Z / 20.f));
    if (const FShadowSample* Saved = ShadowSamples.Find(Cell); Saved && Now - Saved->At < .2f
        && FVector::DistSquared(Point, Saved->Point) < 9.f) return Saved->Lux;
    if (Now >= NextShadowLights)
    {
        ShadowLights.Reset(); NextShadowLights = Now + 2.f;
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            if (It->IsEditorOnly()) continue;
            for (ULightComponent* Light : TInlineComponentArray<ULightComponent*>(*It))
                if (Light->IsRegistered()) ShadowLights.Add(Light);
        }
    }
    struct FContribution { ULightComponent* Light; FVector Source; float Lux; };
    TArray<FContribution, TInlineAllocator<16>> Relevant;
    for (const auto& Weak : ShadowLights)
    {
        ULightComponent* Light = Weak.Get();
        if (!Light || !Light->IsVisible() || !Light->bAffectsWorld
            || (Light->GetOwner() && Light->GetOwner()->IsHidden())) continue;
        // Use the renderer's own unit/IES conversion, including spot solid
        // angle, candela/lumen/nit/EV values and legacy unitless lights.
        const float Energy = Light->ComputeLightBrightness() * FLinearColor(Light->LightColor).GetLuminance();
        if (Energy <= 0.f) continue;
        FVector Source = Light->GetComponentLocation(); float Lux = 0.f;
        if (Cast<UDirectionalLightComponent>(Light))
        {
            Source = Point - Light->GetForwardVector() * 20000.f;
            Lux = Energy;
        }
        else if (const auto* Local = Cast<ULocalLightComponent>(Light))
        {
            const FVector ToPoint = Point - Source;
            const float Distance = ToPoint.Size(), Radius = Local->AttenuationRadius;
            if (Radius <= 1.f || Distance >= Radius) continue;
            float Cone = 1.f;
            if (const auto* Spot = Cast<USpotLightComponent>(Light))
            {
                const float Outer = FMath::Cos(FMath::DegreesToRadians(Spot->OuterConeAngle));
                const float Inner = FMath::Cos(FMath::DegreesToRadians(Spot->InnerConeAngle));
                Cone = FMath::Clamp((FVector::DotProduct(Light->GetForwardVector(), ToPoint.GetSafeNormal()) - Outer)
                    / FMath::Max(.001f, Inner - Outer), 0.f, 1.f);
                Cone *= Cone;
            }
            if (Cone <= .001f) continue;
            if (const auto* PointLight = Cast<UPointLightComponent>(Light); PointLight && !PointLight->bUseInverseSquaredFalloff)
                Lux = Energy * FMath::Pow(FMath::Max(0.f, 1.f - Distance / Radius), FMath::Max(1.f, PointLight->LightFalloffExponent));
            else
            {
                const float Cutoff = FMath::Square(FMath::Max(0.f, 1.f - FMath::Pow(Distance / Radius, 4.f)));
                Lux = Energy * Cutoff / FMath::Max(400.f, FMath::Square(Distance));
            }
            Lux *= Cone;
        }
        if (Lux > .001f) Relevant.Add({Light, Source, Lux});
    }
    Relevant.Sort([](const FContribution& A, const FContribution& B) { return A.Lux > B.Lux; });
    // A thought is bounded even in a heavily dressed room. The strongest local
    // lights dominate visibility; one scan is shared by routes, corners and eyes.
    float Total = 0.f;
    for (int32 I = 0; I < FMath::Min(12, Relevant.Num()); ++I)
    {
        const auto& Item = Relevant[I];
        if (Item.Light->CastShadows)
        {
            FCollisionQueryParams Query(SCENE_QUERY_STAT(MimShadowLight), true, Creature);
            Query.AddIgnoredActor(Item.Light->GetOwner());
            const FVector Ray = (Item.Source - Point).GetSafeNormal();
            if (GetWorld()->LineTraceTestByChannel(Point + Ray, Item.Source - Ray * 2.f, ECC_Visibility, Query)) continue;
        }
        Total += Item.Lux;
    }
    if (ShadowSamples.Num() > 256) ShadowSamples.Reset();
    ShadowSamples.Add(Cell, FShadowSample{Point, Now, Total});
    return Total;
}

float ATallowHunterAIController::GetShadowLevelAt(const FVector& Point)
{
    return FMath::Exp(-GetLightExposureAt(Point) / 3.f);
}

void ATallowHunterAIController::ResetShadowRouting()
{
    ShadowLights.Reset(); ShadowSamples.Reset();
    NextShadowLights = NextShadowPlan = ShadowWaypointUntil = ShadowDirectUntil = 0.f;
    bShadowWaypointActive = bShadowInternalGoal = false;
    ShadowDestination = ShadowWaypoint = FVector::ZeroVector;
    ShadowDetours = ShadowDirectCommits = 0;
    ShadowDirectExposure = ShadowChosenExposure = 0.f;
    ShadowRouteReason = TEXT("No light route yet");
}

bool ATallowHunterAIController::SelectShadowGoal(const FVector& Desired, FVector& Routed)
{
    Routed = Desired;
    if (!Creature || bShadowInternalGoal || bRecoveryGoalRequest || bCornerAmbush || bCornerAttackPending
        || TacticalPhase != NAME_None || PendingLink != INDEX_NONE || bOnLink
        || HuntMode == ETallowHuntMode::Captured || !Creature->HunterTraversal
        || Creature->HunterTraversal->IsTraversing() || Creature->HunterTraversal->GetSqueezeSeverity() > .05f)
        return false;
    const float Now = GetWorld()->GetTimeSeconds();
    const FVector Here = Creature->GetActorLocation();
    if (bVisible && !bHadVisible && Now >= ShadowDirectUntil && FVector::Dist2D(Here, Desired) > 180.f)
    {
        FVector DirectGoal; float DirectCost;
        if (IsReachable(Desired, DirectGoal, &DirectCost) && FVector::Dist2D(DirectGoal, Desired) <= 40.f
            && DirectGoal.Z <= Desired.Z + 20.f && Random.FRand() < .18f)
        {
            // Recognition can turn an existing shadow approach into a sudden
            // commitment, while keeping the same genuinely sensed destination.
            bShadowWaypointActive = false;
            ShadowDirectUntil = NextShadowPlan = Now + 3.5f;
            ++ShadowDirectCommits; ShadowRouteReason = TEXT("Committed direct pursuit on recognition");
            return false;
        }
    }
    bool bRestored = false;
    if (bShadowWaypointActive)
    {
        const FVector Actual = FVector::Dist2D(Desired, ShadowWaypoint) < 100.f ? ShadowDestination : Desired;
        if (Now < ShadowWaypointUntil && FVector::Dist2D(Actual, ShadowDestination) < 240.f
            && FVector::Dist2D(Here, ShadowWaypoint) > 70.f)
        {
            ShadowDestination = Actual; Routed = ShadowWaypoint;
            return true;
        }
        bShadowWaypointActive = false; Routed = Actual; bRestored = true;
        NextShadowPlan = Now + 3.f;
    }
    if (Now < NextShadowPlan || Now < ShadowDirectUntil || FVector::Dist2D(Here, Routed) < 180.f) return bRestored;
    NextShadowPlan = Now + 1.5f;
    FVector End; float DirectLength;
    if (!IsReachable(Routed, End, &DirectLength) || FVector::Dist2D(End, Routed) > 40.f || End.Z > Routed.Z + 20.f)
        return bRestored; // A real missing-nav aperture is handled by the fit/recovery planner.
    const auto* Direct = UNavigationSystemV1::FindPathToLocationSynchronously(this, Here, End, this);
    if (!Direct || Direct->IsPartial() || Direct->PathPoints.Num() < 2) return bRestored;
    if (bVisible && bHadVisible && Random.FRand() < .18f)
    {
        ShadowDirectUntil = NextShadowPlan = Now + 3.5f;
        ++ShadowDirectCommits; ShadowRouteReason = TEXT("Committed direct pursuit");
        return bRestored;
    }
    auto Exposure = [&](const UNavigationPath* Path)
    {
        float Mean = 0.f, Peak = 0.f;
        const auto& P = Path->PathPoints;
        // Six samples cover the actual navigation polyline, not a ray through walls.
        TArray<float, TInlineAllocator<32>> Lengths; float Length = 0.f;
        for (int32 I = 0; I + 1 < P.Num(); ++I) { Lengths.Add(FVector::Dist(P[I], P[I + 1])); Length += Lengths.Last(); }
        for (int32 Sample = 0; Sample < 6; ++Sample)
        {
            float Along = Length * Sample / 5.f; int32 Segment = 0;
            while (Segment + 1 < Lengths.Num() && Along > Lengths[Segment]) { Along -= Lengths[Segment]; ++Segment; }
            const FVector Floor = FMath::Lerp(P[Segment], P[Segment + 1], FMath::Clamp(Along / FMath::Max(1.f, Lengths[Segment]), 0.f, 1.f));
            const float Lux = GetLightExposureAt(Floor + FVector(0, 0, 55)) * .6f
                + GetLightExposureAt(Floor + FVector(0, 0, 110)) * .4f;
            const float Value = FMath::Loge(1.f + Lux);
            Mean += Value; Peak = FMath::Max(Peak, Value);
        }
        return Mean / 6.f + Peak * .7f;
    };
    ShadowDirectExposure = Exposure(Direct);
    ShadowChosenExposure = ShadowDirectExposure;
    if (ShadowDirectExposure < .2f) { ShadowRouteReason = TEXT("Direct route is already dark"); return bRestored; }
    const float Length = Direct->GetPathLength();
    // Darkness wins inside the explicit travel budget. Distance ranks the
    // safer options; it must not cancel a real shadow just because a candle
    // is physically dim in a dark-adapted room.
    float BestCost = MAX_flt;
    FVector Best = FVector::ZeroVector; float BestExposure = ShadowDirectExposure, BestEntryLength = 0.f;
    const FVector Forward = (End - Here).GetSafeNormal2D(), Right = FVector::CrossProduct(FVector::UpVector, Forward);
    const auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!Nav) return bRestored;
    for (const float Progress : {.35f, .65f})
        for (const float Side : {-1.f, 1.f})
            for (const float Width : {180.f, 300.f})
            {
                const FVector Candidate = Here + Forward * FMath::Clamp(FVector::Dist2D(Here, End) * Progress, 130.f, 440.f) + Right * Side * Width;
                FNavLocation Projected;
                if (!Nav->ProjectPointToNavigation(Candidate, Projected, FVector(70, 70, 130))
                    || FMath::Abs(Projected.Location.Z - (Here.Z - Creature->GetSimpleCollisionHalfHeight())) > 140.f
                    || FVector::Dist2D(Projected.Location, Here) < 130.f || FVector::Dist2D(Projected.Location, End) > FVector::Dist2D(Here, End) - 20.f) continue;
                const auto* A = UNavigationSystemV1::FindPathToLocationSynchronously(this, Here, Projected.Location, this);
                const auto* B = UNavigationSystemV1::FindPathToLocationSynchronously(this, Projected.Location, End, this);
                if (!A || !B || !A->IsValid() || !B->IsValid() || A->IsPartial() || B->IsPartial()
                    || A->PathPoints.Num() < 2 || B->PathPoints.Num() < 2 || PathCrossesShut(A->PathPoints) || PathCrossesShut(B->PathPoints)) continue;
                const float RouteLength = A->GetPathLength() + B->GetPathLength();
                if (RouteLength > Length + FMath::Min(650.f, Length * .5f + 220.f)) continue;
                const float RiskA = Exposure(A), RiskB = Exposure(B);
                const float Risk = bHasEvidence
                    ? (RiskA * A->GetPathLength() + RiskB * B->GetPathLength()) / FMath::Max(1.f, RouteLength) : RiskA;
                const float Cost = RouteLength + Risk * 180.f;
                if (Risk < ShadowDirectExposure * .90f && Cost + 35.f < BestCost)
                { BestCost = Cost; Best = Projected.Location; BestExposure = Risk; BestEntryLength = A->GetPathLength(); }
            }
    if (Best.IsZero()) { ShadowRouteReason = TEXT("No safer reachable shadow approach"); return bRestored; }
    ShadowDestination = Routed; ShadowWaypoint = Best; Routed = Best;
    bShadowWaypointActive = true;
    ShadowWaypointUntil = Now + FMath::Clamp(BestEntryLength / 95.f + 2.f, 4.f, 9.f);
    ShadowChosenExposure = BestExposure; ++ShadowDetours;
    ShadowRouteReason = TEXT("Approaching sensed evidence through darkness");
    return true;
}

bool ATallowHunterAIController::ResumeShadowDestination(float Now)
{
    if (!bShadowWaypointActive) return false;
    const FVector Destination = ShadowDestination;
    bShadowWaypointActive = false; NextShadowPlan = Now + 4.f;
    if (!bHasEvidence && (HuntMode == ETallowHuntMode::Roam || HuntMode == ETallowHuntMode::Withdraw))
    {
        bHasGoal = false; StopMovement();
        NextDecision = FMath::Max(NextDecision, Now + 3.f);
        ShadowRouteReason = TEXT("Staying in reachable darkness");
        return true;
    }
    const TGuardValue<bool> Internal(bShadowInternalGoal, true);
    SetGoalNearest(Destination);
    ShadowRouteReason = TEXT("Shadow approach complete; closing on remembered evidence");
    return true;
}
