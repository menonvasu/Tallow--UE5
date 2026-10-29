#include "AI/Hunter/TallowHunterAIController.h"
#include "AI/Hunter/TallowHunterCreature.h"
#include "AI/Hunter/TallowHunterMotionComponent.h"
#include "AI/Hunter/TallowHunterTraversalComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Player/TallowCharacter.h"

TArray<FName> ATallowHunterAIController::RankRecoveryRoutes(const FVector& FinalGoal)
{
    const FVector Here = Creature->GetActorLocation(), Up = FVector::UpVector;
    const FVector Forward = (FinalGoal - Here).GetSafeNormal2D();
    const FVector Right = FVector::CrossProduct(Up, Forward);
    const float Distance = FVector::Dist2D(Here, FinalGoal), ProbeDistance = FMath::Min(420.f, Distance);
    const float HalfHeight = Creature->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FVector FloorAt = Here - Up * HalfHeight;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MimAffordanceCosts), true, Creature);
    Query.AddIgnoredActor(Player);
    Query.AddIgnoredActors(Creature->GetCapsuleComponent()->GetMoveIgnoreActors());
    TArray<AActor*> Attached; Creature->GetAttachedActors(Attached); Query.AddIgnoredActors(Attached);
    auto Trace = [&](const FVector& A, const FVector& B, FHitResult& Hit)
    { return GetWorld()->LineTraceSingleByChannel(Hit, A, B, ECC_Visibility, Query); };
    float Width = 400.f, Height = 350.f, LastFloor = FloorAt.Z;
    bool bContinuousFloor = true;
    const int32 Samples = FMath::Clamp(FMath::CeilToInt(ProbeDistance / 40.f), 2, 11);
    for (int32 I = 0; I <= Samples; ++I)
    {
        FVector At = FloorAt + Forward * (ProbeDistance * I / Samples); At.Z = LastFloor;
        FHitResult Floor;
        if (!Trace(At + Up * 40.f, At - Up * 120.f, Floor)
            || Floor.ImpactNormal.Z < .7f || FMath::Abs(Floor.ImpactPoint.Z - LastFloor) > 35.f)
        { bContinuousFloor = false; continue; }
        At = Floor.ImpactPoint; LastFloor = At.Z;
        FHitResult Roof;
        const float Headroom = Trace(At + Up * 3.f, At + Up * 350.f, Roof)
            ? FMath::Max(0.f, Roof.ImpactPoint.Z - At.Z) : 350.f;
        Height = FMath::Min(Height, Headroom);
        for (const float Fraction : {.25f, .55f, .85f})
        {
            const FVector Core = At + Up * FMath::Max(8.f, FMath::Min(Headroom - 8.f, 100.f) * Fraction);
            float Span = 0.f;
            for (float Side : {-1.f, 1.f})
            {
                FHitResult Wall;
                Span += Trace(Core, Core + Right * Side * 200.f, Wall) ? Wall.Distance : 200.f;
            }
            Width = FMath::Min(Width, Span);
        }
    }
    GeometryPassage = FVector(ProbeDistance, Width, Height);
    const float Compression = FMath::Max(FMath::Clamp((166.f - Width) / 108.f, 0.f, 1.f),
        FMath::Clamp((186.f - Height) / 128.f, 0.f, 1.f));
    const float SidePressure = FMath::Clamp((550.f - (Width - 6.f)) / 405.f, 0.f, 1.f);
    const float LowPressure = FMath::Clamp((180.f - (Height - 2.f)) / 128.f, 0.f, 1.f);
    const float CombinedHaul = FMath::Lerp(1.f, .2f,
        FMath::SmoothStep(.4f, .9f, SidePressure) * FMath::SmoothStep(.25f, .6f, LowPressure));
    const float FitScale = FMath::Lerp(1.f, .16f, FMath::Pow(Compression, 1.5f)) * CombinedHaul;
    const auto* Traversal = Creature->HunterTraversal.Get();
    const float CrawlSpeed = FMath::Max(12.f, Traversal->SqueezeSpeed * FitScale * .65f);
    const float SurfaceSpeed = FMath::Max(20.f, Traversal->CrawlSpeed * .65f);
    const float CeilingSpeed = FMath::Max(20.f, Traversal->CeilingSpeed * .65f);
    struct FCandidate { FName Mode; float Seconds; };
    TArray<FCandidate> Candidates;
    const bool bConstrained = Width < 76.f || Height < HalfHeight * 2.f + 8.f;
    Candidates.Add({TEXT("Crawl"), ProbeDistance / CrawlSpeed
        + FMath::Max(0.f, Distance - ProbeDistance) / 190.f
        + (!bContinuousFloor || Width < 82.f || Height < 85.f || !bConstrained ? 500.f : 0.f)});
    if (bUseSurfaceRoutes)
    {
        Candidates.Add({TEXT("Over"), (Distance + 220.f) / SurfaceSpeed + 1.4f});
        Candidates.Add({TEXT("Wall"), (Distance * 1.25f + 280.f) / SurfaceSpeed + 2.f});
        Candidates.Add({TEXT("Ceiling"), (Distance + FMath::Clamp(Height, 170.f, 420.f) * 2.f) / CeilingSpeed + 2.8f});
    }
    Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Seconds < B.Seconds; });
    GeometryRouteCosts = FString::Printf(TEXT("measured %.0fx%.0fcm, fit speed %.2f; "), Width, Height, FitScale);
    TArray<FName> Order;
    for (const auto& Candidate : Candidates)
    {
        Order.Add(Candidate.Mode);
        GeometryRouteCosts += FString::Printf(TEXT("%s=%.1fs "), *Candidate.Mode.ToString(), Candidate.Seconds);
    }
    NoteRecovery(GeometryRouteCosts);
    return Order;
}
