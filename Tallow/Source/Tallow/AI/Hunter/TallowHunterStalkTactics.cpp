#include "AI/Hunter/TallowHunterAIController.h"
#include "AI/Hunter/TallowHunterCreature.h"
#include "AI/Hunter/TallowHunterDirector.h"
#include "AI/Hunter/TallowHunterMotionComponent.h"
#include "AI/Hunter/TallowHunterSearchAnchor.h"
#include "AI/Hunter/TallowHunterTraversalComponent.h"
#include "Audio/TallowKeeperVoiceComponent.h"
#include "Candle/TallowCandleComponent.h"
#include "Environment/TallowDoor.h"
#include "EngineUtils.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "PatrolPoint/TallowPatrolPoint.h"
#include "Player/TallowCharacter.h"

// The stalk (28 Oct: "I want to see Mim stalk and ambush and hide more... be seen watching"). While the evening
// builds it stands off in the dark a room or a hall away, where she can see it from where she is, holds still and
// watches. Turn the light on it and it slips out of sight; it comes back a little closer. The post is chosen with the
// same area knowledge as the pass-by; it never pursues from a watch on that knowledge, only on her lit candle, a sound,
// or her breath close by, exactly as anywhere else.

namespace
{
    // Walking distance from her to the post, by how many watches it has already held this build: far, nearer, close.
    const float WatchBands[3][2] = {{1000.f, 1500.f}, {750.f, 1050.f}, {550.f, 800.f}};
    // When no band has a spot: anywhere in her sight 6-12 m off by walking distance, partly lit at worst.
    const float WideBand[2] = {600.f, 1200.f};
    // Her lit candle slips a watch closer than this; lit, every post starts a little beyond it.
    const float WatchSlipCm = 620.f;
    const float WatchLitClearCm = WatchSlipCm + 30.f;

    bool SightBetween(const UWorld* World, const FVector& From, const FVector& To, const AActor* A, const AActor* B)
    {
        FCollisionQueryParams Query(SCENE_QUERY_STAT(HunterWatchSight), true, A);
        if (B) Query.AddIgnoredActor(B);
        FHitResult Hit;
        return !World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Query);
    }
}

bool ATallowHunterAIController::FindWatchPost(const FVector& Area, float MinPath, float MaxPath, FVector& Post, float LightLimit)
{
    if (!Creature || !Player) return false;
    auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
    FNavLocation AreaFloor;
    if (!Nav || !Nav->ProjectPointToNavigation(Area, AreaFloor, FVector(120.f, 120.f, 250.f))) return false;
    const FVector Here = Creature->GetActorLocation();
    // She has to be able to see it from where she is (her eyes when she is the area, else a child's eye over the sound).
    const FVector Eye = FVector::Dist(Area, Player->GetActorLocation()) < 200.f ? Player->GetPawnViewLocation()
        : AreaFloor.Location + FVector(0.f, 0.f, 120.f);
    FVector View = Player->GetControlRotation().Vector(); View.Z = 0.f; View.Normalize();
    const bool bLitNow = Player->GetCandleComponent() && Player->GetCandleComponent()->GetCandleState() != ECandleState::Out;
    struct FCandidate { FVector Point; float Bonus; };
    TArray<FCandidate> Candidates;
    for (TActorIterator<ATallowPatrolPoint> It(GetWorld()); It; ++It) Candidates.Add({It->GetActorLocation(), 0.f});
    for (TActorIterator<ATallowHunterSearchAnchor> It(GetWorld()); It; ++It)
        if (It->bEnabled && It->AnchorType != ETallowHunterAnchorType::Squeeze) Candidates.Add({It->GetActorLocation(), 20.f});
    // The floor itself, in rings round her: the far end of a corridor, a shadowed corner, the dark side of a room.
    for (int32 Ring = 0; Ring < 3; ++Ring)
        for (int32 Ray = 0; Ray < 16; ++Ray)
        {
            const float Angle = (Ray + .5f * Ring) * 2.f * PI / 16.f;
            Candidates.Add({AreaFloor.Location + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * FMath::Lerp(MinPath, MaxPath, (Ring + .5f) / 3.f), -10.f});
        }
    CacheDoors();
    // Chapter two is the stalking chapter (29 Oct): there it all but always takes a doorway when one will do.
    const UTallowHunterDirector* Pacing = GetHuntDirector();
    const float DoorBonus = Pacing && Pacing->GetAct() == ETallowStoryAct::FuseHunt ? 240.f : 120.f;
    for (const FDoorWatch& Watch : Doors)
    {
        if (!Watch.Door.IsValid() || IsBarred(Watch.Door.Get()) || DoorOpenness(Watch) < .5f) continue;
        // Framed in a doorway, just through it on the far side from her: the oldest shape in the house.
        const FVector Away = (Watch.Center - AreaFloor.Location).GetSafeNormal2D();
        for (const float Depth : {70.f, 150.f}) Candidates.Add({Watch.Center + Away * Depth, DoorBonus});
    }
    float BestScore = -MAX_flt;
    // Why a search finds no post, for the log (each test that turned candidates down).
    int32 Far = 0, Repeat = 0, NoNav = 0, Unseen = 0, Lit = 0, NoWalk = 0, Unreachable = 0, Passes = 0;
    for (const FCandidate& Candidate : Candidates)
    {
        const float Straight = FVector::Dist2D(Candidate.Point, AreaFloor.Location);
        if (Straight > MaxPath || Straight < MinPath * .55f || FMath::Abs(Candidate.Point.Z - AreaFloor.Location.Z) > 160.f) { ++Far; continue; }
        if (FVector::Dist2D(Candidate.Point, LastWatchPost) < 250.f) { ++Repeat; continue; }
        FNavLocation Floor;
        if (!Nav->ProjectPointToNavigation(Candidate.Point, Floor, FVector(80.f, 80.f, 200.f))) { ++NoNav; continue; }
        const FVector Head = Floor.Location + FVector(0.f, 0.f, 75.f);
        if (!SightBetween(GetWorld(), Eye, Head, Creature, Player) || BoardsBetween(Eye, Head)) { ++Unseen; continue; }
        const float Exposure = GetLightExposureAt(Head);
        if (Exposure >= LightLimit) { ++Lit; continue; }
        const UNavigationPath* FromHer = UNavigationSystemV1::FindPathToLocationSynchronously(
            this, Floor.Location, AreaFloor.Location, this);
        if (!FromHer || !FromHer->IsValid() || FromHer->IsPartial()) { ++NoWalk; continue; }
        const float Walk = FromHer->GetPathLength();
        if (Walk < MinPath || Walk > MaxPath) { ++NoWalk; continue; }
        FVector Projected; float MimPath = 0.f;
        if (!IsReachable(Floor.Location, Projected, &MimPath) || MimPath > 2600.f) { ++Unreachable; continue; }
        if (bLitNow && FVector::Dist2D(Projected, Player->GetActorLocation()) < WatchLitClearCm) { ++Far; continue; }
        // It gets there without passing her: no point of its way within 5 m of her.
        const UNavigationPath* Way = UNavigationSystemV1::FindPathToLocationSynchronously(
            this, Here, Projected, this);
        if (!Way || !Way->IsValid()) { ++Unreachable; continue; }
        bool bPasses = false;
        for (int32 I = 0; I + 1 < Way->PathPoints.Num() && !bPasses; ++I)
            bPasses = FMath::PointDistToSegment(AreaFloor.Location, Way->PathPoints[I], Way->PathPoints[I + 1]) < 500.f;
        if (bPasses) { ++Passes; continue; }
        const float Ahead = FMath::Max(0.f, FVector::DotProduct(View, (Projected - AreaFloor.Location).GetSafeNormal2D()));
        const float Score = Candidate.Bonus + Ahead * 80.f - Exposure * 40.f - MimPath * .04f
            - FMath::Abs(Walk - (MinPath + MaxPath) * .5f) * .1f + Random.FRandRange(0.f, 60.f);
        if (Score > BestScore) { BestScore = Score; Post = Projected; bWatchFromDoor = Candidate.Bonus >= 120.f; }
    }
    if (BestScore == -MAX_flt)
        UE_LOG(LogTemp, Display, TEXT("TALLOW_WATCH none in %.0f-%.0f: candidates=%d far=%d repeat=%d nonav=%d unseen=%d lit=%d walk=%d unreachable=%d passes=%d"),
            MinPath, MaxPath, Candidates.Num(), Far, Repeat, NoNav, Unseen, Lit, NoWalk, Unreachable, Passes);
    return BestScore > -MAX_flt;
}

bool ATallowHunterAIController::TryWatch(float Now, const FVector& Area, bool bFromNoise)
{
    if (!Creature || !Player || !bUseDirector || !bUseAmbushes || bVisible || bWatchPending || bWatching
        || Now < NextWatchTry || PlannedHold != EHold::None || bDisguisePending || bBooPending || bCornerAmbush
        || PendingLink != INDEX_NONE || bOnLink || Player->IsTiedToChair()) return false;
    const UTallowHunterDirector* Director = GetHuntDirector();
    if (!Director || Creature->HunterTraversal->IsTraversing()) return false;
    NextWatchTry = Now + 3.f;
    WatchStage = Director->GetWatchStage();
    FVector Post;
    bool bFound = false;
    // Its own band first; then nearer, then further. In a house a dark spot 10-15 m walk away that she can also see
    // is rare (a 17-minute soak never found one), and a watcher a band too close beats no watcher.
    // With her candle lit no post sits inside the slip radius, or the watch would end the moment it began.
    const bool bLit = Player->GetCandleComponent() && Player->GetCandleComponent()->GetCandleState() != ECandleState::Out;
    const auto Nearest = [bLit](float Band) { return bLit ? FMath::Max(Band, WatchLitClearCm) : Band; };
    for (const int32 Stage : {WatchStage, WatchStage + 1, WatchStage + 2, WatchStage - 1, WatchStage - 2})
        if (!bFound && Stage >= 0 && Stage < 3) bFound = FindWatchPost(Area, Nearest(WatchBands[Stage][0]), WatchBands[Stage][1], Post, 2.f);
    // Still nothing: 6-12 m and half-dark will do (her candle still has to reach it before it is lit up).
    if (!bFound) bFound = FindWatchPost(Area, Nearest(WideBand[0]), WideBand[1], Post, 3.5f);
    if (!bFound || !SetGoal(Post))
    {
        LastDecision = TEXT("Watch: no dark spot in her sight");
        return false;
    }
    EnterMode(ETallowHuntMode::Ambush);
    bWatchPending = true;
    WatchArea = Area; WatchPost = Post;
    AmbushFocus = Area + FVector(0.f, 0.f, 80.f);
    bTravellingToAmbush = true;
    AmbushUntil = Now + 40.f;
    if (bFromNoise) SearchUntil = FMath::Max(SearchUntil, Now + 55.f);
    SetPace(.8f, 40.f);
    ++WatchesStarted;
    LastDecision = bFromNoise ? TEXT("Watch: to a dark spot to watch where it heard her")
        : TEXT("Watch: creeping to a dark spot where she will see it");
    return true;
}

void ATallowHunterAIController::BeginWatch(float Now)
{
    bWatchPending = false;
    bWatching = true; bWatchStaring = false;
    StopMovement(); bHasGoal = false; bTravellingToAmbush = false;
    WatchUntil = Now + Random.FRandRange(4.f, 7.f);
    AmbushUntil = WatchUntil + 2.f;
    WatchLookedAt = 0.f; NextWatchGaze = 0.f;
    LastWatchPost = Creature->GetActorLocation();
    ++WatchesHeld;
    LastDecision = bWatchFromDoor ? TEXT("Watch: from the doorway, watching her") : TEXT("Watch: standing in the dark, watching her");
}

void ATallowHunterAIController::UpdateWatch(float Now, float DeltaSeconds)
{
    WatchGlint = FMath::FInterpConstantTo(WatchGlint, bWatching ? 1.f : 0.f, DeltaSeconds, bWatching ? 1.5f : .8f);
    if (bWatchPending && HuntMode != ETallowHuntMode::Ambush) bWatchPending = false;
    if (!bWatching)
    {
        // Out of her sight again: a little laugh from where it went, now and then.
        if (SlipGiggleAt > 0.f && Now >= SlipGiggleAt)
        {
            SlipGiggleAt = -1.f;
            if (Creature && Creature->Voice && !PlayerLooksAt(HeadLocation(), .5f)) Creature->Voice->Say(ETallowKeeperLine::Giggle);
        }
        return;
    }
    if (!Creature || !Player || HuntMode != ETallowHuntMode::Ambush) { bWatching = bWatchStaring = false; return; }
    if (bWatchStaring || bWatchReviewHold) return;   // the sight branch owns the stare; a review hold stays put
    // Face her side of the house, slowly, standing (never on a wall).
    if (!Creature->HunterTraversal->IsTraversing() && Creature->GetActorUpVector().Z > .9f)
    {
        const float Want = (WatchArea - Creature->GetActorLocation()).Rotation().Yaw;
        const float Yaw = Creature->GetActorRotation().Yaw;
        const float Turn = FMath::Clamp(FMath::FindDeltaAngleDegrees(Yaw, Want), -90.f * DeltaSeconds, 90.f * DeltaSeconds);
        if (FMath::Abs(Turn) > .01f) Creature->SetActorRotation(FRotator(0.f, Yaw + Turn, 0.f));
    }
    // In the dark it does not know where in the room she is: the gaze wanders over the place, it does not lock on.
    if (Now >= NextWatchGaze)
    {
        NextWatchGaze = Now + Random.FRandRange(1.4f, 2.4f);
        const float Angle = Random.FRandRange(0.f, 2.f * PI);
        AmbushFocus = WatchArea + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Random.FRandRange(80.f, 160.f)
            + FVector(0.f, 0.f, Random.FRandRange(60.f, 110.f));
    }
    const float Away = FVector::Dist(Creature->GetActorLocation(), Player->GetActorLocation());
    const bool bLit = Player->GetCandleComponent() && Player->GetCandleComponent()->GetCandleState() != ECandleState::Out;
    // Her eyes on its face, nothing between, long enough to have seen it: then it goes.
    const bool bLooked = !Player->IsHiddenInSpot() && Away < 1800.f && PlayerLooksAt(HeadLocation(), .94f);
    WatchLookedAt = bLooked ? WatchLookedAt + DeltaSeconds : FMath::Max(0.f, WatchLookedAt - DeltaSeconds * .5f);
    if (WatchLookedAt >= .35f) { EndWatch(Now, true, TEXT("Watch: she looked straight at it, slipping away")); return; }
    if (bLit && Away < WatchSlipCm) { EndWatch(Now, true, TEXT("Watch: her light was about to reach it, slipping away")); return; }
    if (Now >= WatchUntil) EndWatch(Now, false, TEXT("Watch: had its look, moving off"));
}

void ATallowHunterAIController::EndWatch(float Now, bool bSlip, const TCHAR* Why)
{
    const bool bHeld = bWatching;
    bWatchReviewHold = false;
    if (!bHeld && !bWatchPending) return;
    bWatching = bWatchPending = bWatchStaring = false;
    if (bHeld)
        if (auto* Director = GetHuntDirector()) Director->NoteWatch(bSlip);
    bTravellingToAmbush = false; AmbushUntil = 0.f;
    const bool bLeave = bHeld && Creature && Player && (bSlip || FCString::Strstr(Why, TEXT("moving off")));
    if (!bLeave)
    {
        // Something else needs answering (a sound elsewhere, the flame held on it): out of the watch, nothing more.
        EnterMode(ETallowHuntMode::Roam);
        StopMovement(); bHasGoal = false;
        LastDecision = Why;
        return;
    }
    // Out of her sight: a point a few metres off that she cannot see from where she stands, away from her.
    const FVector Post = Creature->GetActorLocation();
    const FVector Eye = Player->GetPawnViewLocation();
    const FVector Back = (Post - Player->GetActorLocation()).GetSafeNormal2D();
    FVector Best = FVector::ZeroVector; float BestScore = -MAX_flt;
    for (int32 Try = 0; Try < 14; ++Try)
    {
        const float Angle = Random.FRandRange(-110.f, 110.f);
        const FVector Candidate = Post + Back.RotateAngleAxis(Angle, FVector::UpVector) * Random.FRandRange(250.f, 500.f);
        FVector Projected; float Path = 0.f;
        if (!IsReachable(Candidate, Projected, &Path) || Path > 900.f || FMath::Abs(Projected.Z - Post.Z) > 160.f) continue;
        if (SightBetween(GetWorld(), Eye, Projected + FVector(0.f, 0.f, 75.f), Creature, Player)) continue;
        const float Score = FVector::DotProduct((Projected - Post).GetSafeNormal2D(), Back) * 100.f - Path * .1f;
        if (Score > BestScore) { BestScore = Score; Best = Projected; }
    }
    EnterMode(ETallowHuntMode::Withdraw);
    LastDecision = Why;
    if (BestScore > -MAX_flt && SetGoal(Best))
    {
        SetPace(bSlip ? 1.25f : 1.f, 4.f);
        // Let it reach the dark before the pacing sends it anywhere, and let a sighting sink in before it comes back.
        NextAreaDecision = Now + 6.f;
        NextDecision = Now + Random.FRandRange(5.f, 9.f);
    }
    else { bHasGoal = false; NextDecision = Now + .5f; }
    if (bSlip)
    {
        ++WatchSlips;
        WatchSlipUntil = Now + 3.f;
        if (Random.FRand() < .6f) SlipGiggleAt = Now + Random.FRandRange(1.8f, 2.2f);
    }
}

bool ATallowHunterAIController::ReviewWatchAt(const FVector& Where, bool bHold)
{
    if (!Creature || !Player) return false;
    CancelSpiderPlans();
    StopMovement();
    FVector Spot = Where;
    FNavLocation Floor;
    if (const auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld()); Nav && Nav->ProjectPointToNavigation(Where, Floor, FVector(150, 150, 250)))
        Spot = Floor.Location;
    const FVector Toward = (Player->GetActorLocation() - Spot).GetSafeNormal2D();
    Creature->SetActorLocationAndRotation(Spot + FVector(0, 0, Creature->GetSimpleCollisionHalfHeight() + 2.f),
        FRotator(0.f, Toward.Rotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
    Creature->HunterMotion->SnapFit();
    WatchArea = Player->GetActorLocation();
    EnterMode(ETallowHuntMode::Ambush);
    BeginWatch(GetWorld()->GetTimeSeconds());
    if (bHold)
    {
        bWatchReviewHold = true;
        WatchUntil = GetWorld()->GetTimeSeconds() + 600.f; AmbushUntil = WatchUntil + 2.f;
        WatchGlint = 1.f;
    }
    return true;
}
