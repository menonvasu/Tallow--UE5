#include "AI/Hunter/TallowHunterAIController.h"
#include "AI/Hunter/TallowHunterCreature.h"
#include "AI/Hunter/TallowHunterDirector.h"
#include "AI/Hunter/TallowHunterMotionComponent.h"
#include "AI/Hunter/TallowHunterTraversalComponent.h"
#include "Algo/Reverse.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Candle/TallowCandleComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Environment/TallowDoor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Player/Stealth/TallowStealthCharacter.h"
#include "Presentation/TallowRouteGate.h"
#include "Player/TallowCharacter.h"
#include "Sound/SoundBase.h"
#include "Audio/TallowKeeperVoiceComponent.h"
#include "Tasks/TallowFusePickup.h"

// The spider's own ways (25 Oct): the resting heap that unfolds when she comes close, hanging from the ceiling
// over a doorway and dropping, waiting behind the mouth of a crawl or on top of a cupboard, and the climb and crawl
// links it shares with the player (shortcuts, and a way to vanish and come out somewhere else).

namespace
{
    FString MarkerValue(const AActor* Actor, const TCHAR* Prefix)
    {
        for (const FName& Tag : Actor->Tags)
            if (Tag.ToString().StartsWith(Prefix)) return Tag.ToString().RightChop(FCString::Strlen(Prefix));
        return FString();
    }

    bool MarkerNormal(const AActor* Actor, FVector& Out)
    {
        TArray<FString> N;
        MarkerValue(Actor, TEXT("Normal=")).ParseIntoArray(N, TEXT(","));
        if (N.Num() != 3) return false;
        Out = FVector(FCString::Atof(*N[0]), FCString::Atof(*N[1]), FCString::Atof(*N[2])).GetSafeNormal();
        return !Out.IsNearlyZero();
    }

    bool WorldTrace(const UWorld* World, const AActor* A, const AActor* B, const FVector& From, const FVector& To, FHitResult& Hit)
    {
        FCollisionQueryParams Query(SCENE_QUERY_STAT(HunterSpider), true, A);
        if (B) Query.AddIgnoredActor(B);
        return World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Query);
    }

    void PlaySting(const UObject* Context)
    {
        if (USoundBase* Sting = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Creature/K_Mim_Capture_Sting.K_Mim_Capture_Sting")))
            UGameplayStatics::PlaySound2D(Context, Sting, .12f);
    }
}

FVector ATallowHunterAIController::HeadLocation() const
{
    if (!Creature) return FVector::ZeroVector;
    return Creature->HunterMotion && Creature->HunterMotion->HasInitialized()
        ? Creature->HunterMotion->GetEyeLocation() : Creature->GetPawnViewLocation();
}

bool ATallowHunterAIController::PlayerLooksAt(const FVector& Point, float MinDot, bool bNeedClear) const
{
    // Her own eyes and where she is looking (her view rotation), not whatever camera is rendering: a review camera
    // filming the lab counted as her staring, and her camera component only turns while it is the one rendering.
    if (!Player) return false;
    FVector Eye; FRotator View;
    Player->GetActorEyesViewPoint(Eye, View);
    const FVector Forward = View.Vector();
    if (FVector::DotProduct((Point - Eye).GetSafeNormal(), Forward) < MinDot) return false;
    return !bNeedClear || HasClearTrace(Eye, Point, Player);
}

bool ATallowHunterAIController::SensesPlayerClose(float Distance) const
{
    if (!Player || Player->IsHiddenInSpot()) return false;
    if (bVisible || SightConfidence > .3f) return true;
    // Her footsteps this close, unless she creeps; her breathing in the dark.
    if (Distance < 320.f && Player->GetVelocity().Size2D() > 120.f) return true;
    return IsDark() && Distance < 300.f && HearsBreathing(Distance);
}

void ATallowHunterAIController::CancelSpiderPlans()
{
    if (Creature && Creature->HunterTraversal && Creature->HunterTraversal->IsHolding())
    {
        if (PlannedHold == EHold::Ceiling || PlannedHold == EHold::Top) Creature->HunterTraversal->PounceToward(EvidenceLocation);
        else Creature->HunterTraversal->RequestGroundExit();
    }
    if (bDisguised && Creature)
    {
        Creature->HunterMotion->SetMound(false, true);
        if (Creature->Voice) Creature->Voice->SetDisguised(false);
    }
    bDisguisePending = bDisguiseArrived = bDisguised = false;
    bHoldPending = bBurstOut = false;
    PlannedHold = EHold::None;
    HoldLink = PendingLink = INDEX_NONE;
    bLinkFinalGoal = false;
    StaredFor = 0.f;
}

bool ATallowHunterAIController::UpdateSpiderAmbush(float Now, float DeltaSeconds)
{
    if (!Creature || !Player || !Creature->HunterTraversal) return false;
    auto* Traversal = Creature->HunterTraversal.Get();
    const FVector Me = Creature->GetActorLocation(), Her = Player->GetActorLocation();
    const float Distance = FVector::Dist(Me, Her), Flat = FVector::Dist2D(Me, Her);
    if (bBurstOut && !Traversal->IsTraversing())
    {
        // Out of the crawl mouth: now at her.
        bBurstOut = false;
        Rush(Her, 2.5f);
        LastDecision = TEXT("Revealed: out of the crawl");
        return false;
    }
    if (Traversal->IsHolding() && PlannedHold != EHold::None)
    {
        Creature->HunterMotion->LookAt(SensesPlayerClose(Distance) ? Player->GetPawnViewLocation() : AmbushFocus, .3f);
        ClosestWhileDisguised = FMath::Min(ClosestWhileDisguised, Flat);
        const bool bSensed = SensesPlayerClose(Distance);
        const bool bBlocked = BoardsBetween(HeadLocation(), Player->GetPawnViewLocation());
        const float Heading = FVector::DotProduct(Player->GetVelocity().GetSafeNormal2D(), (Her - Me).GetSafeNormal2D());
        if (!bBlocked && !Player->IsHiddenInSpot())
        {
            if (PlannedHold == EHold::Ceiling)
            {
                // Right under it, or just through and past with her back to it: down it comes. Unsensed it only drops
                // when she is all but touching it (a still child holding her breath in the dark is not found from
                // above: the dark-and-silent rule holds on the ceiling too).
                if (Flat < 100.f || (Flat < 140.f && bSensed) || (Flat < 300.f && Heading > .3f && bSensed))
                { ReleaseHold(Now, TEXT("CeilingDrop: dropped on her"), true); return true; }
                // She came near and turned back the way she came.
                if (bSensed && ClosestWhileDisguised < 600.f && Flat > ClosestWhileDisguised + 160.f)
                { ReleaseHold(Now, TEXT("CeilingDrop: she turned back"), true); return true; }
            }
            else if (PlannedHold == EHold::Crawl && Flat < 300.f && bSensed)
            { ReleaseHold(Now, TEXT("Revealed: bursting out of the crawl"), true); return true; }
            else if (PlannedHold == EHold::Top && Flat < 340.f && bSensed)
            { ReleaseHold(Now, TEXT("Pounce: off the top of the cupboard"), true); return true; }
        }
        // Spotted: she stands and stares straight at it.
        const bool bStill = UpdateClosing(Distance, DeltaSeconds) < 60.f;
        StaredFor = bStill && PlayerLooksAt(HeadLocation(), .96f) && Distance < 1000.f ? StaredFor + DeltaSeconds
            : FMath::Max(0.f, StaredFor - DeltaSeconds);
        if (StaredFor > 1.4f) { ReleaseHold(Now, TEXT("Spotted: coming down at her"), Distance < 700.f && !bBlocked); return true; }
        if (Now > HoldUntil && !PlayerLooksAt(Me, .6f)) { ReleaseHold(Now, TEXT("Hold: gave up waiting"), false); return true; }
        return true;
    }
    if (!bDisguisePending || !bDisguiseArrived) return false;
    StopMovement();
    const FVector Body = Me + FVector(0.f, 0.f, 10.f);
    if (!bDisguised)
    {
        // Arrived. It only ever settles where she cannot watch it happen.
        if (!PlayerLooksAt(Body, .5f)) SettleDisguise(Now);
        else if (Now > DisguiseSettleBy)
        {
            bDisguisePending = bDisguiseArrived = false;
            EnterMode(ETallowHuntMode::Search);
            bHasGoal = false; NextDecision = Now + .5f;
            LastDecision = TEXT("DisguiseAbandoned: she was watching");
            return false;
        }
        return true;
    }
    // Still and silent: no "seen" tell on her candle or breath while it lies there.
    if (auto* Stealth = Cast<ATallowStealthCharacter>(Player)) { Stealth->bSeenByHunter = false; Stealth->HunterAwareness = 0.f; }
    Player->KeeperSight = 0.f;
    Player->bSeenByKeeper = false;
    ClosestWhileDisguised = FMath::Min(ClosestWhileDisguised, Distance);
    const bool bStill = UpdateClosing(Distance, DeltaSeconds) < 60.f;
    StaredFor = bStill && PlayerLooksAt(Body, .97f) && Distance < 1100.f ? StaredFor + DeltaSeconds : FMath::Max(0.f, StaredFor - DeltaSeconds * .5f);
    const bool bSensed = SensesPlayerClose(Distance);
    const bool bBackTurned = !PlayerLooksAt(Body, .1f, false);
    if (Distance < 230.f && bSensed) { Unfurl(Now, true, TEXT("Revealed: she came close")); return false; }
    if (Distance < 480.f && bBackTurned && bSensed) { Unfurl(Now, true, TEXT("Revealed: her back was turned")); return false; }
    if (StaredFor > 2.2f) { Unfurl(Now, Distance < 450.f, TEXT("Revealed slowly: she stared at it")); return false; }
    if (ClosestWhileDisguised > 380.f && ClosestWhileDisguised < 950.f && Distance > ClosestWhileDisguised + 180.f
        && (bVisible || SightConfidence > .3f))
    { Unfurl(Now, false, TEXT("Revealed slowly: she gave it a wide berth")); return false; }
    if (Now > DisguiseUntil && !PlayerLooksAt(Body, .5f)) { Unfurl(Now, false, TEXT("Revealed slowly: tired of waiting")); return false; }
    return true;
}

float ATallowHunterAIController::UpdateClosing(float Distance, float DeltaSeconds)
{
    // How fast she is coming toward it (cm/s), from the distance itself, so a teleported review player counts too.
    if (PreviousAmbushDistance > 0.f && DeltaSeconds > SMALL_NUMBER)
        AmbushClosing = FMath::FInterpTo(AmbushClosing, (PreviousAmbushDistance - Distance) / DeltaSeconds, DeltaSeconds, 4.f);
    PreviousAmbushDistance = Distance;
    return AmbushClosing;
}

bool ATallowHunterAIController::HandleNoiseInAmbush(const FVector& Where, float Perceived, bool bLoud)
{
    // Lying in wait it still answers every sound. One coming its way is her walking into it: it holds still and
    // turns its attention. One far off pulls it out to go and see.
    const float Now = GetWorld()->GetTimeSeconds();
    const bool bWaiting = bDisguised || bDisguiseArrived || (Creature->HunterTraversal && Creature->HunterTraversal->IsHolding());
    const float Away = FVector::Dist2D(Creature->GetActorLocation(), Where);
    Observe(Where, FMath::Clamp(Perceived * 3.f, .2f, .9f), false);
    // A heap or a hold does not turn its head to a sound (the body's heard turn would give the shape away).
    if (!bWaiting) FireEvent(ETallowKeeperEvent::Heard);
    Creature->HunterMotion->LookAt(Where + FVector(0, 0, 55), 1.2f);
    // Her running round the house is not her walking into it (29 Oct: it sat waiting while he ran rings round
    // it). A covered wait breaks and comes for anything loud that is not right on top of it; a heap or a hold keeps
    // still for her unless she runs past well out of reach.
    if (bLoud && !bWaiting && Away > 300.f)
    {
        bTravellingToAmbush = false; AmbushUntil = 0.f;
        EnterMode(ETallowHuntMode::Investigate);
        LastDecision = TEXT("Heard her run: leaving the ambush");
        return false;
    }
    if (Away < (bLoud ? 700.f : 1100.f))
    {
        LastDecision = bWaiting ? TEXT("Heard her coming: holding still") : TEXT("Heard her: still going to wait for her");
        return true;
    }
    if (!bWaiting)
    {
        // Still on its way there: drop the plan and answer the sound the ordinary way.
        if (!Creature->HunterTraversal->IsTraversing()) { CancelSpiderPlans(); return false; }
        return true;
    }
    if (bDisguised || bDisguiseArrived) Unfurl(Now, false, TEXT("Revealed slowly: a sound far off"));
    else ReleaseHold(Now, TEXT("Hold: a sound far off, going to see"), false);
    EnterMode(ETallowHuntMode::Investigate);
    SetGoalNearest(Where);
    LastDecision = TEXT("Heard it far off: leaving the ambush to hunt");
    return true;
}

bool ATallowHunterAIController::TryDisguise(float Now)
{
    auto* Overseer = GetHuntDirector();
    if (!Creature || !Player || !bUseDirector || !bUseAmbushes || !Overseer || !Overseer->CanDisguise() || bVisible
        || bDisguisePending || PlannedHold != EHold::None || Now < NextDisguiseTry) return false;
    NextDisguiseTry = Now + 6.f;
    // It needs its own idea of where she is: a sound or a sighting in the last minute and a half.
    if (Now - FMath::Max(LastSeenAt, LastHeardAt) > 90.f || EvidenceLocation.IsZero()) return false;
    FVector Spot, Facing;
    if (!FindMoundSpot(Spot, Facing) || !SetGoal(Spot)) return false;
    Overseer->NoteDisguise();
    bDisguisePending = true; bDisguiseArrived = false;
    AmbushFocus = Facing;
    bTravellingToAmbush = true;
    AmbushUntil = Now + 60.f;
    SearchUntil = FMath::Max(SearchUntil, Now + 90.f);
    EnterMode(ETallowHuntMode::Ambush);
    SetPace(.85f, 40.f);
    LastDecision = TEXT("DisguiseAmbush: going to lie in wait on her way");
    return true;
}

bool ATallowHunterAIController::FindMoundSpot(FVector& Spot, FVector& Facing)
{
    // Where would she go from where it last knew her? To the fuse she still needs, else out by a door it can use.
    const FVector From = EvidenceLocation, Here = Creature->GetActorLocation();
    if (FVector::Dist2D(Here, From) < 500.f) return false;
    FVector Toward = FVector::ZeroVector;
    float Nearest = 2600.f;
    for (TActorIterator<ATallowFusePickup> It(GetWorld()); It; ++It)
    {
        const float D = FVector::Dist(It->GetActorLocation(), From);
        if (!It->IsHidden() && D > 300.f && D < Nearest) { Nearest = D; Toward = It->GetActorLocation(); }
    }
    if (Toward.IsZero())
    {
        CacheDoors();
        float Far = 0.f;
        for (const FDoorWatch& Watch : Doors)
        {
            if (!Watch.Door.IsValid() || IsBarred(Watch.Door.Get())) continue;
            const float D = FVector::Dist2D(Watch.Center, From), Score = FVector::Dist2D(Watch.Center, Here);
            if (D > 250.f && D < 1200.f && Score > Far) { Far = Score; Toward = Watch.Center; }
        }
    }
    if (Toward.IsZero() && !EvidenceHeading.IsNearlyZero()) Toward = From + EvidenceHeading * 900.f;
    if (Toward.IsZero()) return false;
    // Her likely route there, along the house's own walkways.
    TArray<FVector> Route;
    auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
    FNavLocation A, B;
    if (Nav && Nav->ProjectPointToNavigation(From, A, FVector(120, 120, 250)) && Nav->ProjectPointToNavigation(Toward, B, FVector(200, 200, 250)))
        if (const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(this, A.Location, B.Location, this);
            Path && Path->IsValid())
            Route = Path->PathPoints;
    if (Route.Num() < 2) Route = {From, Toward};
    auto OffRoute = [&Route](const FVector& P, FVector& Closest)
    {
        float Best = MAX_flt;
        for (int32 I = 0; I + 1 < Route.Num(); ++I)
        {
            const FVector C = FMath::ClosestPointOnSegment(P, Route[I], Route[I + 1]);
            const float D = FVector::Dist2D(P, C);
            if (D < Best) { Best = D; Closest = C; }
        }
        return Best;
    };
    TArray<TPair<FVector, float>> Candidates;
    // Marked spots first: among cushions, by a bed, a heap in the laundry.
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(TEXT("Tallow.Mound"))) Candidates.Add({It->GetActorLocation(), 160.f});
    // Then the corners of the rooms along her way, found from the walls themselves.
    float Walked = 0.f;
    for (int32 I = 0; I + 1 < Route.Num(); ++I)
    {
        const float Length = FVector::Dist(Route[I], Route[I + 1]);
        for (float T = 0.f; T < Length; T += 240.f)
        {
            Walked += 240.f;
            if (Walked < 300.f) continue;
            const FVector Floor = FMath::Lerp(Route[I], Route[I + 1], T / FMath::Max(Length, 1.f));
            const FVector P = Floor + FVector(0, 0, 60);
            for (const FVector2D Q : {FVector2D(1, 1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(-1, -1)})
            {
                const FVector X(Q.X, 0, 0), Y(0, Q.Y, 0);
                FHitResult HX, HY, CX, CY;
                if (!WorldTrace(GetWorld(), Creature, Player, P, P + X * 650.f, HX) || FMath::Abs(HX.ImpactNormal.X) < .9f
                    || !WorldTrace(GetWorld(), Creature, Player, P, P + Y * 650.f, HY) || FMath::Abs(HY.ImpactNormal.Y) < .9f) continue;
                const FVector Corner(HX.ImpactPoint.X - Q.X * 62.f, HY.ImpactPoint.Y - Q.Y * 62.f, Floor.Z);
                // A real corner: both walls run right up beside it, not a doorway or the end of a wall.
                const FVector C = Corner + FVector(0, 0, 60);
                if (!WorldTrace(GetWorld(), Creature, Player, C, C + X * 110.f, CX)
                    || !WorldTrace(GetWorld(), Creature, Player, C, C + Y * 110.f, CY)) continue;
                if (Candidates.ContainsByPredicate([&Corner](const TPair<FVector, float>& E) { return FVector::Dist2D(E.Key, Corner) < 80.f; })) continue;
                Candidates.Add({Corner, 0.f});
            }
        }
    }
    float BestScore = -MAX_flt;
    for (const TPair<FVector, float>& Candidate : Candidates)
    {
        const FVector& P = Candidate.Key;
        const float Bonus = Candidate.Value;
        const float FromHer = FVector::Dist2D(P, From);
        if (FromHer < 450.f || FromHer > 1800.f || FMath::Abs(P.Z - From.Z) > 200.f) continue;
        if (FVector::Dist2D(P, LastDisguiseSpot) < 400.f) continue;
        FVector Closest;
        const float Off = OffRoute(P, Closest);
        if (Off < 60.f || Off > 420.f) continue;
        // It must get there without walking past her.
        if (FMath::PointDistToSegment(From, Here, P) < 350.f) continue;
        FVector Projected; float PathLength;
        if (!IsReachable(P, Projected, &PathLength) || PathLength > 2600.f) continue;
        const float Score = Bonus - FMath::Abs(Off - 170.f) * .6f - PathLength * .05f + Random.FRandRange(0.f, 60.f);
        if (Score > BestScore) { BestScore = Score; Spot = Projected; Facing = Closest + FVector(0, 0, 60); }
    }
    return BestScore > -MAX_flt;
}

void ATallowHunterAIController::SettleDisguise(float Now)
{
    bDisguised = true;
    DisguiseUntil = Now + Random.FRandRange(45.f, 70.f);
    StaredFor = 0.f; ClosestWhileDisguised = MAX_flt; PreviousAmbushDistance = -1.f; AmbushClosing = 0.f;
    StopMovement(); bHasGoal = false;
    // Facing the way she will come (unseen, so the turn is never watched).
    FVector Toward = AmbushFocus - Creature->GetActorLocation(); Toward.Z = 0.f;
    if (!Toward.IsNearlyZero()) Creature->SetActorRotation(FRotator(0.f, Toward.Rotation().Yaw, 0.f));
    Creature->HunterMotion->SetMound(true);
    // The heap lures softly while she is near ("Hi, are you lost?").
    if (Creature->Voice) Creature->Voice->SetDisguised(true);
    LastDecision = TEXT("DisguiseAmbush: settled as a harmless heap");
}

void ATallowHunterAIController::Unfurl(float Now, bool bAttack, const TCHAR* Why)
{
    bDisguisePending = bDisguiseArrived = bDisguised = false;
    StaredFor = 0.f;
    Creature->HunterMotion->SetMound(false, bAttack);
    if (Creature->Voice) Creature->Voice->SetDisguised(false);
    LastDisguiseSpot = Creature->GetActorLocation();
    bTravellingToAmbush = false; AmbushUntil = 0.f;
    FireEvent(ETallowKeeperEvent::Reveal);
    if (bAttack && Player)
    {
        if (auto* Overseer = GetHuntDirector()) Overseer->NoteScare();
        if (PlayerLooksAt(HeadLocation(), .45f, false)) PlaySting(this);
        Rush(Player->GetActorLocation(), 2.5f);
    }
    else
    {
        // Slowly up, then after her at a creep: for a few seconds the sight of her keeps it creeping, not charging.
        if (Player && (bVisible || SightConfidence > .3f)) Observe(Player->GetActorLocation(), .8f, true);
        StalkSightUntil = Now + 5.f;
        SetPace(.55f, 20.f);
        EnterMode(ETallowHuntMode::Investigate);
        SetGoalNearest(EvidenceLocation);
        Creature->HunterMotion->PauseAndLook(EvidenceLocation + FVector(0, 0, 60), 1.2f);
    }
    LastDecision = Why;
}

bool ATallowHunterAIController::PerchOver(const FVector& Where, const FVector& Focus, const TCHAR* Why)
{
    if (!Creature || !bUseSurfaceRoutes || !Creature->HunterTraversal || Creature->HunterTraversal->IsTraversing()) return false;
    const float Now = GetWorld()->GetTimeSeconds();
    // Hanging, the body reaches about 1.5 m down from the ceiling; she is 1.35 m tall and must pass under it.
    if (Creature->HunterTraversal->ClearanceAt(Where) < 290.f) { LastDecision = TEXT("CeilingTooLowToHang"); return false; }
    PlannedHold = EHold::Ceiling; HoldTarget = Where; HoldLink = INDEX_NONE;
    AmbushFocus = Focus;
    const FVector Here = Creature->GetActorLocation();
    const float Away = FVector::Dist2D(Here, Where);
    bool bStarted = Away > 300.f && Away < 1300.f && FMath::Abs(Where.Z - Here.Z) < 200.f && StartHold();
    if (!bStarted)
    {
        // Walk to where a climb can start, a few metres short on its own side, then go up.
        PlannedHold = EHold::Ceiling;
        const FVector Stage = Where + (Here - Where).GetSafeNormal2D() * 520.f;
        FVector Projected;
        if (!IsReachable(Stage, Projected) || !SetGoal(Projected)) { PlannedHold = EHold::None; return false; }
        bHoldPending = true;
    }
    bTravellingToAmbush = bHoldPending;
    EnterMode(ETallowHuntMode::Ambush);
    HoldUntil = FMath::Max(HoldUntil, Now + 30.f);
    AmbushUntil = Now + 60.f;
    SearchUntil = FMath::Max(SearchUntil, AmbushUntil + 5.f);
    SetPace(.8f, 20.f);
    LastDecision = Why;
    return true;
}

bool ATallowHunterAIController::LurkInLink(const FVector& Where)
{
    CacheLinks();
    if (!Creature || Links.IsEmpty()) return false;
    int32 Best = INDEX_NONE; bool bBestReverse = false; float BestScore = -MAX_flt; FVector BestEntry;
    for (int32 I = 0; I < Links.Num(); ++I)
        for (const bool bReverse : {false, true})
        {
            const FLink& Link = Links[I];
            if (!LinkOpen(I)) continue;
            const FVector Entry = bReverse ? Link.Points.Last() : Link.Points[0];
            const FVector End = bReverse ? Link.Points[0] : Link.Points.Last();
            // A crawl: in from the far end, to wait behind the mouth nearest the sound. A climb: up, to wait on top.
            if (!Link.bCrawl && End.Z < Entry.Z + 80.f) continue;
            const float FromSound = FVector::Dist2D(End, Where);
            if (FromSound > 750.f || FromSound < 120.f || FMath::Abs(End.Z - Where.Z) > (Link.bCrawl ? 160.f : 320.f)) continue;
            if (Link.bCrawl && FVector::Dist2D(Entry, Where) < FromSound + 150.f) continue;
            FVector Projected; float Path;
            if (!IsReachable(Entry, Projected, &Path) || Path > 2200.f) continue;
            const float Score = -FromSound * .4f - Path * .1f + Random.FRandRange(0.f, 80.f);
            if (Score > BestScore) { BestScore = Score; Best = I; bBestReverse = bReverse; BestEntry = Projected; }
        }
    if (Best == INDEX_NONE || !SetGoal(BestEntry)) return false;
    const float Now = GetWorld()->GetTimeSeconds();
    PlannedHold = Links[Best].bCrawl ? EHold::Crawl : EHold::Top;
    HoldLink = Best; bHoldLinkReverse = bBestReverse; bHoldPending = true;
    AmbushFocus = Where + FVector(0, 0, 40);
    bTravellingToAmbush = true;
    AmbushUntil = Now + 60.f;
    SearchUntil = FMath::Max(SearchUntil, AmbushUntil + 5.f);
    EnterMode(ETallowHuntMode::Ambush);
    SetPace(.8f, 25.f);
    LastDecision = PlannedHold == EHold::Crawl ? TEXT("Lurk: round to the crawl, to wait behind its mouth")
        : TEXT("Lurk: to the climb, to wait up on top");
    return true;
}

bool ATallowHunterAIController::TrySpiderAmbush(const FVector& Where)
{
    // Its own ways of waiting for her: hanging from the ceiling just past the way she would leave, or behind the
    // mouth of a crawl, or on top of a cupboard near the sound.
    if (Random.FRand() < .7f)
    {
        if (LurkAtMarker(Where)) return true;
        CacheDoors();
        const FVector Here = Creature->GetActorLocation();
        const FDoorWatch* Pick = nullptr; float PickScore = -MAX_flt;
        for (const FDoorWatch& Watch : Doors)
        {
            if (!Watch.Door.IsValid() || IsBarred(Watch.Door.Get()) || DoorOpenness(Watch) < .3f) continue;
            const float FromSound = FVector::Dist2D(Watch.Center, Where);
            if (FromSound > 900.f || FromSound < 120.f) continue;
            const float Score = FVector::Dist2D(Watch.Center, Here) * .4f - FromSound * .3f + Random.FRandRange(0.f, 80.f);
            if (Score > PickScore) { PickScore = Score; Pick = &Watch; }
        }
        // Over the far side of the doorway: she comes through and walks under it.
        if (Pick && PerchOver(Pick->Center + (Pick->Center - Where).GetSafeNormal2D() * 110.f, Pick->Center + FVector(0, 0, 40),
            TEXT("CeilingAmbush: to hang over the way out"))) return true;
    }
    return LurkInLink(Where);
}

bool ATallowHunterAIController::StartHold()
{
    auto* Traversal = Creature->HunterTraversal.Get();
    const bool bStarted = PlannedHold == EHold::Ceiling ? Traversal->StartChaseSurfaceRoute(HoldTarget, true)
        : HoldLink != INDEX_NONE && BuildLinkRoute(HoldLink, bHoldLinkReverse, true);
    bHoldPending = false;
    if (!bStarted)
    {
        LastDecision = FName(*(FString(TEXT("HoldFailed: ")) + Traversal->LastRouteFailure.Left(90)));
        PlannedHold = EHold::None; HoldLink = INDEX_NONE;
        return false;
    }
    StopMovement();
    bTravellingToAmbush = false;
    const float Now = GetWorld()->GetTimeSeconds();
    HoldUntil = Now + Random.FRandRange(30.f, 45.f);
    AmbushUntil = HoldUntil + 30.f;
    SearchUntil = FMath::Max(SearchUntil, AmbushUntil + 5.f);
    StaredFor = 0.f; ClosestWhileDisguised = MAX_flt; PreviousAmbushDistance = -1.f; AmbushClosing = 0.f;
    LastDecision = PlannedHold == EHold::Ceiling ? TEXT("CeilingAmbush: up the wall and over")
        : PlannedHold == EHold::Crawl ? TEXT("Lurk: in through the far end of the crawl") : TEXT("Lurk: climbing up on top");
    return true;
}

void ATallowHunterAIController::ReleaseHold(float Now, const TCHAR* Why, bool bAttack)
{
    auto* Traversal = Creature->HunterTraversal.Get();
    const EHold Hold = PlannedHold;
    PlannedHold = EHold::None; StaredFor = 0.f;
    const FVector Her = Player ? Player->GetActorLocation() : EvidenceLocation;
    if (bAttack)
    {
        if (auto* Overseer = GetHuntDirector()) Overseer->NoteScare();
        FireEvent(ETallowKeeperEvent::Reveal);
        if (PlayerLooksAt(HeadLocation(), .45f, false) && FVector::Dist(Her, Creature->GetActorLocation()) < 1000.f) PlaySting(this);
    }
    if (Hold == EHold::Crawl && bAttack && Links.IsValidIndex(HoldLink))
    {
        // On out of the mouth it waited behind, then at her.
        const FLink& Link = Links[HoldLink];
        const FVector Mouth = bHoldLinkReverse ? Link.Points[0] : Link.Points.Last();
        const FVector Inner = bHoldLinkReverse ? Link.Points[1] : Link.Points[Link.Points.Num() - 2];
        const FVector Out = (Mouth - Inner).GetSafeNormal2D();
        FHitResult Floor;
        const FVector Exit = WorldTrace(GetWorld(), Creature, Player, Mouth + FVector(0, 0, 40), Mouth - FVector(0, 0, 120), Floor)
            ? Floor.ImpactPoint : Mouth;
        EnterMode(ETallowHuntMode::Pursue);
        if (Traversal->ResumeHeld({Exit, Exit + Out * 70.f}, {FVector::UpVector, FVector::UpVector})) bBurstOut = true;
        else { Traversal->RequestGroundExit(); bBurstOut = true; }
    }
    else if (bAttack)
    {
        // Down in front of her, in her way (never onto her head): ahead of where she is walking, else toward her.
        FVector Ahead = Player ? Player->GetVelocity().GetSafeNormal2D() : FVector::ZeroVector;
        if (Ahead.IsNearlyZero()) Ahead = (Her - Creature->GetActorLocation()).GetSafeNormal2D();
        if (Ahead.IsNearlyZero()) Ahead = Creature->GetActorForwardVector().GetSafeNormal2D();
        Traversal->PounceToward(Hold == EHold::Ceiling ? Her + Ahead * 120.f : Her);
        Rush(Her, 2.5f);
    }
    else
    {
        // Back down the way it came, quietly, and on with the search.
        Traversal->RequestGroundExit();
        EnterMode(ETallowHuntMode::Search);
        bHasGoal = false; NextDecision = Now + .5f;
    }
    HoldLink = INDEX_NONE;
    LastDecision = Why;
}

void ATallowHunterAIController::CacheLinks()
{
    if (bLinksCached) return;
    bLinksCached = true;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(TEXT("Tallow.Lurk")))
        {
            FLurk Lurk;
            FVector N;
            Lurk.Location = It->GetActorLocation();
            Lurk.bCeiling = MarkerNormal(*It, N) && N.Z < -.5f;
            const FString Watch = MarkerValue(*It, TEXT("Watch="));
            int32 Count = 0;
            if (!Watch.IsEmpty())
                for (TActorIterator<AActor> Mouth(GetWorld()); Mouth; ++Mouth)
                    if (MarkerValue(*Mouth, TEXT("Link=")) == Watch) { Lurk.Watch += Mouth->GetActorLocation(); ++Count; }
            if (Count) Lurk.Watch /= Count;
            Lurks.Add(Lurk);
        }
    for (const int32 Kind : {0, 1, 2})
    {
        const bool bCrawl = Kind > 0, bPlayerCrawl = Kind == 2;
        const FName Tag = Kind == 0 ? FName(TEXT("Tallow.Climb")) : Kind == 1 ? FName(TEXT("Tallow.Crawl")) : FName(TEXT("Tallow.PlayerCrawl"));
        TMap<FString, TArray<AActor*>> Grouped;
        TArray<AActor*> Loose;
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
            if (It->ActorHasTag(Tag))
            {
                const FString Id = MarkerValue(*It, TEXT("Link="));
                if (Id.IsEmpty()) Loose.Add(*It); else Grouped.FindOrAdd(Id).Add(*It);
            }
        // Unlabelled markers pair with their nearest partner: the foot and top of one climb, the two mouths of a crawl.
        int32 Pairs = 0;
        while (Loose.Num() >= 2)
        {
            int32 A = 0, B = 1; float Best = MAX_flt;
            for (int32 I = 0; I < Loose.Num(); ++I)
                for (int32 J = I + 1; J < Loose.Num(); ++J)
                {
                    const float D = FVector::Dist(Loose[I]->GetActorLocation(), Loose[J]->GetActorLocation());
                    if (D < Best) { Best = D; A = I; B = J; }
                }
            if (Best > 1500.f) break;
            Grouped.Add(FString::Printf(TEXT("~%d"), Pairs++), {Loose[A], Loose[B]});
            Loose.RemoveAt(B); Loose.RemoveAt(A);
        }
        for (auto& Group : Grouped)
        {
            TArray<AActor*>& Marks = Group.Value;
            if (Marks.Num() < 2) continue;
            Marks.Sort([bCrawl](const AActor& X, const AActor& Y)
            {
                const FString OX = MarkerValue(&X, TEXT("Order=")), OY = MarkerValue(&Y, TEXT("Order="));
                if (!OX.IsEmpty() && !OY.IsEmpty()) return FCString::Atoi(*OX) < FCString::Atoi(*OY);
                const FString EX = MarkerValue(&X, TEXT("End=")), EY = MarkerValue(&Y, TEXT("End="));
                if (!EX.IsEmpty() && !EY.IsEmpty()) return EX < EY;
                return bCrawl ? X.GetName() < Y.GetName() : X.GetActorLocation().Z < Y.GetActorLocation().Z;
            });
            FLink Link;
            Link.bCrawl = bCrawl; Link.bPlayerCrawl = bPlayerCrawl; Link.bExplicit = true;
            // A power-locked portal is shut to it too until the gate swings open.
            for (const AActor* Mark : Marks)
                if (const FString GateName = MarkerValue(Mark, TEXT("Gate=")); !GateName.IsEmpty())
                    for (TActorIterator<ATallowRouteGate> Gate(GetWorld()); Gate; ++Gate)
                    {
                        bool bMatch = Gate->GetName() == GateName;
#if WITH_EDITOR
                        bMatch |= Gate->GetActorLabel() == GateName;
#endif
                        if (bMatch) Link.Gate = *Gate;
                    }
            for (const AActor* Mark : Marks)
            {
                FVector N = FVector::ZeroVector;
                if (!MarkerNormal(Mark, N)) Link.bExplicit = false;
                if (!Link.Points.IsEmpty()) Link.Length += FVector::Dist(Link.Points.Last(), Mark->GetActorLocation());
                Link.Points.Add(Mark->GetActorLocation());
                Link.Normals.Add(N);
            }
            if (Link.bCrawl && Creature && Creature->HunterTraversal)
            {
                float Height = 400.f;
                float Width = 400.f;
                for (const AActor* Mark : Marks)
                {
                    const float Gap = Creature->HunterTraversal->ClearanceAt(Mark->GetActorLocation());
                    if (Gap > 0.f) Height = FMath::Min(Height, Gap);
                    TArray<FString> Dimensions;
                    MarkerValue(Mark, TEXT("Clear=")).ParseIntoArray(Dimensions, TEXT("x"));
                    if (Dimensions.Num() == 2)
                    {
                        Width = FMath::Min(Width, FCString::Atof(*Dimensions[0]));
                        Height = FMath::Min(Height, FCString::Atof(*Dimensions[1]));
                    }
                }
                const float Fold = FMath::Max(FMath::Clamp((180.f - Height) / 122.f, 0.f, 1.f),
                    FMath::Clamp((160.f - Width) / 102.f, 0.f, 1.f));
                Link.EstimatedSpeedScale = FMath::Lerp(1.f, .16f, Fold);
            }
            Links.Add(MoveTemp(Link));
        }
    }
}

bool ATallowHunterAIController::BuildLinkRoute(int32 Index, bool bReverse, bool bHold)
{
    if (!Links.IsValidIndex(Index) || !Creature || !Creature->HunterTraversal) return false;
    auto* Traversal = Creature->HunterTraversal.Get();
    const FLink& Link = Links[Index];
    TArray<FVector> P = Link.Points, N = Link.Normals;
    if (bReverse) { Algo::Reverse(P); Algo::Reverse(N); }
    const float Standing = Traversal->GetStandingHalfHeight(), StandingRadius = Traversal->GetStandingRadius();
    const FVector Up = FVector::UpVector;
    // The tightest gap on the way sets how flat it goes (flat enough, it is a ball of about 52 cm).
    float HalfHeight = -1.f;
    auto FitTo = [&](float Gap)
    {
        if (Gap >= Standing * 2.f + 8.f) return;
        HalfHeight = FMath::Min(HalfHeight > 0.f ? HalfHeight : Standing, FMath::Max(26.f, Gap * .5f - 5.f));
    };
    float Tightest = 400.f;
    if (Link.bCrawl)
    {
        for (int32 I = 0; I + 1 < P.Num(); ++I)
            for (float T = 0.f; T <= 1.001f; T += .1f)
            {
                const float Gap = Traversal->ClearanceAt(FMath::Lerp(P[I], P[I + 1], T));
                if (Gap > 0.f) { FitTo(Gap); Tightest = FMath::Min(Tightest, Gap); }
            }
    }
    else for (const FVector& End : {P[0], P.Last()})
    {
        const float Gap = Traversal->ClearanceAt(End);
        if (Gap > 0.f) { FitTo(Gap); Tightest = FMath::Min(Tightest, Gap); }
    }
    if (Tightest < 26.f * 2.f + 4.f) { LastDecision = TEXT("LinkTooTight"); return false; }
    const float HH = HalfHeight > 0.f ? HalfHeight : Standing;
    const float Radius = FMath::Min(StandingRadius, HH);
    TArray<FVector> Surface, Normals;
    if (Link.bExplicit) { Surface = P; Normals = N; }
    else if (Link.bCrawl)
    {
        for (const FVector& Point : P)
        {
            FHitResult Floor;
            if (!WorldTrace(GetWorld(), Creature, Player, Point + Up * 40.f, Point - Up * 120.f, Floor)) return false;
            Surface.Add(Floor.ImpactPoint); Normals.Add(Up);
        }
        // Waiting in a crawl: behind its mouth, not out in the room.
        if (bHold) Surface.Last() -= (Surface.Last() - Surface[Surface.Num() - 2]).GetSafeNormal2D() * 90.f;
    }
    else
    {
        // A ledge or a cupboard: the foot on the floor in front of it, the top on it. The face between them is found.
        const bool bUp = P.Last().Z > P[0].Z;
        const FVector Low = bUp ? P[0] : P.Last(), High = bUp ? P.Last() : P[0];
        FHitResult LowFloor, HighFloor, Face;
        if (!WorldTrace(GetWorld(), Creature, Player, Low + Up * 40.f, Low - Up * 120.f, LowFloor)
            || !WorldTrace(GetWorld(), Creature, Player, High + Up * 40.f, High - Up * 120.f, HighFloor)) return false;
        const float FloorZ = LowFloor.ImpactPoint.Z, TopZ = HighFloor.ImpactPoint.Z;
        const FVector Probe(Low.X, Low.Y, FloorZ + FMath::Min(90.f, (TopZ - FloorZ) * .5f));
        const FVector Across = (High - Low).GetSafeNormal2D();
        bool bFace = !Across.IsNearlyZero() && WorldTrace(GetWorld(), Creature, Player, Probe, Probe + Across * 300.f, Face)
            && FMath::Abs(Face.ImpactNormal.Z) < .3f;
        if (!bFace)
        {
            float Near = MAX_flt;
            for (const FVector& Axis : {FVector::ForwardVector, -FVector::ForwardVector, FVector::RightVector, -FVector::RightVector})
            {
                FHitResult Hit;
                if (WorldTrace(GetWorld(), Creature, Player, Probe, Probe + Axis * 220.f, Hit) && FMath::Abs(Hit.ImpactNormal.Z) < .3f
                    && Hit.Distance < Near) { Near = Hit.Distance; Face = Hit; bFace = true; }
            }
        }
        if (!bFace) { LastDecision = TEXT("LinkNoClimbFace"); return false; }
        const FVector WN = Face.ImpactNormal.GetSafeNormal2D();
        const FVector Wall(Face.ImpactPoint.X, Face.ImpactPoint.Y, 0.f);
        const auto At = [&Wall](float Z) { return FVector(Wall.X, Wall.Y, Z); };
        // On top, at least a body's width in from the edge.
        FVector TopPoint = HighFloor.ImpactPoint;
        if (FVector::DotProduct(TopPoint - At(TopZ), -WN) < Radius + 15.f) TopPoint = At(TopZ) - WN * (Radius + 15.f);
        Surface = {At(FloorZ) + WN * (HH + 28.f), At(FloorZ + HH + 15.f), At(TopZ - 40.f), At(TopZ), TopPoint};
        Normals = {Up, WN, WN, (WN + Up).GetSafeNormal(), Up};
        if (!bUp) { Algo::Reverse(Surface); Algo::Reverse(Normals); }
    }
    if (!Traversal->StartLinkRoute(Surface, Normals, Link.bCrawl || HalfHeight > 0.f, HalfHeight, bHold))
    {
        LastDecision = FName(*(FString(TEXT("LinkRouteFailed: ")) + Traversal->LastRouteFailure.Left(90)));
        return false;
    }
    return true;
}

bool ATallowHunterAIController::LinkOpen(int32 Index) const
{
    if (!Links.IsValidIndex(Index)) return false;
    if (!Links[Index].Gate.IsValid()) return true;
    const FGateWatch Watch{Links[Index].Gate, FVector::ZeroVector};
    return GateOpenness(Watch) >= .75f;
}

bool ATallowHunterAIController::LurkAtMarker(const FVector& Where)
{
    // The spots the house offers: the floor just past the far mouth of one of her crawls (she comes out into it),
    // or the ceiling just inside a doorway near the sound.
    CacheLinks();
    if (!Creature || Lurks.IsEmpty()) return false;
    const FLurk* Best = nullptr; float BestScore = -MAX_flt; FVector BestFloor;
    for (const FLurk& Lurk : Lurks)
    {
        const float FromSound = FVector::Dist2D(Lurk.Location, Where);
        if (FromSound > 1000.f || FromSound < 150.f || FMath::Abs(Lurk.Location.Z - Where.Z) > 400.f) continue;
        FVector Floor = Lurk.Location;
        if (Lurk.bCeiling)
        {
            FHitResult Hit;
            if (!WorldTrace(GetWorld(), Creature, Player, Lurk.Location - FVector(0, 0, 20), Lurk.Location - FVector(0, 0, 500), Hit)) continue;
            Floor = Hit.ImpactPoint;
        }
        FVector Projected; float Path;
        if (!IsReachable(Floor, Projected, &Path) || Path > 2400.f) continue;
        const float Score = (Lurk.Watch.IsZero() ? 0.f : 120.f) - FromSound * .3f - Path * .08f + Random.FRandRange(0.f, 90.f);
        if (Score > BestScore) { BestScore = Score; Best = &Lurk; BestFloor = Lurk.bCeiling ? Floor : Projected; }
    }
    if (!Best) return false;
    if (Best->bCeiling)
        return PerchOver(BestFloor + FVector(0, 0, 50), Where + FVector(0, 0, 40), TEXT("CeilingAmbush: to the ceiling inside the doorway"));
    if (!SetGoal(BestFloor)) return false;
    const float Now = GetWorld()->GetTimeSeconds();
    AmbushFocus = (Best->Watch.IsZero() ? Where : Best->Watch) + FVector(0, 0, 30);
    bTravellingToAmbush = true;
    AmbushUntil = Now + 40.f;
    SearchUntil = FMath::Max(SearchUntil, AmbushUntil + 5.f);
    EnterMode(ETallowHuntMode::Ambush);
    SetPace(.8f, 25.f);
    LastDecision = Best->Watch.IsZero() ? TEXT("Lurk: at a waiting spot near the sound") : TEXT("Ambush: waiting where her crawl comes out");
    return true;
}

bool ATallowHunterAIController::HeadForLink(int32 Index, bool bReverse, const TCHAR* Why)
{
    if (!Links.IsValidIndex(Index)) return false;
    const FVector Entry = bReverse ? Links[Index].Points.Last() : Links[Index].Points[0];
    const FVector Final = Goal;
    const bool bHadGoal = bHasGoal;
    if (!SetGoal(Entry)) return false;
    PendingLink = Index; bPendingLinkReverse = bReverse;
    LinkFinalGoal = Final; bLinkFinalGoal = bHadGoal;
    LastDecision = Why;
    return true;
}

bool ATallowHunterAIController::TryLinkShortcut(float Now, bool bRecovery)
{
    if ((!bRecovery && Now < NextLinkThought) || !bHasGoal || (bVisible && !bRecovery) || PendingLink != INDEX_NONE || bOnLink
        || PlannedHold != EHold::None || bDisguisePending || HuntMode == ETallowHuntMode::Ambush) return false;
    NextLinkThought = Now + 1.5f;
    CacheLinks();
    if (Links.IsEmpty()) return false;
    const FVector Here = Creature->GetActorLocation();
    const float Straight = FVector::Dist(Here, Goal);
    if (Straight < (bRecovery ? 250.f : 450.f)) return false;
    FVector Unused; float Direct = 0.f;
    if (!IsReachable(Goal, Unused, &Direct)) Direct = 100000.f;
    auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
    int32 Best = INDEX_NONE; bool bBestReverse = false;
    float BestCost = bRecovery ? MAX_flt : Direct * .7f - 150.f;
    for (int32 I = 0; I < Links.Num(); ++I)
        for (const bool bReverse : {false, true})
        {
            const FLink& Link = Links[I];
            if (const float* Until = LinkRetryAfter.Find(I); Until && *Until > Now) continue;
            if (!LinkOpen(I)) continue;
            const FVector Entry = bReverse ? Link.Points.Last() : Link.Points[0];
            const FVector Exit = bReverse ? Link.Points[0] : Link.Points.Last();
            if (FVector::Dist(Here, Entry) > 1200.f || FVector::Dist(Exit, Goal) > Straight - 200.f) continue;
            FVector Projected; float ToEntry;
            if (!IsReachable(Entry, Projected, &ToEntry)) continue;
            FNavLocation Out;
            if (!Nav || !Nav->ProjectPointToNavigation(Exit, Out, FVector(150, 150, 200))) continue;
            const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(this, Out.Location, Goal, this);
            if (!Path || !Path->IsValid() || Path->IsPartial() || PathCrossesShut(Path->PathPoints)) continue;
            const float Cost = ToEntry + Link.Length / FMath::Max(.16f, Link.EstimatedSpeedScale)
                + FMath::Max(Path->GetPathLength(), Path->GetPathCost());
            if (Cost < BestCost) { BestCost = Cost; Best = I; bBestReverse = bReverse; }
        }
    if (Best == INDEX_NONE) return false;
    return HeadForLink(Best, bBestReverse, Links[Best].bCrawl ? TEXT("Shortcut: through the crawl") : TEXT("Shortcut: up the climb"));
}

bool ATallowHunterAIController::TryLinkEscape(float Now)
{
    // Gone where she cannot follow: into a crawl or up a chute, and out somewhere else entirely.
    CacheLinks();
    if (!Creature || Links.IsEmpty() || PendingLink != INDEX_NONE || bOnLink) return false;
    int32 Best = INDEX_NONE; bool bBestReverse = false; float BestScore = -MAX_flt;
    for (int32 I = 0; I < Links.Num(); ++I)
        for (const bool bReverse : {false, true})
        {
            const FLink& Link = Links[I];
            if (!LinkOpen(I)) continue;
            // The top of a cupboard is no way out.
            if (!Link.bCrawl && FMath::Abs(Link.Points.Last().Z - Link.Points[0].Z) < 250.f) continue;
            const FVector Entry = bReverse ? Link.Points.Last() : Link.Points[0];
            const FVector Exit = bReverse ? Link.Points[0] : Link.Points.Last();
            FVector Projected; float Path;
            if (!IsReachable(Entry, Projected, &Path) || Path > 1400.f) continue;
            const float Score = FVector::Dist(Exit, EvidenceLocation) - FVector::Dist(Entry, EvidenceLocation) * .3f - Path * .5f
                + Random.FRandRange(0.f, 100.f);
            if (Score > BestScore) { BestScore = Score; Best = I; bBestReverse = bReverse; }
        }
    if (Best == INDEX_NONE || !HeadForLink(Best, bBestReverse, Links[Best].bCrawl
        ? TEXT("Escape: into the crawl, out somewhere else") : TEXT("Escape: up and away, out somewhere else"))) return false;
    // Off the far end it carries on from wherever it comes out.
    bLinkFinalGoal = false;
    return true;
}

bool ATallowHunterAIController::UpdateLinks(float Now)
{
    auto* Traversal = Creature->HunterTraversal.Get();
    if (!Traversal || Traversal->IsTraversing()) return false;
    if (bOnLink)
    {
        bOnLink = false;
        if (bLinkFinalGoal) { bLinkFinalGoal = false; SetGoalNearest(LinkFinalGoal); }
        else { bHasGoal = false; NextDecision = Now; }
        LastDecision = TEXT("Off the link: carrying on");
        return false;
    }
    const FVector Here = Creature->GetActorLocation();
    if (PendingLink != INDEX_NONE)
    {
        if (!Links.IsValidIndex(PendingLink)) { PendingLink = INDEX_NONE; return false; }
        const FVector Entry = bPendingLinkReverse ? Links[PendingLink].Points.Last() : Links[PendingLink].Points[0];
        if (FVector::Dist2D(Here, Entry) < 140.f && FMath::Abs(Here.Z - Entry.Z) < 160.f)
        {
            const int32 Index = PendingLink;
            PendingLink = INDEX_NONE;
            StopMovement();
            if (BuildLinkRoute(Index, bPendingLinkReverse, false))
            {
                bOnLink = true;
                LastDecision = Links[Index].bCrawl ? TEXT("Into the crawl") : TEXT("Up the climb");
                return true;
            }
            LinkRetryAfter.Add(Index, Now + 12.f);
            if (bLinkFinalGoal) { bLinkFinalGoal = false; SetGoalNearest(LinkFinalGoal); }
        }
        return false;
    }
    if (bHoldPending)
    {
        const bool bAtStart = PlannedHold == EHold::Ceiling ? FVector::Dist2D(Here, Goal) < 70.f
            : Links.IsValidIndex(HoldLink)
                && FVector::Dist2D(Here, bHoldLinkReverse ? Links[HoldLink].Points.Last() : Links[HoldLink].Points[0]) < 140.f;
        if (bAtStart)
        {
            if (StartHold()) return true;
            EnterMode(ETallowHuntMode::Search);
            bHasGoal = false; NextDecision = Now + .5f;
        }
    }
    return false;
}

bool ATallowHunterAIController::ReviewDisguiseAt(const FVector& Where)
{
    if (!Creature) return false;
    CancelSpiderPlans();
    StopMovement();
    FVector Spot = Where;
    FNavLocation Floor;
    if (const auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld()); Nav && Nav->ProjectPointToNavigation(Where, Floor, FVector(150, 150, 250)))
        Spot = Floor.Location;
    Creature->SetActorLocation(Spot + FVector(0, 0, Creature->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f),
        false, nullptr, ETeleportType::TeleportPhysics);
    EnterMode(ETallowHuntMode::Ambush);
    bDisguisePending = bDisguiseArrived = true;
    bTravellingToAmbush = false;
    AmbushFocus = Player ? Player->GetActorLocation() : Spot;
    SettleDisguise(GetWorld()->GetTimeSeconds());
    Creature->HunterMotion->SnapFit();
    DisguiseUntil += 60.f;
    return true;
}

bool ATallowHunterAIController::ReviewPerchOver(const FVector& Where)
{
    CancelSpiderPlans();
    return PerchOver(Where, Where + FVector(0, 0, 40), TEXT("CeilingAmbush: to hang over the spot"));
}

bool ATallowHunterAIController::ReviewTakeLink(int32 Index, bool bReverse)
{
    CacheLinks();
    if (!Links.IsValidIndex(Index)) return false;
    CancelSpiderPlans();
    EnterMode(ETallowHuntMode::Roam);
    bHasGoal = false;
    return HeadForLink(Index, bReverse, Links[Index].bCrawl ? TEXT("Review: to the crawl") : TEXT("Review: to the climb"));
}

FString ATallowHunterAIController::DescribeLinks()
{
    CacheLinks();
    FString Out;
    for (int32 I = 0; I < Links.Num(); ++I)
    {
        const FLink& Link = Links[I];
        Out += FString::Printf(TEXT("%d %s%s points=%d length=%.0f from=%s to=%s; "), I,
            Link.bPlayerCrawl ? TEXT("her-crawl") : Link.bCrawl ? TEXT("crawl") : TEXT("climb"),
            Link.bExplicit ? TEXT(" explicit") : TEXT(""), Link.Points.Num(), Link.Length,
            *Link.Points[0].ToCompactString(), *Link.Points.Last().ToCompactString());
    }
    for (const FLurk& Lurk : Lurks)
        Out += FString::Printf(TEXT("lurk %s at %s; "), Lurk.bCeiling ? TEXT("ceiling") : TEXT("floor"), *Lurk.Location.ToCompactString());
    return Out;
}
