#include "AI/Hunter/TallowHunterAIController.h"
#include "AI/Hunter/TallowHunterBrain.h"
#include "AI/Hunter/TallowHunterCreature.h"
#include "AI/Hunter/TallowHunterDirector.h"
#include "AI/Hunter/TallowHunterMotionComponent.h"
#include "AI/Hunter/TallowHunterSearchAnchor.h"
#include "AI/Hunter/TallowHunterTraversalComponent.h"
#include "Audio/TallowKeeperVoiceComponent.h"
#include "Candle/TallowCandleComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Environment/TallowDoor.h"
#include "Presentation/TallowRouteGate.h"
#include "Environment/TallowHideVolume.h"
#include "GameMode/TallowGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Player/Stealth/TallowStealthCharacter.h"
#include "AI/Hunter/TallowHunterNavFilter.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "PatrolPoint/TallowPatrolPoint.h"
#include "Player/TallowCharacter.h"
#include "Tasks/TallowFuseBox.h"
#include "Tasks/TallowFusePickup.h"
#include "HAL/IConsoleManager.h"

namespace
{
// I loved the wall walks and the spiral chases. The movement contract now drives her on the floor; this lets her
// still choose the walls and the ceiling herself instead of only when the floor route fails. Off until the movement
// regressions (contract fixture, kitchen, stairs, the stuck and pancake corners) have been run with it on.
TAutoConsoleVariable<int32> CVarMimVoluntarySurfaces(TEXT("tallow.Mim.VoluntarySurfaces"), 0,
    TEXT("1: Mim chooses wall, ceiling, furniture and crawl-shortcut routes while the movement contract drives her. 0: only when the floor route fails."));

FBox DoorLeafBounds(const ATallowDoor* Door)
{
    if (!Door) return FBox(ForceInit);
    for (const UPrimitiveComponent* Part : TInlineComponentArray<UPrimitiveComponent*>(Door))
        if (Part->GetFName() == TEXT("DoorMesh")) return Part->Bounds.GetBox();
    return Door->GetRootComponent() ? Door->GetRootComponent()->Bounds.GetBox() : FBox(ForceInit);
}

bool SegmentCrossesBarrier(const FVector& From, const FVector& To, const FBox& Bounds,
    const FVector& Extent = FVector::ZeroVector)
{
    if (!Bounds.IsValid) return false;
    return FMath::LineBoxIntersection(Bounds.ExpandBy(Extent), From, To, To - From);
}
}

ATallowHunterAIController::ATallowHunterAIController()
{
    PrimaryActorTick.bCanEverTick = true;
    HunterBrain = CreateDefaultSubobject<UTallowHunterBrain>(TEXT("HunterBrain"));
    BrainComponent = HunterBrain;
    DefaultNavigationFilterClass = UTallowHunterNavFilter::StaticClass();
}

void ATallowHunterAIController::BeginPlay()
{
    Super::BeginPlay();
    Player = Cast<ATallowCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (Player) Player->GetCandleComponent()->OnCandleReset.AddUniqueDynamic(this, &ThisClass::ResetHunt);
    if (auto* Noise = UTallowNoiseSubsystem::Get(this))
        Noise->OnNoiseEmitted.AddUniqueDynamic(this, &ThisClass::HearNoise);
    if (auto* Mode = Cast<ATallowGameMode>(UGameplayStatics::GetGameMode(this)))
        Mode->OnCaught.AddUniqueDynamic(this, &ThisClass::OnCaptured);
    for (TActorIterator<ATallowFuseBox> It(GetWorld()); It; ++It)
        It->OnPowerRestored.AddUniqueDynamic(this, &ThisClass::EnableSecondPhase);
    CacheDoors();
    FTimerHandle StartTimer;
    GetWorldTimerManager().SetTimer(StartTimer, FTimerDelegate::CreateWeakLambda(this, [this]{ RandomiseStart(true); }), .6f, false);
}

void ATallowHunterAIController::AlertPlayer()
{
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now < NextAlertCue) return;
    NextAlertCue = Now + 4.f;
    FireEvent(ETallowKeeperEvent::Alert);
    // Its call when it sees you is my voice line, spoken from its body by the voice component off SeenCallAt.
    // The candle, the flame and her breath answer the call (TallowCandleFlameComponent, TallowFlameVisualComponent,
    // TallowSoundscapeComponent read these).
    if (Player && Creature)
    {
        Player->SeenCallAt = Now;
        Player->KeeperSeenFrom = HeadLocation();
    }
    if (auto* Stealth = Cast<ATallowStealthCharacter>(Player)) Stealth->HunterAlertUntil = Now + 2.5f;
}

void ATallowHunterAIController::Rush(const FVector& Where, float Seconds)
{
    if (!Creature || HuntMode == ETallowHuntMode::Captured) return;
    CancelSpiderPlans();
    const float Now = GetWorld()->GetTimeSeconds();
    RushUntil = FMath::Max(RushUntil, Now + Seconds);
    Observe(Player ? Player->GetActorLocation() : Where, 1.f, false);
    LastHeardAt = Now;
    LastEvidenceType = TEXT("Rush");
    InspectionSpot = nullptr; SearchAnchor = nullptr;
    bTravellingToAmbush = false; AmbushUntil = 0.f;
    SearchUntil = Now + 20.f;
    EnterMode(ETallowHuntMode::Pursue);
    SetGoalNearest(EvidenceLocation);
    Creature->HunterMotion->LookAt(EvidenceLocation + FVector(0, 0, 55), .4f);
    // Its call, the cold bow of the flame and her gasp mean one thing: it can see her. A rush on a sound (her
    // running, a landing, out of an ambush) is answered by its chase voice instead, so the call never lies.
    if (bVisible) AlertPlayer();
    else
    {
        FireEvent(ETallowKeeperEvent::Heard);
        if (Creature->Voice) Creature->Voice->Say(ETallowKeeperLine::Chase, true);
    }
}

void ATallowHunterAIController::ReactToPry(const FVector& Where)
{
    ListenedLoudness = 1.f;
    // 25 Oct: prying used to teleport it outside the door half the time and rush it there the rest, so the
    // first door could not be beaten. Now it hears it, stops to listen, and answers one of several ways.
    RespondToNoise(Where, true);
}

void ATallowHunterAIController::FireEvent(ETallowKeeperEvent Type)
{
    IntentEvent.Type = Type; ++IntentEvent.Count; IntentEvent.Time = GetWorld()->GetTimeSeconds();
}

ETallowKeeperIntent ATallowHunterAIController::GetIntent() const
{
    if (AttackStartedAt >= 0.f) return ETallowKeeperIntent::Lunge;
    if (TacticalPhase == TEXT("WatchingEscape")) return ETallowKeeperIntent::Listen;
    if (TacticalPhase == TEXT("BackingOut")) return ETallowKeeperIntent::Stalk;
    if (bDisguised) return ETallowKeeperIntent::Disguise;
    if (Creature && Creature->HunterTraversal && Creature->HunterTraversal->IsHolding()) return ETallowKeeperIntent::Hold;
    const float Pace = GetPaceScale();
    switch (HuntMode)
    {
    case ETallowHuntMode::Pursue: return ETallowKeeperIntent::Chase;
    case ETallowHuntMode::Investigate: return Pace > 1.05f ? ETallowKeeperIntent::Reveal : Pace < .95f ? ETallowKeeperIntent::Stalk : ETallowKeeperIntent::Hunt;
    case ETallowHuntMode::Search: return ETallowKeeperIntent::Search;
    case ETallowHuntMode::Inspect: return ETallowKeeperIntent::Inspect;
    case ETallowHuntMode::Listen: return ETallowKeeperIntent::Listen;
    case ETallowHuntMode::Ambush: return bTravellingToAmbush ? ETallowKeeperIntent::Stalk : ETallowKeeperIntent::Lurk;
    case ETallowHuntMode::Roam: case ETallowHuntMode::Withdraw: return ETallowKeeperIntent::Wander;
    default: return ETallowKeeperIntent::Idle;
    }
}

FVector ATallowHunterAIController::GetIntentFocus() const
{
    if (TacticalPhase == TEXT("WatchingEscape") || TacticalPhase == TEXT("BackingOut")) return TacticalMemory;
    switch (HuntMode)
    {
    case ETallowHuntMode::Listen: return ListenedTo.IsZero() ? EvidenceLocation : ListenedTo;
    case ETallowHuntMode::Ambush: return AmbushFocus;
    case ETallowHuntMode::Inspect: return InspectionSpot ? InspectionSpot->GetActorLocation() : EvidenceLocation;
    case ETallowHuntMode::Pursue: return EvidenceLocation;
    default: return bHasGoal ? Goal : EvidenceLocation;
    }
}

bool ATallowHunterAIController::GetRouteAhead(FVector& Out) const
{
    const UPathFollowingComponent* Follow = GetPathFollowingComponent();
    if (!Follow || !bHasGoal) return false;
    const FNavPathSharedPtr Path = Follow->GetPath();
    if (!Path.IsValid() || !Path->IsValid()) return false;
    const TArray<FNavPathPoint>& Points = Path->GetPathPoints();
    if (Points.Num() < 2) return false;
    const int32 Ahead = FMath::Clamp(int32(Follow->GetCurrentPathIndex()) + 2, 0, Points.Num() - 1);
    Out = Points[Ahead].Location;
    return true;
}

float ATallowHunterAIController::GetPaceScale() const
{
    return GetWorld() && GetWorld()->GetTimeSeconds() < PaceUntil ? PaceScale : 1.f;
}

void ATallowHunterAIController::SetPace(float Scale, float Seconds)
{
    PaceScale = Scale;
    PaceUntil = GetWorld()->GetTimeSeconds() + Seconds;
}

ETallowNoiseResponse ATallowHunterAIController::ChooseNoiseResponse(const FVector& Where, bool bPry)
{
    if (ForcedNoiseResponse != ETallowNoiseResponse::None) return ForcedNoiseResponse;
    // Weights: hunt it, stalk it, flank it, ambush the way out, lurk near it, rush it, watch it from the dark.
    // Final pass: more waiting and hiding, fewer straight walks at the sound ("stalk and ambush and hide more").
    float W[8] = {0.f, .15f, .25f, .15f, .25f, .2f, 0.f, .2f};
    const auto* Overseer = GetWorld()->GetSubsystem<UTallowHunterDirector>();
    const ETallowTensionPhase Phase = Overseer && bUseDirector ? Overseer->GetPhase() : ETallowTensionPhase::Build;
    switch (Phase)
    {
    case ETallowTensionPhase::Quiet: W[1] = .2f;  W[2] = .35f; W[3] = .15f; W[4] = .15f; W[5] = .15f; W[7] = .2f; break;
    case ETallowTensionPhase::Peak:  W[1] = .25f; W[2] = .1f;  W[3] = .15f; W[4] = .2f;  W[5] = .2f;  W[6] = .1f; W[7] = 0.f; break;
    case ETallowTensionPhase::Relax: W[1] = .4f;  W[2] = .6f;  W[3] = 0.f;  W[4] = 0.f;  W[5] = 0.f;  W[7] = 0.f; break;
    default: break;
    }
    if (!Overseer || !bUseDirector || !Overseer->CanWatchFromNoise()) W[7] = 0.f;
    // The first board: answer it from the far end of the hall, not with a charge at the door (every logged first
    // contact on 27 Oct was a sighting within 7 to 19 s and a chase). It watches if it can, else creeps or waits.
    if (bPry && Overseer && bUseDirector && Overseer->GetAct() == ETallowStoryAct::FirstContact)
    {
        W[1] = 0.f; W[2] = .45f; W[3] = 0.f; W[4] = .3f; W[5] = .25f; W[6] = 0.f;
        if (W[7] > 0.f) W[7] = 1.5f;
    }
    FVector Projected; float PathLength = 3000.f;
    if (!IsReachable(Where, Projected, &PathLength)) PathLength = 3000.f;
    if (PathLength < 600.f && ListenedLoudness > .35f) W[6] += .5f;     // loud and right there
    if (PathLength > 1800.f) { W[3] *= .5f; W[4] *= .5f; W[5] *= .5f; }  // far off: it just comes
    if (bPry) { W[4] *= 1.4f; W[5] *= 1.4f; }                           // a door being forced: wait by it, or outside
    // It saw her recently: it thinks ahead, round the other way or out at the exit.
    if (GetWorld()->GetTimeSeconds() - LastSeenAt < 20.f) { W[3] *= 1.5f; W[4] *= 1.5f; }
    W[static_cast<int32>(LastNoiseResponse)] = 0.f;                     // never the same way twice running
    float Total = 0.f;
    for (int32 I = 1; I < 8; ++I) Total += W[I];
    if (Total <= 0.f) return LastNoiseResponse == ETallowNoiseResponse::Hunt ? ETallowNoiseResponse::Stalk : ETallowNoiseResponse::Hunt;
    float Pick = Random.FRandRange(0.f, Total);
    for (int32 I = 1; I < 8; ++I) { Pick -= W[I]; if (Pick <= 0.f) return static_cast<ETallowNoiseResponse>(I); }
    return ETallowNoiseResponse::Hunt;
}

void ATallowHunterAIController::RespondToNoise(const FVector& Where, bool bPry)
{
    const float Now = GetWorld()->GetTimeSeconds();
    const ETallowNoiseResponse Response = ChooseNoiseResponse(Where, bPry);
    LastNoiseResponse = Response;
    FireEvent(ETallowKeeperEvent::Heard);
    Observe(Where, .9f, false);
    SearchUntil = Now + (bSecondPhase ? 28.f : 21.f);
    Creature->HunterMotion->LookAt(Where + FVector(0, 0, 55), 1.5f);
    ListenedTo = Where;
    if (Response == ETallowNoiseResponse::Rush)
    {
        LastDecision = TEXT("Rush: loud and close");
        Rush(Where, 3.f);
        return;
    }
    // Always the listening beat first: it freezes and turns its head, then commits.
    ListenOutcome = Response;
    BeginListening(Random.FRandRange(1.3f, 2.f));
    static const TCHAR* Names[] = {TEXT("None"), TEXT("Heard it: will hunt"), TEXT("Heard it: will stalk"), TEXT("Heard it: will flank"),
        TEXT("Heard it: will ambush the way out"), TEXT("Heard it: will lurk"), TEXT("Rush"), TEXT("Heard it: will watch from the dark")};
    LastDecision = Names[static_cast<int32>(Response)];
}

void ATallowHunterAIController::FinishListening(float Now)
{
    const ETallowNoiseResponse Outcome = ListenOutcome;
    ListenOutcome = ETallowNoiseResponse::Hunt;
    // The jump-out is decided here, the moment it has just heard her from a distance (it used to wait for a turn
    // with no goal while 5 to 13 m from fresh evidence, which almost never came).
    if (Outcome != ETallowNoiseResponse::Watch && TryBoo(Now)) return;
    switch (Outcome)
    {
    case ETallowNoiseResponse::Watch:
        if (TryWatch(Now, ListenedTo, true)) return;
        SetPace(.65f, 30.f);
        EnterMode(ETallowHuntMode::Investigate);
        SetGoalNearest(EvidenceLocation);
        LastDecision = TEXT("Stalk: creeping in on the sound");
        return;
    case ETallowNoiseResponse::Stalk:
        SetPace(.65f, 30.f);
        EnterMode(ETallowHuntMode::Investigate);
        SetGoalNearest(EvidenceLocation);
        LastDecision = TEXT("Stalk: creeping in on the sound");
        return;
    case ETallowNoiseResponse::Flank:
        if (FlankTo(ListenedTo)) return;
        break;
    case ETallowNoiseResponse::Ambush:
        if (TrySpiderAmbush(ListenedTo)) return;
        if (AmbushExit(ListenedTo)) return;
        if (LurkNear(ListenedTo)) return;
        break;
    case ETallowNoiseResponse::Lurk:
        if (LurkNear(ListenedTo)) return;
        break;
    default: break;
    }
    EnterMode(ETallowHuntMode::Investigate);
    SetGoalNearest(EvidenceLocation);
    LastDecision = TEXT("Hunt: straight at the sound");
}

bool ATallowHunterAIController::FlankTo(const FVector& Where)
{
    // Come at the sound from another side: a point beside it that the straight route would not arrive from,
    // preferring a different doorway into that room.
    if (!Creature) return false;
    CacheDoors();
    const FVector Straight = (Where - Creature->GetActorLocation()).GetSafeNormal2D();
    FVector Best = FVector::ZeroVector; float BestScore = -MAX_flt;
    auto Consider = [&](const FVector& Candidate, float Bonus)
    {
        FVector Projected; float PathLength;
        if (!IsReachable(Candidate, Projected, &PathLength)) return;
        const FVector From = (Projected - Where).GetSafeNormal2D();
        // Arriving from the side or the far side of the sound, not straight down the line it heard it along.
        const float Around = -FVector::DotProduct(From, -Straight);
        if (Around < -.2f) return;
        const float Score = Bonus + Around * 120.f - PathLength * .05f + Random.FRandRange(0.f, 60.f);
        if (Score > BestScore) { BestScore = Score; Best = Projected; }
    };
    for (const FDoorWatch& Watch : Doors)
        if (Watch.Door.IsValid() && DoorOpenness(Watch) > .3f && FVector::Dist2D(Watch.Center, Where) < 700.f)
            Consider(Watch.Center + (Watch.Center - Where).GetSafeNormal2D() * 60.f, 120.f);
    for (int32 Try = 0; Try < 10; ++Try)
    {
        const float Angle = Random.FRandRange(0.f, 2.f * PI);
        Consider(Where + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Random.FRandRange(250.f, 420.f), 0.f);
    }
    if (BestScore == -MAX_flt || !SetGoal(Best)) return false;
    EnterMode(ETallowHuntMode::Investigate);       // on arrival it searches round the sound itself
    LastDecision = TEXT("Flank: coming round the other way");
    return true;
}

bool ATallowHunterAIController::AmbushExit(const FVector& Where)
{
    // Where would the kid go from there? Out of the room by the door it is not coming through. It waits just
    // outside that one, facing in.
    if (!Creature) return false;
    CacheDoors();
    const FVector Here = Creature->GetActorLocation();
    const FDoorWatch* Pick = nullptr; float PickScore = -MAX_flt; FVector Wait;
    for (const FDoorWatch& Watch : Doors)
    {
        if (!Watch.Door.IsValid() || IsBarred(Watch.Door.Get())) continue;
        const float FromSound = FVector::Dist2D(Watch.Center, Where);
        if (FromSound > 900.f) continue;
        // The exit away from it: the further this door is from where it stands, the likelier the kid takes it. It
        // waits beside the doorway on the far side, against the wall, which hides it from the room (it used to stand
        // out in the open 1.6 m past the door and every one was rejected as exposed).
        const FVector Out = (Watch.Center - Where).GetSafeNormal2D();
        const FVector Along = FVector::CrossProduct(FVector::UpVector, Out);
        for (const float Side : {1.f, -1.f})
        {
            FVector Projected; float PathLength;
            if (!IsReachable(Watch.Center + Out * 75.f + Along * Side * 125.f, Projected, &PathLength) || !HasAmbushCover(Projected)) continue;
            const float Score = FVector::Dist2D(Watch.Center, Here) * .5f - FromSound * .3f + Random.FRandRange(0.f, 80.f);
            if (Score > PickScore) { PickScore = Score; Pick = &Watch; Wait = Projected; }
        }
    }
    if (!Pick) return RequestAmbushFromEvidence();
    if (!SetGoal(Wait)) return false;
    const float Now = GetWorld()->GetTimeSeconds();
    AmbushFocus = Pick->Center + FVector(0, 0, 60);
    bTravellingToAmbush = true;
    AmbushUntil = Now + 30.f;
    SearchUntil = FMath::Max(SearchUntil, AmbushUntil + 5.f);
    EnterMode(ETallowHuntMode::Ambush);
    SetPace(.8f, 20.f);
    LastDecision = TEXT("Ambush: waiting beside the other door");
    return true;
}

bool ATallowHunterAIController::LurkNear(const FVector& Where)
{
    // Somewhere out of sight of the noise, next door or round a corner, to wait: an authored ambush spot if one
    // covers the place, else a covered point 3 to 4.5 m short of it on its own side.
    EvidenceLocation = Where;
    // Its own ways first, some of the time: behind a crawl's mouth, on a cupboard, at the far end of her crawl,
    // or up on the ceiling short of the sound.
    if (Random.FRand() < .75f)
    {
        if (LurkInLink(Where) || LurkAtMarker(Where)) return true;
        const FVector Short = Where + (Creature->GetActorLocation() - Where).GetSafeNormal2D() * Random.FRandRange(220.f, 360.f);
        if (PerchOver(Short, Where + FVector(0, 0, 40), TEXT("Lurk: up on the ceiling short of the sound"))) return true;
    }
    if (RequestAmbushFromEvidence()) { SetPace(.55f, 25.f); LastDecision = TEXT("LurkNearNoise"); return true; }
    const auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!Nav || !Creature) return false;
    const FVector Back = (Creature->GetActorLocation() - Where).GetSafeNormal2D();
    for (int32 Try = 0; Try < 14; ++Try)
    {
        const float Angle = Random.FRandRange(-70.f, 70.f);
        const FVector Candidate = Where + Back.RotateAngleAxis(Angle, FVector::UpVector) * Random.FRandRange(300.f, 450.f);
        FVector Projected;
        if (!IsReachable(Candidate, Projected) || !HasAmbushCover(Projected)) continue;
        if (!SetGoal(Projected)) continue;
        const float Now = GetWorld()->GetTimeSeconds();
        AmbushFocus = Where + FVector(0, 0, 60);
        bTravellingToAmbush = true;
        AmbushUntil = Now + 25.f;
        SearchUntil = FMath::Max(SearchUntil, AmbushUntil + 5.f);
        EnterMode(ETallowHuntMode::Ambush);
        SetPace(.55f, 25.f);
        LastDecision = TEXT("LurkNearNoise");
        return true;
    }
    return false;
}

void ATallowHunterAIController::RandomiseStart(bool bTeleport)
{
    if (!Creature || !Player) return;
    // A distant marker may be inside a room whose two bolts are still locked.
    // Choose from the main connected patrol region with the current doors and
    // gates, then apply the distance rule. Otherwise a new run can strand Mim
    // in the back stairwell before it has made its first decision.
    auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
    TArray<FVector> Candidates;
    for (TActorIterator<ATallowPatrolPoint> It(GetWorld()); It; ++It)
    {
        FNavLocation Projected;
        if (Nav && Nav->ProjectPointToNavigation(It->GetActorLocation(), Projected, FVector(60.f, 60.f, 250.f))
            && !Candidates.ContainsByPredicate([&Projected](const FVector& P) { return FVector::DistSquared(P, Projected.Location) < 25.f; }))
            Candidates.Add(Projected.Location);
    }
    if (Candidates.Num() == 0) return;
    TArray<int32> Region;
    for (int32 I = 0; I < Candidates.Num(); ++I) Region.Add(I);
    auto Root = [&Region](int32 I)
    {
        while (Region[I] != I) I = Region[I];
        return I;
    };
    for (int32 I = 0; I < Candidates.Num(); ++I)
        for (int32 J = I + 1; J < Candidates.Num(); ++J)
        {
            if (Root(I) == Root(J)) continue;
            const auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(this,
                Candidates[I], Candidates[J], this);
            if (Path && Path->IsValid() && !Path->IsPartial() && !PathCrossesShut(Path->PathPoints))
                Region[Root(J)] = Root(I);
        }
    int32 MainRegion = INDEX_NONE, MainCount = 0;
    float MainNear = MAX_flt;
    for (int32 I = 0; I < Candidates.Num(); ++I)
    {
        int32 Count = 0;
        float Near = MAX_flt;
        for (int32 J = 0; J < Candidates.Num(); ++J)
            if (Root(I) == Root(J))
            {
                ++Count;
                Near = FMath::Min(Near, FVector::Dist(Candidates[J], Player->GetActorLocation()));
            }
        if (Count > MainCount || (Count == MainCount && Near < MainNear))
        { MainRegion = Root(I); MainCount = Count; MainNear = Near; }
    }
    TArray<int32> Distant;
    int32 Farthest = INDEX_NONE;
    float FarthestDistance = 0.f;
    for (int32 I = 0; I < Candidates.Num(); ++I)
        if (Root(I) == MainRegion)
        {
            const float Distance = FVector::Dist(Candidates[I], Player->GetActorLocation());
            if (Distance > 1500.f) Distant.Add(I);
            if (Distance > FarthestDistance) { Farthest = I; FarthestDistance = Distance; }
        }
    if (Farthest == INDEX_NONE || (bTeleport && FarthestDistance < 900.f)) return;
    const FVector Pick = Candidates[Distant.IsEmpty() ? Farthest : Distant[Random.RandRange(0, Distant.Num() - 1)]];
    UE_LOG(LogTemp, Display, TEXT("HUNTER_SPAWN connected=%d/%d at=%s distance=%.0f"),
        MainCount, Candidates.Num(), *Pick.ToCompactString(), FVector::Dist(Pick, Player->GetActorLocation()));
    const FVector Feet = Pick + FVector(0.f, 0.f, Creature->GetSimpleCollisionHalfHeight());
    Creature->SetResetLocation(Feet);
    if (bTeleport)
    {
        Creature->SetActorLocation(Feet, false, nullptr, ETeleportType::TeleportPhysics);
        Creature->HunterMotion->SnapFit();
        ResetHunt();
    }
}

void ATallowHunterAIController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (auto* Noise = UTallowNoiseSubsystem::Get(this))
        Noise->OnNoiseEmitted.RemoveDynamic(this, &ThisClass::HearNoise);
    if (Player) Player->GetCandleComponent()->OnCandleReset.RemoveDynamic(this, &ThisClass::ResetHunt);
    Super::EndPlay(Reason);
}

void ATallowHunterAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    Creature = Cast<ATallowHunterCreature>(InPawn);
    ResetHunt();
    HunterBrain->StartLogic();
}

void ATallowHunterAIController::ResetHunt()
{
    bContractReview = false; ContractReviewReached = false; ContractBlockedSince = -1.f;
    StopMovement();
    bVisible = bHadVisible = bHasEvidence = bHasGoal = bSecondPhase = false;
    bDirectPlayerSight = false; VisiblePlayerGazeTarget = FVector::ZeroVector;
    KnownOccupiedHide.Reset(); KnownHideEvidenceAt = -1000.f;
    SightConfidence = 0.f;
    if (Player) { Player->KeeperSight = 0.f; Player->bSeenByKeeper = false; }
    LastSeenAt = LastHeardAt = -1000.f;
    EvidenceLocation = Goal = LastPursuitGoal = FVector::ZeroVector;
    bRecoveryDetour = bRecoveryTraversal = bRecoveryWaiting = false;
    RecoveryMode = NAME_None;
    RecoveryCooldowns.Reset(); LinkRetryAfter.Reset(); RecoveryLog.Reset();
    ResetRecoveryPlanner(true);
    RecoveryFloorQueue.Reset();
    RecoveryAttempts = RecoverySuccesses = 0; NextRecoveryTry = 0.f;
    RecoveryFitClearSince = -1.f; NextRecoveryTurnTry = 0.f;
    ResetShadowRouting();
    NextCornerProbe = CornerHoldUntil = 0.f; bCornerAmbush = bCornerAttackPending = false;
    TacticalPhase = NAME_None; TacticalRetreats = 0; TacticalLog.Reset();
    StruggleSince = -1.f; LastStruggleAt = -1000.f; StruggleClosest = MAX_flt; NextTacticalThought = TacticalUntil = 0.f;
    bTacticalBackoutGoal = false;
    NextDecision = NextAreaDecision = NextMoveRequest = NextPursuitGoalTry = SearchUntil = InspectUntil = NextSurfaceTry = 0.f;
    NextVoluntaryTry = 0.f; NextWanderClimb = 15.f;
    bWatchPending = bWatching = bWatchStaring = bWatchReviewHold = false; WatchGlint = 0.f; WatchStage = 0; SlipGiggleAt = -1.f;
    WatchSlipUntil = 0.f;
    LastWatchPost = FVector(1.e7f); LoggedTactic = NAME_None;
    RushUntil = 0.f;
    ListenUntil = AmbushUntil = NextAmbushAt = NextNoiseReaction = SearchStartedAt = 0.f;
    PaceUntil = SpotFreezeUntil = StalkSightUntil = 0.f; LastSeenAt = -1000.f;
    ListenOutcome = ETallowNoiseResponse::Hunt;
    bBooPending = false; BooRevealAt = -1.f;
    AttackStartedAt = -1.f;
    AttackWindupAlpha = EvidenceStrength = ContinuousPursuit = 0.f;
    EvidenceHeading = PreviousVisualLocation = AmbushFocus = FVector::ZeroVector;
    PreviousVisualAt = -1000.f;
    SearchSteps = 0;
    bTravellingToAmbush = false;
    SpotVisits.Reset();
    AnchorVisits.Reset();
    Observations.Reset();
    LearnedAreas.Reset();
    RecentSearchPoints.Reset();
    InspectionSpot = nullptr;
    SearchAnchor = nullptr;
    CancelSpiderPlans();
    bOnLink = false;
    DecisionLog.Reset(); LoggedDecision = NAME_None;
    LastDisguiseSpot = FVector(1.e7f);
    if (Creature && Creature->HunterMotion) Creature->HunterMotion->SetMound(false);
    if (Creature && Creature->Voice) Creature->Voice->SetDisguised(false);
    LastDecision = TEXT("Roam");
    LastEvidenceType = NAME_None;
    HuntMode = ETallowHuntMode::Roam;
    DefaultNavigationFilterClass = UTallowHunterNavFilter::StaticClass();
    if (Creature)
    {
        ProgressAnchor = Creature->GetActorLocation();
        Creature->SetPhase(ETallowCreaturePhase::PhaseOne);
        Creature->SetCreatureVisualState(ETallowCreatureState::Patrol);
        Creature->HunterMotion->ResetContacts();
        Creature->HunterTraversal->ResetTraversal();
    }
    if (auto* Director = GetWorld()->GetSubsystem<UTallowHunterDirector>()) Director->ResetPressure();
}

void ATallowHunterAIController::SetHuntEnabled(bool bEnabled)
{
    if (bEnabled) HunterBrain->ResumeLogic(TEXT("Preview enabled"));
    else
    {
        bDirectPlayerSight = false;
        HunterBrain->PauseLogic(TEXT("Preview suspended"));
        StopMovement();
        AttackStartedAt = -1.f;
        AttackWindupAlpha = 0.f;
        TacticalPhase = NAME_None;
        if (Creature && Creature->HunterTraversal)
        {
            Creature->HunterTraversal->SetTacticalPaused(false);
            Creature->HunterTraversal->SetAttackPaused(false);
        }
    }
}

bool ATallowHunterAIController::SetReviewMovementMode(ETallowHuntMode Mode)
{
    if (!HunterBrain || !HunterBrain->IsPaused()
        || HuntMode == ETallowHuntMode::Captured || Mode == ETallowHuntMode::Captured) return false;
    EnterMode(Mode);
    return true;
}

UTallowNoiseSubsystem* ATallowHunterAIController::GetNoiseSystem() const
{
    return UTallowNoiseSubsystem::Get(this);
}

UTallowHunterDirector* ATallowHunterAIController::GetHuntDirector() const
{
    return GetWorld()->GetSubsystem<UTallowHunterDirector>();
}

void ATallowHunterAIController::EnableSecondPhase()
{
    bSecondPhase = true;
    DefaultNavigationFilterClass = UTallowHunterNavFilter::StaticClass();
    if (Creature) Creature->SetPhase(ETallowCreaturePhase::PhaseTwo);
}

void ATallowHunterAIController::OnCaptured()
{
    if (auto* Overseer = GetWorld()->GetSubsystem<UTallowHunterDirector>()) Overseer->NoteCaught();
    RandomiseStart(false);
    EnterMode(ETallowHuntMode::Captured);
    StopMovement();
    if (Creature && Creature->HunterTraversal) Creature->HunterTraversal->FreezeForCapture();
}

bool ATallowHunterAIController::SensePlayer(float DeltaSeconds)
{
    bDirectPlayerSight = false;
    bool bSense = false;
    if (Player && Creature && !Player->IsTiedToChair() && !Player->IsHiddenInSpot())
    {
        const ECandleState Light = Player->GetCandleComponent()->GetCandleState();
        if (Light != ECandleState::Out)
        {
            FVector Eye; FRotator Facing;
            Creature->GetActorEyesViewPoint(Eye, Facing);
            Eye = HeadLocation();
            const FVector Target = Player->GetPawnViewLocation();
            const FVector ToPlayer = Target - Eye;
            const auto* Overseer = GetWorld()->GetSubsystem<UTallowHunterDirector>();
            float Range = (Light == ECandleState::Lit ? LitSightRange : CuppedSightRange)
                * (Overseer ? Overseer->GetSightMultiplier() : 1.f);
            // Only the candle gives you away (25 Oct): holding your breath silences you, it doesn't hide you.
            const float ViewDot = FVector::DotProduct(ToPlayer.GetSafeNormal(), Facing.Vector());
            // Once it has you it keeps you: the cone no longer matters, it simply turns to look.
            const bool bTracking = bHadVisible;
            if (ToPlayer.SizeSquared() < FMath::Square(Range) && (bTracking || ViewDot > .34f
                || (Light == ECandleState::Lit && ViewDot > -.3f && ToPlayer.SizeSquared() < FMath::Square(700.f))))
            {
                // Head, body or legs: a chair or a rail in the way does not break contact.
                const FVector Body = Player->GetActorLocation(), Legs = Body - FVector(0, 0, 70.f);
                bSense = (HasClearTrace(Eye, Target) || HasClearTrace(Eye, Body) || HasClearTrace(Eye, Legs))
                    && !BoardsBetween(Eye, Target);
            }
            const float Now = GetWorld()->GetTimeSeconds();
            if (bSense) LastClearAt = Now;
            // Recognition may linger behind cover, but fresh visibility must
            // stop immediately so pursuit cannot update from a hidden pawn.
            else if (bTracking && Now - LastClearAt < 1.6f)
            {
                SightConfidence = 1.f;
            }
            const float Gain = Light == ECandleState::Lit ? 7.f : 1.7f;
            SightConfidence = FMath::Clamp(SightConfidence + DeltaSeconds * (bSense ? Gain : -3.f), 0.f, 1.f);
        }
        else SightConfidence = FMath::Max(0.f, SightConfidence - DeltaSeconds * 3.f);
    }
    else SightConfidence = FMath::Max(0.f, SightConfidence - DeltaSeconds * 3.f);
    // Confidence can linger; permission to capture cannot. A fresh clear trace
    // is required on this update as well as accumulated recognition.
    bDirectPlayerSight = bSense && SightConfidence >= .95f;
    if (bDirectPlayerSight) VisiblePlayerGazeTarget = Player->GetPawnViewLocation();
    return bDirectPlayerSight;
}

void ATallowHunterAIController::HearNoise(const FTallowNoiseEvent& Event)
{
    if (!Creature || !HunterBrain->IsRunning() || HunterBrain->IsPaused()
        || HuntMode == ETallowHuntMode::Captured || !Event.bAlertsAI
        || Event.Instigator == Creature || (Player && Player->IsTiedToChair())) return;
    auto* Noise = UTallowNoiseSubsystem::Get(this);
    FVector Ear; FRotator Facing;
    Creature->GetActorEyesViewPoint(Ear, Facing);
    Ear = HeadLocation();
    // Sprinting, landing and lighting a match carry much further than the shared
    // profiles say. Scaled here so the shared noise system stays untouched.
    FTallowNoiseEvent Heard = Event;
    const float Carry = Event.Type == ETallowNoiseType::RunStep ? 2.4f
        : Event.Type == ETallowNoiseType::Land ? 2.2f
        : Event.Type == ETallowNoiseType::MatchStrike ? 3.f
        : Event.Type == ETallowNoiseType::Footstep ? 1.5f : 1.3f;
    Heard.Loudness = FMath::Min(1.f, Heard.Loudness * Carry);
    Heard.Range *= Carry;
    if (const auto* Overseer = GetWorld()->GetSubsystem<UTallowHunterDirector>())
    {
        Heard.Loudness = FMath::Min(1.f, Heard.Loudness * Overseer->GetHearingMultiplier());
        Heard.Range *= Overseer->GetHearingMultiplier();
    }
    const bool bGiveaway = Event.Type == ETallowNoiseType::RunStep || Event.Type == ETallowNoiseType::Land;
    const bool bPry = Event.Type == ETallowNoiseType::Pry;
    // Running, jumping and prying carry through walls.
    const float Perceived = Noise ? Noise->GetLoudnessAt(Heard, Ear, !(bGiveaway || bPry)) : 0.f;
    if (Perceived < .025f) return;
    // A nearby prop or door may draw an inspection, but does not reveal who is
    // concealed there. Only a new, audible sound made by the hidden player does.
    if (Player && Player->IsHiddenInSpot() && Event.Instigator == Player
        && Event.Timestamp >= GetWorld()->GetTimeSeconds() - .25
        && FVector::Dist(Event.Location, Player->GetPawnViewLocation()) < 100.f)
    {
        KnownOccupiedHide = Player->GetCurrentHideVolume();
        KnownHideEvidenceAt = GetWorld()->GetTimeSeconds();
    }
    // Done with that area: only something loud brings it straight back.
    if (!bGiveaway && (bPry || Perceived < .12f) && GetWorld()->GetTimeSeconds() < QuietUntil
        && FVector::Dist2D(Event.Location, QuietAreaCenter) < 700.f) return;
    ++HeardNoiseCount;
    LastHeardAt = GetWorld()->GetTimeSeconds();
    if (TacticalPhase != NAME_None)
    {
        // Real new sound may update her memory while she gives up the struggle,
        // without canceling the physical backout or the selected intercept.
        Observe(Event.Location, FMath::Clamp(Perceived, .2f, 1.f), false);
        TacticalMemory = Event.Location;
        return;
    }
    // Live visual contact is stronger evidence than a distraction.
    if (bVisible) return;
    if (bWatchPending || bWatching)
    {
        // Watching the place the sound came from: it keeps watching, its attention on the new sound. A sound from
        // anywhere else, or a loud one, is answered the ordinary way.
        if (FVector::Dist2D(Event.Location, WatchArea) < 350.f && Perceived < .5f && !bGiveaway)
        {
            Observe(Event.Location, FMath::Clamp(Perceived * 3.f, .2f, .9f), false);
            SearchUntil = FMath::Max(SearchUntil, GetWorld()->GetTimeSeconds() + 21.f);
            WatchArea = Event.Location;
            FireEvent(ETallowKeeperEvent::Heard);
            Creature->HunterMotion->LookAt(Event.Location + FVector(0, 0, 55), 1.2f);
            LastDecision = TEXT("Watch: heard her where it is watching");
            return;
        }
        EndWatch(GetWorld()->GetTimeSeconds(), false, TEXT("Watch: a sound elsewhere, going to see"));
    }
    // Lying in wait it still answers every sound, but one coming its way is her walking into it. Every ambush now
    // holds for that (it used to be only the heap and the holds: a footstep ended every other wait on the spot).
    if ((bDisguisePending || PlannedHold != EHold::None || HuntMode == ETallowHuntMode::Ambush)
        && HandleNoiseInAmbush(Event.Location, Perceived)) return;
    const float NowHeard = GetWorld()->GetTimeSeconds();
    if (bPry && NowHeard >= NextPryReaction)
    {
        NextPryReaction = NowHeard + 12.f;
        ReactToPry(Event.Location);
        return;
    }
    if (bGiveaway)
    {
        // Running gives her away, but in the quiet and the relief it is a noise to answer, not a charge across the
        // house (a sprint that summons it at full speed from anywhere felt cheap).
        const auto* Overseer = GetWorld()->GetSubsystem<UTallowHunterDirector>();
        const ETallowTensionPhase Phase = Overseer && bUseDirector ? Overseer->GetPhase() : ETallowTensionPhase::Peak;
        if ((Phase == ETallowTensionPhase::Quiet || Phase == ETallowTensionPhase::Relax) && HuntMode != ETallowHuntMode::Pursue
            && Perceived < .5f)
        {
            if (HuntMode != ETallowHuntMode::Listen && NowHeard >= NextNoiseReaction)
            {
                NextNoiseReaction = NowHeard + 2.4f;
                ListenedLoudness = Perceived;
                RespondToNoise(Event.Location, false);
            }
            return;
        }
        Rush(Event.Location, 3.5f);
        return;
    }
    // Quiet, occluded sounds identify an area. Only strong nearby sounds fix
    // a precise point. This is sampled once per event, never from the player.
    const float Uncertainty = FMath::GetMappedRangeValueClamped(
        FVector2D(.025f, .24f), FVector2D(110.f, 0.f), Perceived);
    const float Angle = Random.FRandRange(0.f, 2.f * PI);
    const FVector HeardPoint = Event.Location + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0)
        * Random.FRandRange(Uncertainty * .35f, Uncertainty);
    Observe(HeardPoint, FMath::Clamp(Perceived * 3.f, .2f, .95f), false);
    LastEvidenceType = FName(*UEnum::GetValueAsString(Event.Type));
    InspectionSpot = nullptr;
    SearchAnchor = nullptr;
    bTravellingToAmbush = false;
    AmbushUntil = 0.f;
    SearchUntil = LastHeardAt + (bSecondPhase ? 28.f : 21.f);
    Creature->HunterMotion->LookAt(HeardPoint + FVector(0, 0, 55), 1.5f);
    // A loud noise nearby: it freezes and turns its head to it first, then goes. 25 Oct: "the door opens
    // and it runs in"; the player needs a couple of seconds to hide. Only a chase skips the beat.
    // Every sound it hears gets a deliberate answer ("it should not ignore loud noises... it's going question mark:
    // where is it right now?"). Mid-chase it just heads for the new sound; mid-search a fresh sound refines the goal.
    if (HuntMode == ETallowHuntMode::Pursue)
    {
        EnterMode(ETallowHuntMode::Investigate);
        SetGoal(EvidenceLocation);
    }
    else if (HuntMode != ETallowHuntMode::Listen && LastHeardAt >= NextNoiseReaction
        && HuntMode != ETallowHuntMode::Ambush)
    {
        NextNoiseReaction = LastHeardAt + 2.4f;
        ListenedLoudness = Perceived;
        RespondToNoise(HeardPoint, false);
    }
    else if (HuntMode == ETallowHuntMode::Investigate || HuntMode == ETallowHuntMode::Search)
    {
        SetGoal(EvidenceLocation);
        LastDecision = TEXT("Heard more: closing in");
    }
}

bool ATallowHunterAIController::HasClearTrace(const FVector& From, const FVector& To, AActor* ExtraIgnored) const
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HunterSight), true, Creature);
    if (ExtraIgnored) Query.AddIgnoredActor(ExtraIgnored);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Query)
        || Hit.GetActor() == Player;
}

void ATallowHunterAIController::Observe(const FVector& Location, float Strength, bool bVisual)
{
    const float Now = GetWorld()->GetTimeSeconds();
    if (bVisual && Now - PreviousVisualAt >= .15f)
    {
        if (Now - PreviousVisualAt < 1.f && FVector::Dist2D(Location, PreviousVisualLocation) > 8.f)
            EvidenceHeading = (Location - PreviousVisualLocation).GetSafeNormal2D();
        PreviousVisualLocation = Location;
        PreviousVisualAt = Now;
    }
    EvidenceLocation = Location;
    EvidenceStrength = Strength;
    bHasEvidence = true;
    Observations.RemoveAll([Now](const FObservation& O) { return Now - O.Time > 35.f; });
    if (Observations.IsEmpty() || Now - Observations.Last().Time > .8f
        || FVector::Dist(Location, Observations.Last().Location) > 120.f)
    {
        Observations.Add({Location, Now, Strength, bVisual});
        if (Observations.Num() > 20) Observations.RemoveAt(0);
        const FIntVector Cell(FMath::FloorToInt(Location.X / 300.f),
            FMath::FloorToInt(Location.Y / 300.f), FMath::FloorToInt(Location.Z / 300.f));
        int32& Count = LearnedAreas.FindOrAdd(Cell);
        Count = FMath::Min(Count + 1, 6);
    }
}

float ATallowHunterAIController::FamiliarityAt(const FVector& Point) const
{
    const FIntVector Cell(FMath::FloorToInt(Point.X / 300.f),
        FMath::FloorToInt(Point.Y / 300.f), FMath::FloorToInt(Point.Z / 300.f));
    const int32* Count = LearnedAreas.Find(Cell);
    return Count ? float(*Count) : 0.f;
}

void ATallowHunterAIController::BeginListening(float Duration)
{
    StopMovement();
    bHasGoal = false;
    ListenUntil = GetWorld()->GetTimeSeconds() + Duration;
    EnterMode(ETallowHuntMode::Listen);
    if (Creature && Creature->Voice) Creature->Voice->Say(ETallowKeeperLine::Heard);
    ++ListeningPauses;
    LastDecision = TEXT("ListenToEvidence");
}

void ATallowHunterAIController::EnterMode(ETallowHuntMode Mode)
{
    if (HuntMode == Mode) return;
    if (HuntMode == ETallowHuntMode::Ambush) DropPendingPlans();
    if (Mode == ETallowHuntMode::Roam || Mode == ETallowHuntMode::Withdraw || Mode == ETallowHuntMode::Captured)
    {
        bRecoveryDetour = bRecoveryTraversal = bRecoveryWaiting = false;
        RecoveryMode = NAME_None;
        RecoveryFloorQueue.Reset();
    }
    HuntMode = Mode;
    LastDecision = FName(*UEnum::GetValueAsString(Mode));
    if (!Creature) return;
    ETallowCreatureState Visual = ETallowCreatureState::Patrol;
    switch (Mode)
    {
    case ETallowHuntMode::Pursue: Visual = ETallowCreatureState::Chasing; break;
    case ETallowHuntMode::Investigate: Visual = ETallowCreatureState::Investigating; break;
    case ETallowHuntMode::Listen: Visual = ETallowCreatureState::Investigating; break;
    case ETallowHuntMode::Ambush: Visual = ETallowCreatureState::SearchLow; break;
    case ETallowHuntMode::Search: Visual = ETallowCreatureState::SearchingCorner; break;
    case ETallowHuntMode::Inspect:
        Visual = InspectionSpot && InspectionSpot->GetHidingSpotType() == ETallowHidingSpotType::UnderBed
            ? ETallowCreatureState::SearchLow : ETallowCreatureState::SearchHigh;
        break;
    default: break;
    }
    Creature->SetCreatureVisualState(Visual);
}

void ATallowHunterAIController::DropPendingPlans()
{
    // An interrupted boo, heap or hold used to stay "pending" for the rest of the life, which silently disabled
    // the boo, the heap and the corner brace for good. Only plans still on their way end here; a heap that has
    // settled, a hold that has begun or a watch that is held keep their own exits.
    bBooPending = false; BooRevealAt = -1.f;
    if (bDisguisePending && !bDisguised) bDisguisePending = bDisguiseArrived = false;
    if (bHoldPending) { bHoldPending = false; PlannedHold = EHold::None; HoldLink = INDEX_NONE; }
    bWatchPending = false;
    if (bWatching) { bWatching = bWatchStaring = false; }
}

bool ATallowHunterAIController::IsLurking() const
{
    const bool bHolding = Creature && Creature->HunterTraversal && Creature->HunterTraversal->IsHolding();
    return bDisguised || bWatching || (bHolding && (PlannedHold != EHold::None || bCornerAmbush))
        || (HuntMode == ETallowHuntMode::Ambush && !bTravellingToAmbush && AttackStartedAt < 0.f && !bLeaveBoardsAfterStare);
}

bool ATallowHunterAIController::IsReachable(const FVector& Location, FVector& Projected, float* OutPathLength) const
{
    if (!Creature) return false;
    const auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
    FNavLocation Point;
    if (!Nav || !Nav->ProjectPointToNavigation(Location, Point, FVector(120, 120, 160))) return false;
    auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(const_cast<ThisClass*>(this),
        Creature->GetActorLocation(), Point.Location, const_cast<ThisClass*>(this));
    if (!Path || !Path->IsValid() || Path->IsPartial()) return false;
    // The navmesh runs straight through boarded doors and shut gates; a route through one is no route. Roaming
    // used to keep walking it into the first boarded door (the Keeper Lab, 25 Oct).
    if (const_cast<ThisClass*>(this)->PathCrossesShut(Path->PathPoints)) return false;
    Projected = Point.Location;
    if (OutPathLength) *OutPathLength = FMath::Max(Path->GetPathLength(), Path->GetPathCost());
    return true;
}

bool ATallowHunterAIController::SetGoalNearest(const FVector& Location)
{
    if (bRecoveryDetour || bRecoveryTraversal || bRecoveryWaiting)
    {
        RecoveryFinalGoal = Location;
        return true;
    }
    if ((PendingLink != INDEX_NONE || bOnLink) && bLinkFinalGoal)
    {
        LinkFinalGoal = Location;
        return true;
    }
    const bool bGoalOnNav = SetGoal(Location);
    if (bGoalOnNav && bShadowWaypointActive) return true;
    const bool bNavOffset = bGoalOnNav && (FVector::Dist2D(Goal, Location) > 40.f || Goal.Z > Location.Z + 20.f);
    if (Creature && (bNavOffset || !bGoalOnNav) && bHasEvidence
        && !Creature->HunterTraversal->IsTraversing())
    {
        const float Now = GetWorld()->GetTimeSeconds();
        const FVector Here = Creature->GetActorLocation();
        const FString Key = RecoveryKey(TEXT("Crawl"), Here, Location);
        const float* Until = RecoveryCooldowns.Find(Key);
        if (!Until || *Until <= Now)
        {
            ++RecoveryAttempts;
            NoteRecovery(TEXT("nav evidence is outside the passage; checking its actual aperture"));
            if (Creature->HunterTraversal->StartSqueezeToward(Location))
            {
                RecoveryFinalGoal = Goal = Location; bHasGoal = true;
                RecoveryStartedAt = Here; RecoveryInitialDistance = FVector::Dist(Here, Location);
                RecoveryTraversalAborts = Creature->HunterTraversal->AbortedTransfers;
                RecoveryMode = TEXT("Crawl"); ActiveRecoveryKey = Key; bRecoveryTraversal = true;
                ProgressAt = Now; ProgressAnchor = Creature->GetActorLocation();
                NextRecoveryTry = Now + 2.5f;
                LastDecision = TEXT("Crawl: following the real evidence through a fitted aperture");
                return true;
            }
            RecoveryCooldowns.Add(Key, Now + 3.f);
        }
    }
    if (bGoalOnNav) return true;
    if (!Creature) return false;
    auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
    FNavLocation Point;
    const bool bProjected = Nav && Nav->ProjectPointToNavigation(Location, Point, FVector(700, 700, 500));
    if (bProjected && SetGoal(Point.Location)) return true;
    // A player in a child passage or beyond a shut route still has a nearest
    // reachable approach. Walk the safe prefix of that route instead of
    // remaining in Pursue with no goal forever. Every accepted prefix is
    // checked against the same locked and boarded leaves as a full route.
    const auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(this,
        Creature->GetActorLocation(), bProjected ? Point.Location : Location, this);
    if (!Path || !Path->IsValid() || Path->PathPoints.Num() < 2) return false;
    TArray<FVector> Prefix{Path->PathPoints[0]};
    FVector Safe = FVector::ZeroVector;
    bool bFoundSafe = false, bBlocked = false;
    for (int32 I = 0; I + 1 < Path->PathPoints.Num() && !bBlocked; ++I)
    {
        const FVector A = Path->PathPoints[I], B = Path->PathPoints[I + 1];
        const int32 Samples = FMath::Max(1, FMath::CeilToInt(FVector::Dist(A, B) / 65.f));
        for (int32 S = 1; S <= Samples; ++S)
        {
            const FVector Candidate = FMath::Lerp(A, B, float(S) / Samples);
            Prefix.Add(Candidate);
            if (PathCrossesShut(Prefix)) { bBlocked = true; Prefix.Pop(); break; }
            FVector Projected;
            if (FVector::Dist2D(Candidate, Creature->GetActorLocation()) > 110.f
                && IsReachable(Candidate, Projected))
            { Safe = Projected; bFoundSafe = true; }
            Prefix.Pop();
        }
        Prefix.Add(B);
    }
    if (!bFoundSafe || !SetGoal(Safe)) return false;
    LastDecision = TEXT("Approaching the reachable side of the evidence");
    return true;
}

bool ATallowHunterAIController::SetGoal(const FVector& InLocation)
{
    FVector Location = InLocation;
    FVector Shaded;
    if (!bRecoveryGoalRequest && !bShadowInternalGoal && SelectShadowGoal(Location, Shaded)) Location = Shaded;
    CacheDoors();
    if (Creature)
        for (const FDoorWatch& Watch : Doors)
            if (Watch.Door.IsValid() && Watch.Door->bIsLocked && FVector::Dist2D(Location, Watch.Center) < 150.f
                && FMath::Abs(Location.Z - Watch.Center.Z) < 200.f)
            {
                FVector Back = (Creature->GetActorLocation() - Watch.Center).GetSafeNormal2D();
                if (Back.IsNearlyZero()) Back = (Location - Watch.Center).GetSafeNormal2D();
                Location = Watch.Center + Back * 160.f;
                Location.Z = InLocation.Z;
                break;
            }
    FVector Projected;
    float PathLength = 0.f;
    if (!IsReachable(Location, Projected, &PathLength)) { bHasGoal = false; return false; }
    // Seeing or hearing the same stationary target is fresh evidence, but it
    // is not physical progress. Resetting this every breath hid every stall.
    if (bHasGoal && !bRecoveryGoalRequest && FVector::Dist(Goal, Projected) < 65.f) return true;
    Goal = Projected;
    bHasGoal = true;
    if (!bRecoveryGoalRequest)
    {
        bRecoveryDetour = bRecoveryTraversal = bRecoveryWaiting = false;
        bRecoveryCheckedTranslation = false;
        RecoveryMode = NAME_None;
        RecoveryFloorQueue.Reset();
    }
    NextMoveRequest = 0.f;
    ProgressAt = GetWorld()->GetTimeSeconds();
    ProgressAnchor = Creature->GetActorLocation();
    GoalDeadline = ProgressAt + FMath::Clamp(PathLength / 50.f + 5.f, 8.f, 80.f);
    return true;
}

FString ATallowHunterAIController::RecoveryKey(FName Mode, const FVector& Here, const FVector& FinalGoal) const
{
    return FString::Printf(TEXT("%s:%d,%d,%d>%d,%d,%d"), *Mode.ToString(),
        FMath::RoundToInt(Here.X / 150.f), FMath::RoundToInt(Here.Y / 150.f), FMath::RoundToInt(Here.Z / 150.f),
        FMath::RoundToInt(FinalGoal.X / 200.f), FMath::RoundToInt(FinalGoal.Y / 200.f), FMath::RoundToInt(FinalGoal.Z / 200.f));
}

void ATallowHunterAIController::NoteRecovery(const FString& Message)
{
    RecoveryLog.Add(FString::Printf(TEXT("%.1f %s"), GetWorld()->GetTimeSeconds(), *Message));
    if (RecoveryLog.Num() > 60) RecoveryLog.RemoveAt(0);
}

void ATallowHunterAIController::NoteTactical(const FString& Message)
{
    TacticalLog.Add(FString::Printf(TEXT("%.1f %s"), GetWorld()->GetTimeSeconds(), *Message));
    if (TacticalLog.Num() > 30) TacticalLog.RemoveAt(0);
}

bool ATallowHunterAIController::TacticalPathClear(const FVector& Destination) const
{
    if (!Creature) return false;
    const auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
        const_cast<ThisClass*>(this), Creature->GetActorLocation(), Destination, const_cast<ThisClass*>(this));
    if (!Path || !Path->IsValid() || Path->IsPartial()
        || Path->PathPoints.IsEmpty() || const_cast<ThisClass*>(this)->PathCrossesShut(Path->PathPoints)) return false;
    const auto* Capsule = Creature->GetCapsuleComponent();
    const float Radius = Capsule->GetScaledCapsuleRadius(), HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    // A raised sphere clips a lintel that the rounded, fitted capsule can
    // leave safely. Preserve the real current floor offset at the departure.
    FVector Previous = Creature->GetActorLocation();
    const FVector Lift(0.f, 0.f, Previous.Z - Path->PathPoints[0].Z);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HunterTacticalBranch), false, Creature);
    Query.AddIgnoredActors(Capsule->GetMoveIgnoreActors());
    TArray<AActor*> Attached; Creature->GetAttachedActors(Attached); Query.AddIgnoredActors(Attached);
    for (int32 I = 0; I + 1 < Path->PathPoints.Num(); ++I)
    {
        const FVector A = Path->PathPoints[I], B = Path->PathPoints[I + 1];
        const FVector Nearest = FMath::ClosestPointOnSegment(TacticalStruggleAt, A, B);
        // Returning to the same choke point is not an alternate intercept.
        if (FMath::Abs(Nearest.Z - TacticalStruggleAt.Z) < 150.f
            && FVector::Dist2D(Nearest, TacticalStruggleAt) < 95.f) return false;
        const FVector Next = B + Lift;
        if (GetWorld()->SweepTestByProfile(Previous, Next, FQuat::Identity,
            Capsule->GetCollisionProfileName(), FCollisionShape::MakeCapsule(Radius, HalfHeight), Query)) return false;
        Previous = Next;
    }
    return true;
}

bool ATallowHunterAIController::PlanTacticalIntercept(float Now)
{
    EvidenceLocation = TacticalMemory;
    bHasEvidence = true;
    SearchUntil = FMath::Max(SearchUntil, Now + 25.f);
    // These existing planners use remembered evidence and authored exits, never
    // an unseen pawn's present position. Reject a plan through the failed gap.
    if ((AmbushExit(TacticalMemory) || RequestAmbushFromEvidence()) && bHasGoal && TacticalPathClear(Goal))
    {
        TacticalPhase = TEXT("Intercepting"); TacticalUntil = Now + 16.f;
        NoteTactical(TEXT("backout clear; intercepting remembered exit at ") + Goal.ToCompactString());
        LastDecision = TEXT("Struggle: intercept the remembered way out");
        return true;
    }
    CancelSpiderPlans(); bTravellingToAmbush = false; bHasGoal = false;
    CacheDoors();
    float Best = MAX_flt; FVector Destination = FVector::ZeroVector;
    auto Consider = [&](const FVector& Candidate, float Penalty)
    {
        FVector Projected; float Cost;
        if (!IsReachable(Candidate, Projected, &Cost) || !TacticalPathClear(Projected)) return;
        if (FVector::Dist2D(Creature->GetActorLocation(), Projected) < 130.f) return;
        const float Score = Cost + FVector::Dist2D(Projected, TacticalMemory) * .45f + Penalty;
        if (Score < Best) { Best = Score; Destination = Projected; }
    };
    for (const FDoorWatch& Watch : Doors)
        if (Watch.Door.IsValid() && !IsBarred(Watch.Door.Get()))
        {
            const FVector Across = Watch.Door->GetActorRightVector();
            Consider(Watch.Center + Across * 140.f, 0.f);
            Consider(Watch.Center - Across * 140.f, 0.f);
        }
    for (TActorIterator<ATallowHunterSearchAnchor> It(GetWorld()); It; ++It)
        if (It->bEnabled && It->AnchorType == ETallowHunterAnchorType::Ambush) Consider(It->GetActorLocation(), 80.f);
    for (TActorIterator<ATallowPatrolPoint> It(GetWorld()); It; ++It) Consider(It->GetActorLocation(), 350.f);
    EnterMode(ETallowHuntMode::Withdraw);
    if (Best < MAX_flt && SetGoal(Destination))
    {
        TacticalPhase = TEXT("Intercepting"); TacticalUntil = Now + 16.f;
        NoteTactical(TEXT("backout clear; taking another known approach at ") + Goal.ToCompactString());
        LastDecision = TEXT("Struggle: circle to another known approach");
        return true;
    }
    TacticalPhase = NAME_None;
    bHasGoal = false; NextDecision = Now + 1.5f;
    NoteTactical(TEXT("no clear alternate exit; withdrew safely and will search remembered evidence"));
    LastDecision = TEXT("Struggle: alternate exits unavailable, regrouping");
    return false;
}

bool ATallowHunterAIController::UpdateTacticalRetreat(float Now, float DeltaTime)
{
    if (!Creature || !Player || HuntMode == ETallowHuntMode::Captured) return false;
    auto* Traversal = Creature->HunterTraversal.Get();
    const FVector Here = Creature->GetActorLocation();
    if (TacticalPhase != NAME_None)
    {
        if (bVisible)
        {
            Observe(Player->GetActorLocation(), 1.f, true);
            LastSeenAt = Now; TacticalMemory = EvidenceLocation;
        }
        if (TacticalPhase == TEXT("WatchingEscape"))
        {
            StopMovement(); Creature->HunterMotion->LockGaze(TacticalMemory + FVector(0, 0, 55), .4f);
            if (Now < TacticalUntil) return true;
            Traversal->SetTacticalPaused(false);
            EnterMode(ETallowHuntMode::Withdraw);
            const float BackoutSpeed = FMath::Max(12.f, 150.f * .52f
                * FMath::Min(Traversal->GetRouteSpeedScale(), Creature->HunterMotion->GetFitSpeed()));
            TacticalPhase = TEXT("BackingOut");
            TacticalUntil = Now + FMath::Clamp(FVector::Dist(Here, TacticalBackout) / BackoutSpeed + 4.f, 12.f, 40.f);
            if (Traversal->IsTraversing()) Traversal->RequestGroundExit();
            else bTacticalBackoutGoal = SetGoalNearest(TacticalBackout);
            NoteTactical(TEXT("watch finished; reversing the actual approach with fitted capsule"));
            LastDecision = TEXT("Struggle: pulling back out");
            return true;
        }
        if (TacticalPhase == TEXT("BackingOut"))
        {
            Creature->HunterMotion->LockGaze(TacticalMemory + FVector(0, 0, 55), .4f);
            if (Traversal->IsTraversing())
            {
                if (Now > TacticalUntil)
                {
                    // A newly closed solid barrier still wins. The traversal
                    // continues its checked local exits; never teleport out.
                    TacticalPhase = NAME_None; NextRecoveryTry = Now + 2.f;
                    NoteTactical(TEXT("return obstructed dynamically; continuing checked physical recovery"));
                }
                return true;
            }
            if (FVector::Dist2D(Here, TacticalBackout) > 55.f && Now < TacticalUntil)
            {
                if (!bTacticalBackoutGoal) bTacticalBackoutGoal = SetGoalNearest(TacticalBackout);
                if (bTacticalBackoutGoal) MoveToLocation(Goal, 12.f, false, true, false, true, nullptr, false);
                return true;
            }
            StopMovement(); bTacticalBackoutGoal = false;
            PlanTacticalIntercept(Now);
            return true;
        }
        if (TacticalPhase == TEXT("Intercepting"))
        {
            if (!bHasGoal || Now > TacticalUntil)
            {
                TacticalPhase = NAME_None;
                NoteTactical(TEXT("alternate approach finished; resuming ordinary evidence decisions"));
                return false;
            }
            UpdateAttack(Now);
            if (HuntMode != ETallowHuntMode::Captured) UpdateMove(Now);
            return true;
        }
    }
    const bool bCompressed = Traversal->IsSqueezing() && Traversal->GetSqueezeSeverity() > .65f;
    const bool bStuck = !Traversal->IsTraversing() && (bHasGoal || bRecoveryWaiting) && RecoveryAttempts > 0
        && Now - ProgressAt > 1.8f && Creature->GetVelocity().Size2D() < 15.f;
    if (!bCompressed && !bStuck)
    {
        if (Creature->GetVelocity().Size2D() > 50.f || Now - LastStruggleAt > 4.f)
        { StruggleSince = -1.f; StruggleClosest = MAX_flt; }
        return false;
    }
    LastStruggleAt = Now;
    if (StruggleSince < 0.f) { StruggleSince = Now; StruggleClosest = MAX_flt; }
    const bool bFreshEvidence = bVisible || Now - LastSeenAt < 5.f || Now - LastHeardAt < 5.f;
    if (!bFreshEvidence || !bHasEvidence) return false;
    const FVector Known = bVisible ? Player->GetActorLocation() : EvidenceLocation;
    const float Distance = FVector::Dist2D(Here, Known);
    StruggleClosest = FMath::Min(StruggleClosest, Distance);
    if (Now < NextTacticalThought || Now - StruggleSince < 3.2f || Distance < 280.f
        || Distance - StruggleClosest < 130.f) return false;
    NextTacticalThought = Now + 8.f;
    const float Chance = FMath::Clamp(StruggleRetreatChance * (bStuck ? 1.35f : 1.f), 0.f, 1.f);
    if (Random.FRand() >= Chance) return false;
    TacticalMemory = Known; TacticalStruggleAt = Here;
    TacticalBackout = Traversal->IsTraversing() ? Traversal->GetRouteStartLocation()
        : Here + (Here - Known).GetSafeNormal2D() * 220.f;
    TacticalPhase = TEXT("WatchingEscape"); TacticalUntil = Now + Random.FRandRange(.85f, 1.35f);
    NextTacticalThought = Now + Random.FRandRange(40.f, 65.f);
    bTacticalBackoutGoal = false;
    bRecoveryDetour = bRecoveryTraversal = bRecoveryWaiting = false; RecoveryFloorQueue.Reset();
    RecoveryCooldowns.Add(RecoveryKey(TEXT("Crawl"), Here, Known), Now + 20.f);
    bHasGoal = false; StopMovement();
    Traversal->SetAttackPaused(false); Traversal->SetTacticalPaused(true);
    AttackStartedAt = -1.f; AttackWindupAlpha = 0.f;
    ++TacticalRetreats;
    LastDecision = TEXT("Struggle: watching her escape before backing out");
    NoteTactical(FString::Printf(TEXT("genuine %s for %.1fs, evidence distance grew %.0fcm; watch then reverse"),
        bCompressed ? TEXT("compression") : TEXT("blockage"), Now - StruggleSince, Distance - StruggleClosest));
    return true;
}

bool ATallowHunterAIController::TryRecoverMove(float Now, const FVector& FinalGoal)
{
    if (!Creature || Now < NextRecoveryTry || Creature->HunterTraversal->IsTraversing()
        || PendingLink != INDEX_NONE || bOnLink || HuntMode == ETallowHuntMode::Captured
        || AttackStartedAt >= 0.f || FVector::Dist(Creature->GetActorLocation(), FinalGoal) < 1.f) return false;
    return UpdateRecoveryPlanner(Now, FinalGoal);
}

void ATallowHunterAIController::BeginSearch()
{
    StopMovement();
    bHasGoal = false;
    InspectionSpot = nullptr;
    SearchAnchor = nullptr;
    EnterMode(ETallowHuntMode::Search);
    const float Now = GetWorld()->GetTimeSeconds();
    SearchStartedAt = Now;
    SearchSteps = 0;
    if (SearchUntil < Now + 5.f) SearchUntil = Now + (bSecondPhase ? 27.f : 19.f);
    InspectUntil = Now + Random.FRandRange(.8f, 1.6f);
    Creature->HunterMotion->LookAt(EvidenceLocation + FVector(0, 0, 75), 1.5f);
}

bool ATallowHunterAIController::ChooseSearchPoint()
{
    const float Now = GetWorld()->GetTimeSeconds();
    float BestScore = -MAX_flt;
    FVector Best = FVector::ZeroVector;
    ATallowHideVolume* BestSpot = nullptr;
    ATallowHunterSearchAnchor* BestAnchor = nullptr;
    // Environment authored approaches improve the shape of a search. Scoring
    // uses the last evidence and previous visits, never secret occupancy.
    for (TActorIterator<ATallowHunterSearchAnchor> It(GetWorld()); It; ++It)
    {
        if (!It->bEnabled || It->AnchorType == ETallowHunterAnchorType::Squeeze) continue;
        const FVector P = It->GetActorLocation();
        const float Distance = FVector::Dist2D(P, EvidenceLocation);
        if (Distance > 750.f || FMath::Abs(P.Z - EvidenceLocation.Z) > 180.f) continue;
        const float* Visit = AnchorVisits.Find(*It);
        if (Visit && Now - *Visit < 28.f) continue;
        FVector Projected; float PathLength;
        if (!IsReachable(P, Projected, &PathLength)) continue;
        const float Direction = FVector::DotProduct((P - EvidenceLocation).GetSafeNormal2D(), EvidenceHeading);
        const float Score = 330.f * It->Priority - Distance * .28f - PathLength * .08f
            + Direction * 90.f + FamiliarityAt(P) * 12.f + Random.FRandRange(0, 70.f);
        if (Score > BestScore) { BestScore = Score; Best = Projected; BestAnchor = *It; }
    }
    // Inspection candidates come from the environment, irrespective of whether
    // any of them contains the player. No occupancy query enters selection.
    for (TActorIterator<ATallowHideVolume> It(GetWorld()); It; ++It)
    {
        const FVector P = It->GetExitPoint()->GetComponentLocation();
        if (FVector::Dist2D(P, EvidenceLocation) > 650.f || FMath::Abs(P.Z - EvidenceLocation.Z) > 180.f) continue;
        const float* Visit = SpotVisits.Find(*It);
        if (Visit && Now - *Visit < 40.f) continue;
        FVector Projected;
        if (!IsReachable(P, Projected)) continue;
        const float Score = 450.f - FVector::Dist2D(P, EvidenceLocation) * .5f + Random.FRandRange(0.f, 160.f);
        if (Score > BestScore) { BestScore = Score; Best = Projected; BestSpot = *It; BestAnchor = nullptr; }
    }
    // Where would the kid go from there? Out of the room (a door it can use), or toward what she needs (the fuses).
    FName Reason = NAME_None; AActor* ReasonDoor = nullptr;
    CacheDoors();
    for (const FDoorWatch& Watch : Doors)
    {
        if (!Watch.Door.IsValid() || IsBarred(Watch.Door.Get())) continue;
        const float D = FVector::Dist2D(Watch.Center, EvidenceLocation);
        if (D > 700.f || FMath::Abs(Watch.Center.Z - EvidenceLocation.Z) > 200.f) continue;
        const float* Visit = SpotVisits.Find(Watch.Door.Get());
        if (Visit && Now - *Visit < 30.f) continue;
        FVector Projected;
        if (!IsReachable(Watch.Center, Projected)) continue;
        const float Score = 380.f - D * .3f + Random.FRandRange(0.f, 120.f);
        if (Score > BestScore) { BestScore = Score; Best = Projected; BestSpot = nullptr; BestAnchor = nullptr; Reason = TEXT("CheckExit: where would it go"); ReasonDoor = Watch.Door.Get(); }
    }
    {
        const AActor* Wanted = nullptr; float WantedDistance = 2500.f;
        for (TActorIterator<ATallowFusePickup> It(GetWorld()); It; ++It)
            if (!It->IsHidden() && FVector::Dist(It->GetActorLocation(), EvidenceLocation) < WantedDistance) { WantedDistance = FVector::Dist(It->GetActorLocation(), EvidenceLocation); Wanted = *It; }
        if (Wanted)
        {
            FVector Projected;
            const FVector Ahead = EvidenceLocation + (Wanted->GetActorLocation() - EvidenceLocation).GetSafeNormal2D() * 380.f;
            if (IsReachable(Ahead, Projected) && FMath::Abs(Projected.Z - EvidenceLocation.Z) < 180.f)
            {
                const float Score = 330.f + Random.FRandRange(0.f, 120.f);
                if (Score > BestScore) { BestScore = Score; Best = Projected; BestSpot = nullptr; BestAnchor = nullptr; Reason = TEXT("CutOff: toward the fuses"); ReasonDoor = nullptr; }
            }
        }
    }
    auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
    for (int32 Attempt = 0; Attempt < 12 && Nav; ++Attempt)
    {
        const float Angle = Random.FRandRange(0.f, 2.f * PI);
        const float Radius = Random.FRandRange(140.f, RecentSearchPoints.Num() < 2
            ? 330.f : bSecondPhase ? 800.f : 560.f);
        const FVector Candidate = EvidenceLocation + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Radius;
        FVector P;
        if (!IsReachable(Candidate, P) || FMath::Abs(P.Z - EvidenceLocation.Z) > 180.f) continue;
        float Novelty = 400.f;
        for (const FVector& Previous : RecentSearchPoints)
            Novelty = FMath::Min(Novelty, float(FVector::Dist2D(P, Previous)));
        if (Novelty < 120.f) continue;
        const float Direction = FVector::DotProduct((P - EvidenceLocation).GetSafeNormal2D(), EvidenceHeading);
        const float Score = Novelty * .45f + Direction * 90.f + Random.FRandRange(0.f, 120.f);
        if (Score > BestScore) { BestScore = Score; Best = P; BestSpot = nullptr; BestAnchor = nullptr; Reason = NAME_None; ReasonDoor = nullptr; }
    }
    if (BestScore == -MAX_flt) return false;
    InspectionSpot = BestSpot;
    SearchAnchor = BestAnchor;
    RecentSearchPoints.Add(Best);
    if (RecentSearchPoints.Num() > 8) RecentSearchPoints.RemoveAt(0);
    EnterMode(ETallowHuntMode::Search);
    if (ReasonDoor) SpotVisits.Add(ReasonDoor, Now);
    LastDecision = Reason != NAME_None ? Reason : BestSpot ? FName(TEXT("Search: check a hiding place")) : BestAnchor
        ? FName(TEXT("Search: check a corner")) : FName(TEXT("Search: sweep the area"));
    return SetGoal(Best);
}

bool ATallowHunterAIController::HasAmbushCover(const FVector& Location) const
{
    const auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
    FNavLocation EvidenceFloor, WaitFloor;
    if (!Nav || !Nav->ProjectPointToNavigation(EvidenceLocation, EvidenceFloor, FVector(120, 120, 250))
        || !Nav->ProjectPointToNavigation(Location, WaitFloor, FVector(60, 60, 160))) return false;
    // Native standing eye measured 136-137 cm in the configured player.
    // A fixed conservative height needs no information about the hidden pawn.
    const FVector From = EvidenceFloor.Location + FVector(0, 0, 140);
    const FVector Side = FVector::CrossProduct(FVector::UpVector,
        (WaitFloor.Location - EvidenceFloor.Location).GetSafeNormal2D()) * 45.f;
    // The low spider body: about 90 cm tall and a metre wide. A chair hiding one low point does not hide the rest.
    const FVector Offsets[] = {FVector(0, 0, 30), FVector(0, 0, 60),
        FVector(0, 0, 90), FVector(0, 0, 60) + Side, FVector(0, 0, 60) - Side};
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HunterAmbushCover), true, Creature);
    Query.AddIgnoredActor(Player);
    for (const FVector& Offset : Offsets)
    {
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByChannel(Hit, From, WaitFloor.Location + Offset,
            ECC_Visibility, Query)) return false;
    }
    return true;
}

bool ATallowHunterAIController::RequestAmbushFromEvidence()
{
    if (!Creature || !bUseAmbushes || !bHasEvidence || bVisible
        || HuntMode == ETallowHuntMode::Captured) return false;
    const float Now = GetWorld()->GetTimeSeconds();
    const auto* Overseer = GetWorld()->GetSubsystem<UTallowHunterDirector>();
    const bool bHunting = Overseer && Overseer->GetAggression() > .3f;
    if (Now < NextAmbushAt || (Now > SearchUntil && !bHunting) || Observations.IsEmpty()) return false;
    ATallowHunterSearchAnchor* Best = nullptr;
    float BestScore = -MAX_flt;
    for (TActorIterator<ATallowHunterSearchAnchor> It(GetWorld()); It; ++It)
    {
        if (!It->bEnabled || It->AnchorType != ETallowHunterAnchorType::Ambush) continue;
        const FVector P = It->GetActorLocation();
        const float D = FVector::Dist2D(P, EvidenceLocation);
        if (D < 130.f || D > (bHunting ? 1100.f : 750.f) || FMath::Abs(P.Z - EvidenceLocation.Z) > 160.f) continue;
        const float* Visit = AnchorVisits.Find(*It);
        if (Visit && Now - *Visit < 35.f) continue;
        // Cover is checked against the remembered approach, never today's
        // hidden player position.
        if (!HasAmbushCover(P)) continue;
        FVector Projected; float Path;
        if (!IsReachable(P, Projected, &Path) || Path > 1500.f) continue;
        const float Direction = FVector::DotProduct((P - EvidenceLocation).GetSafeNormal2D(), EvidenceHeading);
        const float Score = It->Priority * 150.f + Direction * 70.f + FamiliarityAt(P) * 10.f
            - FMath::Abs(D - 340.f) * .2f - Path * .08f;
        if (Score > BestScore) { BestScore = Score; Best = *It; }
    }
    if (!Best) { LastDecision = TEXT("NoCoveredReachableAmbush"); return false; }
    if (!SetGoal(Best->GetActorLocation())) { LastDecision = TEXT("AmbushPathUnavailable"); return false; }
    SearchAnchor = Best;
    InspectionSpot = nullptr;
    AmbushFocus = Best->GetFocusLocation();
    bTravellingToAmbush = true;
    // Separate travel and wait deadlines. An unreachable hide never stalls
    // the hunter indefinitely, and a failed ambush still consumes cooldown.
    AmbushUntil = Now + 18.f;
    SearchUntil = FMath::Max(SearchUntil, AmbushUntil + 5.f);
    NextAmbushAt = Now + 35.f;
    ++AmbushesStarted;
    EnterMode(ETallowHuntMode::Ambush);
    LastDecision = TEXT("WaitAtRememberedExit");
    return true;
}

bool ATallowHunterAIController::CanCapturePlayer() const
{
    return !GetCaptureEvidence().IsNone();
}

FName ATallowHunterAIController::GetCaptureEvidence() const
{
    if (!Creature || !Player || Player->IsTiedToChair()) return NAME_None;
    if (Creature->HunterTraversal && Creature->HunterTraversal->IsTraversing()
        && !(Creature->HunterTraversal->IsSqueezing() && Creature->GetActorUpVector().Z > .9f)) return NAME_None;
    const bool bInspectedEvidence = HuntMode == ETallowHuntMode::Inspect && InspectionSpot
        && Player->GetCurrentHideVolume() == InspectionSpot
        && KnownOccupiedHide.Get() == InspectionSpot;
    const float Reach = FVector::Dist(Creature->GetActorLocation(), Player->GetActorLocation());
    // Silence does not prevent real contact, but the old 95 cm centre-distance
    // test could grab a motionless player without touching them.
    const auto* TheirCapsule = Creature->GetCapsuleComponent();
    const auto* HerCapsule = Player->GetCapsuleComponent();
    const float ContactReach = FMath::Min(BumpReach,
        TheirCapsule->GetScaledCapsuleRadius() + HerCapsule->GetScaledCapsuleRadius() + 4.f);
    const bool bTouching = Reach <= ContactReach;
    const bool bHeard = HearsBreathing(Reach);
    const bool bInTheDark = IsDark() && !Player->IsHiddenInSpot()
        && ((bHeard && Reach < DarkGrabReach) || bTouching);
    // Recognition memory can guide a search; it cannot authorize a grab.
    if (!bDirectPlayerSight && !bInspectedEvidence && !bInTheDark) return NAME_None;
    if (Reach > (bInTheDark ? DarkGrabReach : 200.f)) return NAME_None;
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HunterCapture), true, Creature);
    if (bInspectedEvidence) Query.AddIgnoredActor(InspectionSpot);
    FVector Eye; FRotator Facing;
    Creature->GetActorEyesViewPoint(Eye, Facing);
    Eye = HeadLocation();
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Player->GetPawnViewLocation(), ECC_Visibility, Query);
    if (const_cast<ATallowHunterAIController*>(this)->BoardsBetween(Eye, Player->GetPawnViewLocation())) return NAME_None;
    if (bHit && Hit.GetActor() != Player) return NAME_None;
    return bInspectedEvidence ? FName(TEXT("KnownHidingPlace"))
        : bDirectPlayerSight ? FName(TEXT("FreshSight"))
        : bHeard ? FName(TEXT("HeardBreathing")) : FName(TEXT("PhysicalContact"));
}

bool ATallowHunterAIController::IsDark() const
{
    return Player && Player->GetCandleComponent() && Player->GetCandleComponent()->GetCandleState() == ECandleState::Out;
}

bool ATallowHunterAIController::HearsBreathing(float Distance) const
{
    return Creature && Player && Distance < BreathHearing && !Player->IsHoldingStill()
        && (HasClearTrace(HeadLocation(), Player->GetPawnViewLocation())
            || HasClearTrace(HeadLocation(), Player->GetActorLocation()));
}

bool ATallowHunterAIController::TryCapturePlayer()
{
    UpdateAttack(GetWorld()->GetTimeSeconds());
    return HuntMode == ETallowHuntMode::Captured;
}

void ATallowHunterAIController::UpdateAttack(float Now)
{
    if (!bAllowCapture || !CanCapturePlayer())
    {
        if (Creature && Creature->HunterTraversal) Creature->HunterTraversal->SetAttackPaused(false);
        if (AttackStartedAt >= 0.f) ++AbortedGrabs;
        AttackStartedAt = -1.f;
        AttackWindupAlpha = 0.f;
        return;
    }
    if (AttackStartedAt < 0.f)
    {
        AttackStartedAt = Now;
        LastDecision = TEXT("ReachBeforeGrab");
        UE_LOG(LogTemp, Display, TEXT("HUNTER_CAPTURE evidence=%s hidden=%d breathheld=%d distance=%.1f"),
            *GetCaptureEvidence().ToString(), Player->IsHiddenInSpot(), Player->IsHoldingStill(),
            FVector::Dist(Creature->GetActorLocation(), Player->GetActorLocation()));
    }
    AttackWindupAlpha = FMath::Clamp((Now - AttackStartedAt) / FMath::Max(CaptureWindupSeconds, .15f), 0.f, 1.f);
    if (Creature->HunterTraversal) Creature->HunterTraversal->SetAttackPaused(true);
    StopMovement();
    if (AttackWindupAlpha >= 1.f)
    {
        // Revalidate at the end of the visible reach, including a fresh wall
        // trace. Closing a door or escaping the reach must cancel the grab.
        if (CanCapturePlayer())
            if (auto* Mode = Cast<ATallowGameMode>(UGameplayStatics::GetGameMode(this)))
            {
                FireEvent(ETallowKeeperEvent::Capture);
                Mode->HandlePlayerCaptured();
            }
    }
}

void ATallowHunterAIController::UpdateMove(float Now)
{
    UpdateRecoveryFitAssist(Now);
    if (UpdateLinks(Now)) return;
    auto* Traversal = Creature->HunterTraversal.Get();
    if (!bUseMovementContract && bRecoveryCheckedTranslation && !Traversal->IsTraversing())
    {
        if (AttackStartedAt >= 0.f || HuntMode == ETallowHuntMode::Captured) return;
        StopMovement();
        const bool bArrived = bHasGoal && FVector::Dist2D(Creature->GetActorLocation(), Goal) < 12.f;
        FString Why;
        if (!bArrived && bHasGoal && Creature->HunterMotion->TryCheckedRecoveryStep(Goal, Why))
        {
            ProgressAt = Now; GoalDeadline = FMath::Max(GoalDeadline, Now + 4.f);
            LastDecision = TEXT("Route recovery: checked retreat preserving body heading");
            return;
        }
        if (!bArrived && Now - ProgressAt < .35f) return;
        if (!bArrived) RejectRecoveryConnection(Why.IsEmpty() ? TEXT("checked retreat blocked") : Why);
        else NoteRecovery(TEXT("checked retreat completed; replanning from actual pose"));
        bRecoveryCheckedTranslation = false; bRecoveryDetour = false; bHasGoal = false;
        RecoveryFloorQueue.Reset();
        bRecoverySearchActive = bRecoverySearchFinished = false;
        bRecoveryWaiting = true; NextRecoveryTry = Now;
        return;
    }
    auto TryFitRecoveryTurn = [&](const FVector& Destination)
    {
        if (bRecoverySearchFinished && !bRecoveryDetour && !bRecoveryTraversal
            && RecoveryPlannerStatus.StartsWith(TEXT("no checked"))) return false;
        auto* Motion = Creature->HunterMotion.Get();
        if (!Motion || !Motion->UsesMim() || Motion->IsFitPassable() || AttackStartedAt >= 0.f
            || Motion->GetFitBlockedSeconds() < .2f || Now < NextRecoveryTurnTry
            || Traversal->IsTraversing() || !Creature->GetCharacterMovement()->IsMovingOnGround()) return false;
        NextRecoveryTurnTry = Now + .05f;
        // Stopping at a narrow turn clears path-following acceleration, which
        // also stops ordinary facing changes. Recover only toward the next
        // verified path segment; the distant goal can lie across a wall.
        const FVector Here = Creature->GetActorLocation();
        const auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(this, Here, Destination, this);
        if (!Path || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.Num() < 2
            || PathCrossesShut(Path->PathPoints)) return false;
        for (int32 I = 0; I < Path->PathPoints.Num(); ++I)
        {
            const FVector Ahead = Path->PathPoints[I];
            if (FVector::Dist2D(Here, Ahead) < 12.f) continue;
            FString Reason;
            if (!Motion->TryCheckedRecoveryStep(Ahead, Reason, true))
            {
                if (Now >= NextRecoveryTry && !Reason.IsEmpty()) NoteRecovery(Reason);
                return false;
            }
            const FName Decision(TEXT("Route recovery: checked turn toward the next path segment"));
            if (LastDecision != Decision) NoteRecovery(TEXT("checked turn toward ") + Ahead.ToCompactString());
            LastDecision = Decision;
            ProgressAt = Now; GoalDeadline = FMath::Max(GoalDeadline, Now + 4.f);
            NextRecoveryTry = FMath::Max(NextRecoveryTry, Now + .3f);
            return true;
        }
        return false;
    };
    auto CanAdvanceMeasuredPose = [&](const FVector& Destination)
    {
        const auto* Motion = Creature->HunterMotion.Get();
        if (!Motion || AttackStartedAt >= 0.f || Traversal->IsTraversing()
            || !Creature->GetCharacterMovement()->IsMovingOnGround()) return false;
        const FVector Here = Creature->GetActorLocation();
        const auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(this, Here, Destination, this);
        if (!Path || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.Num() < 2
            || PathCrossesShut(Path->PathPoints)) return false;
        for (const FVector& Ahead : Path->PathPoints)
        {
            if (FVector::Dist2D(Here, Ahead) < 12.f) continue;
            // Turns still use the checked turn executor. This only permits
            // a measured forward step when rectangular pose bounds overstate
            // what actually occupies an irregular stair or furniture gap.
            if (FMath::Abs(FMath::FindDeltaAngleDegrees(Creature->GetActorRotation().Yaw,
                (Ahead - Here).Rotation().Yaw)) > 3.f) return false;
            FString Why;
            return Motion->CanCheckedGroundAdvance(Ahead, Why);
        }
        return false;
    };
    if (!bUseMovementContract && PendingLink != INDEX_NONE && !Traversal->IsTraversing() && Creature->HunterMotion
        && Creature->HunterMotion->UsesMim() && !Creature->HunterMotion->IsFitPassable()
        && Creature->HunterMotion->GetFitBlockedSeconds() >= .65f)
    {
        // A nav-reachable link entry can still be blocked by the actual
        // body. Release that pending plan so other checked routes can run.
        const FVector Final = bLinkFinalGoal ? LinkFinalGoal : (bHasEvidence ? EvidenceLocation : Goal);
        RejectRecoveryConnection(TEXT("link approach blocked by body fit"));
        LinkRetryAfter.Add(PendingLink, Now + 10.f);
        PendingLink = INDEX_NONE; bLinkFinalGoal = false;
        bHasGoal = false; // The obsolete link entry must not replace the retained final goal.
        RecoveryFinalGoal = Final; bRecoveryWaiting = true;
        NextRecoveryTry = Now;
        NoteRecovery(TEXT("link approach blocked by body fit; retaining evidence and checking other routes"));
    }
    if (bRecoveryTraversal && !Traversal->IsTraversing())
    {
        const float Remaining = FVector::Dist(Creature->GetActorLocation(), RecoveryFinalGoal);
        const bool bProgress = RecoveryInitialDistance - Remaining > 60.f
            && Traversal->AbortedTransfers == RecoveryTraversalAborts;
        if (bProgress)
        {
            ++RecoverySuccesses;
            NoteRecovery(RecoveryMode.ToString() + FString::Printf(TEXT(" progressed %.0f cm; resuming evidence goal"), RecoveryInitialDistance - Remaining));
        }
        else
        {
            RecoveryCooldowns.Add(ActiveRecoveryKey, Now + 10.f);
            RejectRecoveryConnection(TEXT("executed traversal aborted or made no progress"));
            NoteRecovery(RecoveryMode.ToString() + TEXT(" did not make progress; trying a different route"));
        }
        bRecoveryTraversal = false;
        const FVector Final = RecoveryFinalGoal;
        SetGoalNearest(Final);
        NextRecoveryTry = Now + .5f;
    }
    if (!bUseMovementContract && bRecoveryWaiting)
    {
        const float Arrival = bRecoveryDetour ? 10.f : HuntMode == ETallowHuntMode::Ambush ? 30.f : 95.f;
        if (bHasGoal && FVector::Dist2D(Creature->GetActorLocation(), Goal) < Arrival
            && FMath::Abs(Creature->GetActorLocation().Z - Goal.Z) < 150.f)
            bRecoveryWaiting = false; // Continue into the ordinary arrival logic below.
        const auto* Motion = Creature->HunterMotion.Get();
        if (bRecoveryWaiting && !Traversal->IsTraversing() && Motion && Motion->UsesMim()
            && (Motion->IsFitPassable() || CanAdvanceMeasuredPose(bHasGoal ? Goal : RecoveryFinalGoal)))
        {
            if (RecoveryFitClearSince < 0.f) RecoveryFitClearSince = Now;
            if (Now - RecoveryFitClearSince >= .2f)
            {
                const FVector Resume = bHasGoal ? Goal : RecoveryFinalGoal;
                FVector Verified; float PathLength = 0.f;
                if (IsReachable(Resume, Verified, &PathLength))
                {
                    // A temporary bad pose must not permanently disable a
                    // now-valid floor route. Keep the evidence and failed modes.
                    const TGuardValue<bool> InternalRequest(bRecoveryGoalRequest, true);
                    bRecoveryWaiting = false;
                    if (SetGoal(Verified))
                    {
                        ProgressAnchor = Creature->GetActorLocation(); ProgressAt = Now;
                        GoalDeadline = Now + FMath::Clamp(PathLength / 50.f + 5.f, 8.f, 80.f);
                        NextMoveRequest = Now;
                        NoteRecovery(TEXT("fit recovered; resuming validated floor goal ") + Goal.ToCompactString());
                        LastDecision = TEXT("Route recovery: body fits again, resuming the same goal");
                    }
                    else bRecoveryWaiting = true;
                }
            }
        }
        else RecoveryFitClearSince = -1.f;
        if (bRecoveryWaiting)
        {
            if (TryFitRecoveryTurn(bRecoveryDetour ? Goal : RecoveryFinalGoal)) return;
            if (Now >= NextRecoveryTry) TryRecoverMove(Now, RecoveryFinalGoal);
            return;
        }
    }
    RecoveryFitClearSince = -1.f;
    if (!bHasGoal)
    {
        if (bHasEvidence && (HuntMode == ETallowHuntMode::Pursue || HuntMode == ETallowHuntMode::Investigate))
            TryRecoverMove(Now, EvidenceLocation);
        return;
    }
    if (Creature->HunterTraversal->IsTraversing())
    {
        ProgressAt = Now;
        ProgressAnchor = Creature->GetActorLocation();
        GoalDeadline = FMath::Max(GoalDeadline, Now + 8.f);
        return;
    }
    if (AttackStartedAt >= 0.f) return;
    const float Distance = FVector::Dist2D(Creature->GetActorLocation(), Goal);
    // A route around a wall or up a staircase can initially lead away from
    // its destination. Detect a stalled body, not a growing straight line.
    if (FVector::Dist(Creature->GetActorLocation(), ProgressAnchor) > 25.f)
    {
        ProgressAnchor = Creature->GetActorLocation();
        ProgressAt = Now;
    }
    // MoveTo stops at acceptance radius plus the 34 cm capsule radius.
    // This must include that boundary or arrived searches never inspect.
    const bool bAmbush = HuntMode == ETallowHuntMode::Ambush;
    if (Distance < (bRecoveryDetour ? 10.f : bAmbush ? 30.f : 95.f)
        && FMath::Abs(Creature->GetActorLocation().Z - Goal.Z) < 150.f)
    {
        if (!bRecoveryDetour && !bRecoveryTraversal && ResumeShadowDestination(Now)) return;
        if (bRecoveryDetour)
        {
            if (!RecoveryFloorQueue.IsEmpty())
            {
                const FVector Next = RecoveryFloorQueue[0]; RecoveryFloorQueue.RemoveAt(0);
                const TGuardValue<bool> InternalRequest(bRecoveryGoalRequest, true);
                if (SetGoal(Next))
                {
                    const FVector FloorHere = Creature->GetActorLocation() - FVector::UpVector
                        * Creature->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
                    ActiveRecoveryKey = RecoveryConnectionKey(TEXT("Floor"), FloorHere, Next,
                        (Next - FloorHere).Rotation().Yaw);
                    const float Speed = FMath::Max(6.f, 190.f * Creature->HunterMotion->GetFitSpeed());
                    GoalDeadline = Now + FMath::Clamp(FVector::Dist(FloorHere, Next) / Speed + 8.f, 8.f, 180.f);
                    NoteRecovery(TEXT("near corner clear; continuing around to ") + Next.ToCompactString());
                    return;
                }
            }
            const float Progress = RecoveryInitialDistance - FVector::Dist(Creature->GetActorLocation(), RecoveryFinalGoal);
            if (Progress > 60.f) ++RecoverySuccesses;
            NoteRecovery(FString::Printf(TEXT("floor branch reached (%+.0f cm toward evidence); resuming original goal"), Progress));
            const FVector Final = RecoveryFinalGoal;
            bRecoveryDetour = false;
            SetGoalNearest(Final);
            NextRecoveryTry = Now + .5f;
            return;
        }
        bHasGoal = false;
        StopMovement();
        if (HuntMode == ETallowHuntMode::Investigate) BeginSearch();
        else if (HuntMode == ETallowHuntMode::Ambush)
        {
            bTravellingToAmbush = false;
            // The heap needs no cover (it hides in plain sight); a climb or crawl starts where it now stands.
            if (bDisguisePending) { bDisguiseArrived = true; DisguiseSettleBy = Now + 4.f; return; }
            // A watch post is chosen to be seen from her side, not covered.
            if (bWatchPending) { BeginWatch(Now); return; }
            if (bHoldPending)
            {
                if (!StartHold()) { EnterMode(ETallowHuntMode::Search); NextDecision = Now + .5f; }
                return;
            }
            if (!HasAmbushCover(Creature->GetActorLocation()))
            {
                EnterMode(ETallowHuntMode::Search);
                LastDecision = TEXT("ExposedAmbushRejected");
                NextDecision = Now + .5f;
                return;
            }
            // Long enough for her to walk into it (it used to give up 4 to 6 s after arriving).
            AmbushUntil = Now + Random.FRandRange(8.f, 14.f);
            if (SearchAnchor) AnchorVisits.Add(SearchAnchor, Now);
            Creature->HunterMotion->LookAt(AmbushFocus, AmbushUntil - Now);
        }
        else if (HuntMode == ETallowHuntMode::Search)
        {
            EnterMode(ETallowHuntMode::Inspect);
            // At a hiding place it lingers, head down by the gap (the near miss is the scare; the grab still needs
            // it to have seen her go in, or to hear her).
            InspectUntil = Now + (InspectionSpot ? Random.FRandRange(3.f, 4.5f) : Random.FRandRange(1.4f, 2.8f));
            if (InspectionSpot) { SpotVisits.Add(InspectionSpot, Now); ++InspectedSpots; }
            if (SearchAnchor) { AnchorVisits.Add(SearchAnchor, Now); ++InspectedSpots; }
            ++SearchSteps;
            const FVector Look = InspectionSpot ? InspectionSpot->GetActorLocation() + FVector(0, 0, 65)
                : SearchAnchor ? SearchAnchor->GetFocusLocation()
                : Creature->GetActorLocation() + Creature->GetActorForwardVector() * 200.f + FVector(0, 0, 10);
            Creature->HunterMotion->LookAt(Look, InspectUntil - Now);
        }
        return;
    }
    if (HoldForDoor(Now)) return;
    if (bUseMovementContract && Creature->HunterMotion && Creature->HunterMotion->UsesMim())
    {
        // The contract only measures the floor. The choice to go up the wall, over the ceiling, over furniture or
        // through a crawl is still the brain's (it used to return here first, so none of those ever happened).
        if (CVarMimVoluntarySurfaces.GetValueOnGameThread() != 0 && Creature->HunterMotion->IsFitPassable()
            && !bRecoveryFitAssist && !bRecoveryDetour && !bRecoveryTraversal && !bRecoveryWaiting
            && AttackStartedAt < 0.f && TryVoluntaryRoutes(Now, Distance)) return;
        // The contract writes its status every tick; keep that in MovementContractStatus and let the decision log
        // (and the body's door peeks and frame hauls, which key off it) keep the brain's own decision.
        const FName Decision = LastDecision;
        const bool bWasTraversing = Creature->HunterTraversal->IsTraversing();
        UpdateContractMovement(Now);
        LastDecision = !bWasTraversing && Creature->HunterTraversal->IsTraversing()
            ? FName(TEXT("Contract: the floor is blocked, climbing instead")) : Decision;
        return;
    }
    if (Creature->HunterMotion && Creature->HunterMotion->UsesMim())
    {
        const auto* Motion = Creature->HunterMotion.Get();
        const FVector Space = Motion->GetFitClearance();
        const FVector Pressure = Motion->GetFitAxisWeights();
        bool bCannotFit = !Motion->IsFitPassable() && !bRecoveryFitAssist;
        if (bCannotFit)
        {
            if (TryFitRecoveryTurn(Goal)) return;
            bCannotFit = !CanAdvanceMeasuredPose(Goal);
        }
        const bool bCapsuleTooTall = Pressure.Z > .45f
            && Space.Z < Creature->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.f + 6.f;
        const bool bSlowFloor = Motion->GetFitSpeed() < .35f;
        if (!bRecoveryFitAssist && (bCannotFit || ((bCapsuleTooTall || bSlowFloor) && Distance > 150.f && Now >= NextRecoveryTry)))
        {
            const FVector Final = bRecoveryDetour ? RecoveryFinalGoal :
                (bHasEvidence && (HuntMode == ETallowHuntMode::Pursue || HuntMode == ETallowHuntMode::Investigate)
                    ? EvidenceLocation : Goal);
            if (bCannotFit)
            {
                FVector RecoveryAhead = Goal;
                if (bRecoveryDetour) GetRouteAhead(RecoveryAhead);
                StopMovement();
                if (bRecoveryDetour)
                {
                    FString StepReason;
                    if (Creature->HunterMotion->TryCheckedRecoveryStep(RecoveryAhead, StepReason))
                    {
                        ProgressAt = Now; GoalDeadline = FMath::Max(GoalDeadline, Now + 4.f);
                        NextMoveRequest = Now;
                        LastDecision = TEXT("Route recovery: checked body backout toward floor branch");
                        return;
                    }
                    if (Now >= NextRecoveryTry) NoteRecovery(StepReason);
                    if (Now - ProgressAt > 1.8f)
                    {
                        RecoveryCooldowns.Add(ActiveRecoveryKey, Now + 10.f);
                        RejectRecoveryConnection(StepReason);
                        NoteRecovery(TEXT("checked floor branch blocked; excluding it before replanning: ") + StepReason);
                        bRecoveryDetour = false; RecoveryFloorQueue.Reset();
                        NextRecoveryTry = Now;
                    }
                    else return;
                }
                if (Motion->GetFitBlockedSeconds() < .65f) return;
            }
            if (TryRecoverMove(Now, Final)) return;
            if (bCannotFit)
            {
                bRecoveryWaiting = true; RecoveryFinalGoal = Final;
                LastDecision = TEXT("BodyFit: no feasible floor passage, checking other remembered approaches");
                return;
            }
        }
    }
    if (bRecoveryFitAssist)
    {
        // Temporary fit recovery: ordinary swept path following keeps walls
        // solid while the visible folded body may overlap nearby furniture.
        if (Now >= NextMoveRequest)
        {
            MoveToLocation(Goal, 4.f, false, true, false, true, nullptr, false);
            NextMoveRequest = Now + .3f;
        }
        return;
    }
    if (Now - ProgressAt > 1.8f || Now > GoalDeadline)
    {
        StopMovement();
        const FVector Final = bRecoveryDetour ? RecoveryFinalGoal :
            (bHasEvidence && (HuntMode == ETallowHuntMode::Pursue || HuntMode == ETallowHuntMode::Investigate)
                ? EvidenceLocation : Goal);
        if (bRecoveryDetour)
        {
            RecoveryCooldowns.Add(ActiveRecoveryKey, Now + 10.f);
            RejectRecoveryConnection(TEXT("floor connection stalled"));
            NoteRecovery(TEXT("floor branch blocked; excluding it and keeping the same evidence"));
            bRecoveryDetour = false;
        }
        TryRecoverMove(Now, Final);
        ProgressAt = Now;
        return;
    }
    if (TryVoluntaryRoutes(Now, Distance)) return;
    if (Now >= NextMoveRequest)
    {
        MoveToLocation(Goal, bRecoveryDetour ? 4.f : bAmbush ? 12.f : 45.f, !bAmbush && !bRecoveryDetour,
            true, false, true, nullptr, false);
        NextMoveRequest = Now + .6f;
    }
}

bool ATallowHunterAIController::TryVoluntaryRoutes(float Now, float Distance)
{
    auto* Traversal = Creature->HunterTraversal.Get();
    if (TryLinkShortcut(Now)) { ++VoluntarySurfaceRoutes; return true; }
    // Too big for a cluttered floor ("Mim is huge... it needs to go on walls or roofs sometimes, or climb over
    // things"): where the walk round winds, it goes over the furniture, or up and across the ceiling.
    if (bUseSurfaceRoutes && Now >= NextOverTry && Distance > 250.f && Distance < 1300.f && !bDisguisePending
        && !bHoldPending && PendingLink == INDEX_NONE && !bTravellingToAmbush)
    {
        NextOverTry = Now + Random.FRandRange(2.5f, 4.f);
        FVector Unused; float PathLength = 0.f;
        if (IsReachable(Goal, Unused, &PathLength) && PathLength > Distance * 1.3f + 80.f)
        {
            const TCHAR* Why = nullptr;
            if (Traversal->StartOverRoute(Goal)) Why = TEXT("Over: climbing across the furniture in its way");
            else if (Distance > 400.f && Traversal->StartChaseSurfaceRoute(Goal)) Why = TEXT("Up: over the ceiling, the floor is too cluttered");
            if (Why)
            {
                StopMovement(); ProgressAt = Now;
                LastDecision = Why; ++VoluntarySurfaceRoutes;
                return true;
            }
        }
    }
    // Its own timer: the contract's climb-when-blocked fallback keeps NextSurfaceTry to itself.
    const auto* Overseer = GetWorld()->GetSubsystem<UTallowHunterDirector>();
    const float Aggro = Overseer ? Overseer->GetAggression() : 0.f;
    // Walls and ceilings ("it never goes on the walls... it doesn't climb the roof"). On walls and ceilings it is
    // as fast as on the floor now, so a chase may spiral up the wall and over the ceiling at her; a hunt for a sound
    // may go over the top; wandering, it skitters along a wall or takes the ceiling now and then.
    if (bUseSurfaceRoutes && Now >= NextVoluntaryTry && Distance > 400.f)
    {
        const bool bHot = !Overseer || !bUseDirector || Aggro > .5f || Overseer->GetPhase() == ETallowTensionPhase::Peak
            || Overseer->GetPhase() == ETallowTensionPhase::Build;
        const bool bChase = HuntMode == ETallowHuntMode::Pursue && bVisible && Now >= SpotFreezeUntil && bHot && Distance < 1300.f;
        const bool bHunt = !bVisible && Now - LastSeenAt > 2.f && (bSecondPhase || Aggro >= .18f)
            && (HuntMode == ETallowHuntMode::Pursue || HuntMode == ETallowHuntMode::Search || HuntMode == ETallowHuntMode::Investigate);
        const bool bWander = !bVisible && (HuntMode == ETallowHuntMode::Roam || HuntMode == ETallowHuntMode::Withdraw)
            && Now >= NextWanderClimb;
        if (bChase || bHunt || bWander)
        {
            NextVoluntaryTry = Now + (bChase ? Random.FRandRange(7.f, 12.f) : Random.FRandRange(6.f, 11.f));
            if (bWander) NextWanderClimb = Now + Random.FRandRange(14.f, 28.f);
            const float Roll = Random.FRand();
            const bool bWallFirst = Random.FRand() < .5f;
            const TCHAR* Why = nullptr;
            if (bChase && Roll < .4f && Traversal->StartChaseSurfaceRoute(Goal)) Why = TEXT("Chase: up the wall and over the ceiling at her");
            else if (bWander && Roll < .7f)
            {
                if (bWallFirst && Traversal->StartWallWalk(Goal)) Why = TEXT("Wander: skittering along the wall");
                else if (Traversal->StartChaseSurfaceRoute(Goal)) Why = TEXT("Wander: over the ceiling");
                else if (!bWallFirst && Traversal->StartWallWalk(Goal)) Why = TEXT("Wander: skittering along the wall");
            }
            else if (bHunt && Roll < .45f)
            {
                if (Traversal->StartChaseSurfaceRoute(Goal)) Why = TEXT("Hunt: over the top");
                else if (Roll < .25f && Traversal->StartWallWalk(Goal)) Why = TEXT("Hunt: along the wall");
            }
            if (Why)
            {
                StopMovement(); ProgressAt = Now;
                LastDecision = Why; ++VoluntarySurfaceRoutes;
                return true;
            }
        }
    }
    return false;
}

void ATallowHunterAIController::CacheDoors()
{
    if (bDoorsCached) return;
    bDoorsCached = true;
    for (TActorIterator<ATallowDoor> It(GetWorld()); It; ++It)
        for (USceneComponent* Part : TInlineComponentArray<USceneComponent*>(*It))
            if (Part->GetName() == TEXT("HingePivot"))
            {
                // The actor sits at the hinge corner, 80 cm off the doorway; measure from the frame's centre instead.
                const FVector Center = It->GetRootComponent() ? It->GetRootComponent()->Bounds.Origin : It->GetActorLocation();
                Doors.Add({*It, Part, Part->GetRelativeRotation().Yaw, Center});
            }
    // The power-locked route gates: their leaf blocks it and its sight, but the navmesh runs through them.
    for (TActorIterator<ATallowRouteGate> It(GetWorld()); It; ++It)
        if (It->Frame) Gates.Add({*It, It->Frame->Bounds.Origin});
}

float ATallowHunterAIController::GateOpenness(const FGateWatch& Watch) const
{
    if (!Watch.Gate.IsValid() || !Watch.Gate->Hinge) return 1.f;
    return FMath::Clamp(FMath::Abs(Watch.Gate->Hinge->GetRelativeRotation().Yaw) / FMath::Max(FMath::Abs(Watch.Gate->OpenAngle), 1.f), 0.f, 1.f);
}

float ATallowHunterAIController::DoorOpenness(const FDoorWatch& Watch) const
{
    if (!Watch.Door.IsValid() || !Watch.Hinge.IsValid()) return 1.f;
    const float Yaw = FMath::Abs(FRotator::NormalizeAxis(Watch.Hinge->GetRelativeRotation().Yaw - Watch.ClosedYaw));
    return FMath::Clamp(Yaw / FMath::Max(FMath::Abs(Watch.Door->OpenAngle), 1.f), 0.f, 1.f);
}

bool ATallowHunterAIController::IsBarred(const ATallowDoor* Door)
{
    // Boarded, or locked until she picks it (read live: once picked it is an ordinary door again).
    return Door && (!Door->CanInteract_Implementation(nullptr) || Door->bIsLocked);
}

bool ATallowHunterAIController::PathCrossesShut(const TArray<FVector>& Points)
{
    CacheDoors();
    const float HalfHeight = Creature ? Creature->GetSimpleCollisionHalfHeight() : 50.f;
    const float Radius = Creature ? Creature->GetSimpleCollisionRadius() : 34.f;
    auto Crosses = [&Points, HalfHeight, Radius](const FBox& Bounds)
    {
        // Navigation points sit on the floor. Test the real upright body path
        // against the closed leaf, including its actual vertical bounds. A
        // nearby door on the storey below must not block a stair segment.
        const FVector Body(0.f, 0.f, HalfHeight);
        const FVector Extent(Radius, Radius, HalfHeight);
        for (int32 I = 0; I + 1 < Points.Num(); ++I)
            if (SegmentCrossesBarrier(Points[I] + Body, Points[I + 1] + Body, Bounds, Extent)) return true;
        return false;
    };
    for (const FDoorWatch& Watch : Doors)
        if (Watch.Door.IsValid() && IsBarred(Watch.Door.Get()) && Crosses(DoorLeafBounds(Watch.Door.Get()))) return true;
    for (const FGateWatch& Watch : Gates)
        if (Watch.Gate.IsValid() && Watch.Gate->Door && GateOpenness(Watch) < .3f
            && Crosses(Watch.Gate->Door->Bounds.GetBox())) return true;
    return false;
}

bool ATallowHunterAIController::BoardsBetween(const FVector& From, const FVector& To)
{
    CacheDoors();
    for (const FDoorWatch& Watch : Doors)
        if (Watch.Door.IsValid() && (IsBarred(Watch.Door.Get()) || DoorOpenness(Watch) < .3f)
            && SegmentCrossesBarrier(From, To, DoorLeafBounds(Watch.Door.Get()), FVector(3.f)))
            return true;
    for (const FGateWatch& Watch : Gates)
        if (Watch.Gate.IsValid() && Watch.Gate->Door && GateOpenness(Watch) < .3f
            && SegmentCrossesBarrier(From, To, Watch.Gate->Door->Bounds.GetBox(), FVector(3.f)))
            return true;
    return false;
}

int32 ATallowHunterAIController::DoorInTheWay(float Now, FVector& DoorAt)
{
    CacheDoors();
    const FVector Here = Creature->GetActorLocation();
    const FVector Velocity = Creature->GetVelocity();
    if (Velocity.Size2D() > 20.f) LastHeading = Velocity.GetSafeNormal2D();
    // Better: where its path goes next, so a door ahead is seen before the body reaches the leaf.
    if (const UPathFollowingComponent* Follow = GetPathFollowingComponent())
        if (const FNavPathSharedPtr Path = Follow->GetPath(); Path.IsValid() && Path->IsValid())
        {
            const TArray<FNavPathPoint>& Points = Path->GetPathPoints();
            const int32 Next = FMath::Clamp(int32(Follow->GetCurrentPathIndex()) + 1, 0, Points.Num() - 1);
            const FVector Ahead = Points[Next].Location - Here;
            if (Ahead.Size2D() > 15.f) LastHeading = Ahead.GetSafeNormal2D();
        }
    for (FDoorWatch& Watch : Doors)
    {
        if (!Watch.Door.IsValid() || !Watch.Hinge.IsValid() || Watch.Door->IsHidden()) { Watch.WaitingSince = -1.f; continue; }
        const FVector To = Watch.Center - Here;
        const bool bBoarded = !Watch.Door->CanInteract_Implementation(nullptr), bLocked = Watch.Door->bIsLocked && !bBoarded;
        const float Approach = bBoarded || bLocked ? 200.f : 170.f;
        const auto* Capsule = Creature->GetCapsuleComponent();
        const FVector Extent(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleRadius(),
            Capsule->GetScaledCapsuleHalfHeight());
        // A boarded or locked door is a wall: stop two metres off, before the reaching hands meet it.
        if (To.Size2D() > Approach || FVector::DotProduct(To.GetSafeNormal2D(), LastHeading) < .35f
            || !SegmentCrossesBarrier(Here, Here + LastHeading * Approach,
                DoorLeafBounds(Watch.Door.Get()), Extent))
        { Watch.WaitingSince = -1.f; continue; }
        if (bLocked)
        {
            // Once, then it goes round. Its own timer: the moment she picks it, it is an ordinary door to wait for.
            if (Now < Watch.LockNoticeUntil) continue;
            Watch.LockNoticeUntil = Now + 6.f;
            DoorAt = Watch.Center;
            return 3;
        }
        if (bBoarded) { DoorAt = Watch.Center; return 2; }
        if (Now < Watch.IgnoreUntil) continue;
        // How far the leaf has swung from where it started the game (closed), whatever the hinge's own rotation.
        const float Open = DoorOpenness(Watch);
        const float Yaw = Open * FMath::Abs(Watch.Door->OpenAngle);
        if (Open >= .75f) { Watch.WaitingSince = -1.f; continue; }      // open enough to pass
        // Never stand at a door for good: past three seconds it gives up on this one for a while.
        if (Watch.WaitingSince < 0.f) Watch.WaitingSince = Now;
        if (Now - Watch.WaitingSince > 3.f) { Watch.IgnoreUntil = Now + 8.f; Watch.WaitingSince = -1.f; continue; }
        if (Yaw < 1.f)
        {
            // Closed: push it open (the door's own creature handling: noise, slam rate in a chase). One that will
            // not move half a second after asking is stuck, and the old routing applies.
            if (Now - Watch.AskedAt > 3.f)
            {
                Watch.AskedAt = Now;
                if (Creature->Voice) Creature->Voice->Foley(true);
                Watch.Door->OnAIProximityBeginOverlap(nullptr, Creature, nullptr, 0, false, FHitResult());
            }
            else if (Now - Watch.AskedAt > .5f) continue;
        }
        DoorAt = Watch.Center;
        return 1;
    }
    for (const FGateWatch& Watch : Gates)
    {
        const FVector To = Watch.Center - Here;
        const auto* Capsule = Creature->GetCapsuleComponent();
        if (!Watch.Gate.IsValid() || !Watch.Gate->Door || GateOpenness(Watch) >= .75f || To.Size2D() > 200.f
            || FVector::DotProduct(To.GetSafeNormal2D(), LastHeading) < .35f
            || !SegmentCrossesBarrier(Here, Here + LastHeading * 200.f, Watch.Gate->Door->Bounds.GetBox(),
                FVector(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()))) continue;
        DoorAt = Watch.Center;
        return 2;
    }
    return 0;
}

void ATallowHunterAIController::ThinkAtDoorways(float Now)
{
    // At an open doorway it stops and looks through before going on, the way a predator checks a room.
    if (!Creature || bVisible || HuntMode == ETallowHuntMode::Pursue || HuntMode == ETallowHuntMode::Captured
        || Creature->GetVelocity().Size2D() < 40.f) return;
    CacheDoors();
    for (FDoorWatch& Watch : Doors)
    {
        if (!Watch.Door.IsValid() || DoorOpenness(Watch) < .75f) continue;
        const FVector To = Watch.Center - Creature->GetActorLocation();
        if (To.Size2D() > 170.f || To.Size2D() < 60.f || FVector::DotProduct(To.GetSafeNormal2D(), LastHeading) < .6f) continue;
        const float* Last = DoorPauses.Find(Watch.Door);
        if (Last && Now - *Last < 25.f) continue;
        DoorPauses.Add(Watch.Door, Now);
        if (Random.FRand() > .65f) continue;
        Creature->HunterMotion->PauseAndLook(Watch.Center + LastHeading * 350.f + FVector(0, 0, 60), Random.FRandRange(.8f, 1.4f));
        LastDecision = TEXT("PauseAtDoor: looking through");
        return;
    }
}

void ATallowHunterAIController::SqueezeAtDoorways(float Now)
{
    // Mim does not fit a doorway ("Mim is huge"): every time, it grabs the frame and hauls itself through, which
    // is where she gains ground on it.
    if (!Creature || bDisguised || Creature->GetVelocity().Size2D() < 60.f
        || (Creature->HunterTraversal && Creature->HunterTraversal->IsTraversing())) return;
    CacheDoors();
    const FVector Here = Creature->GetActorLocation(), Heading = Creature->GetVelocity().GetSafeNormal2D();
    auto Through = [&](const FVector& Center)
    {
        const FVector To = Center - Here;
        return To.Size2D() < 110.f && FMath::Abs(To.Z) < 160.f && FVector::DotProduct(To.GetSafeNormal2D(), Heading) > .4f;
    };
    for (const FDoorWatch& Watch : Doors)
    {
        if (!Watch.Door.IsValid() || Watch.Door->bIsLocked || DoorOpenness(Watch) < .4f || !Through(Watch.Center)) continue;
        float& Last = DoorSqueezes.FindOrAdd(Watch.Door.Get());
        if (Now - Last < 4.f) return;
        Last = Now;
        Creature->HunterMotion->BeginSqueezeBeat(HuntMode == ETallowHuntMode::Pursue ? .7f : 1.f);
        if (Creature->Voice) Creature->Voice->Foley(false, .3f);
        LastDecision = TEXT("Door: hauling itself through the frame");
        return;
    }
    for (const FGateWatch& Watch : Gates)
    {
        if (!Watch.Gate.IsValid() || GateOpenness(Watch) < .75f || !Through(Watch.Center)) continue;
        float& Last = DoorSqueezes.FindOrAdd(Watch.Gate.Get());
        if (Now - Last < 4.f) return;
        Last = Now;
        Creature->HunterMotion->BeginSqueezeBeat(HuntMode == ETallowHuntMode::Pursue ? .7f : 1.f);
        LastDecision = TEXT("Door: hauling itself through the frame");
        return;
    }
}

bool ATallowHunterAIController::TryBoo(float Now)
{
    // The jump-out ("sometimes I'm going to jump out and boo them"). Rare and earned: the director allows it
    // minutes apart while the evening builds; it needs a fresh idea of where she is, from its own senses.
    auto* Overseer = GetWorld()->GetSubsystem<UTallowHunterDirector>();
    if (!Creature || !bUseDirector || !Overseer || !Overseer->CanScare() || bBooPending || bVisible || !bHasEvidence) return false;
    if (Now - FMath::Max(LastSeenAt, LastHeardAt) > 12.f) return false;
    const float Away = FVector::Dist2D(Creature->GetActorLocation(), EvidenceLocation);
    if (Away < 500.f || Away > 1300.f) return false;
    FVector Best = FVector::ZeroVector; float BestDistance = MAX_flt;
    for (int32 Try = 0; Try < 16; ++Try)
    {
        const float Angle = Random.FRandRange(0.f, 2.f * PI);
        const FVector Candidate = EvidenceLocation + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Random.FRandRange(220.f, 380.f);
        FVector Projected; float PathLength;
        if (!IsReachable(Candidate, Projected, &PathLength) || !HasAmbushCover(Projected)) continue;
        if (PathLength < BestDistance) { BestDistance = PathLength; Best = Projected; }
    }
    if (BestDistance == MAX_flt || !SetGoal(Best)) return false;
    Overseer->NoteScare();
    bBooPending = true; BooRevealAt = -1.f; BooTarget = EvidenceLocation;
    AmbushFocus = EvidenceLocation + FVector(0, 0, 60);
    bTravellingToAmbush = true;
    AmbushUntil = Now + 30.f;
    SearchUntil = FMath::Max(SearchUntil, AmbushUntil + 5.f);
    EnterMode(ETallowHuntMode::Ambush);
    SetPace(.55f, 30.f);
    LastDecision = TEXT("Boo: stalking unseen");
    return true;
}

void ATallowHunterAIController::RevealBoo(float Now)
{
    bBooPending = false; BooRevealAt = -1.f;
    // Out it comes, fast, straight at where it thinks she is. The stinger only if it is in front of her eyes.
    if (const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0); PC && PC->PlayerCameraManager && Creature)
    {
        const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
        const FVector To = Creature->GetActorLocation() + FVector(0, 0, 80) - Eye;
        if (To.Size() < 1000.f && FVector::DotProduct(To.GetSafeNormal(), PC->PlayerCameraManager->GetActorForwardVector()) > .45f)
            if (USoundBase* Sting = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Creature/K_Mim_Capture_Sting.K_Mim_Capture_Sting")))
                UGameplayStatics::PlaySound2D(this, Sting, .55f);
    }
    bTravellingToAmbush = false; AmbushUntil = 0.f;
    SetPace(1.6f, 1.2f);
    EnterMode(ETallowHuntMode::Investigate);
    SetGoalNearest(BooTarget);
    Creature->HunterMotion->LockGaze(BooTarget + FVector(0, 0, 70), 1.2f);
    LastDecision = TEXT("Boo: stepping out");
}

bool ATallowHunterAIController::HoldForDoor(float Now)
{
    // Runs every frame, not only when a move is re-requested: an existing path keeps the body walking between
    // requests, and it used to carry the Keeper straight through a shut leaf (the Keeper Lab, 25 Oct).
    if (!Creature || !Player || HuntMode == ETallowHuntMode::Captured || AttackStartedAt >= 0.f || bDisguised
        || (Creature->HunterTraversal && Creature->HunterTraversal->IsTraversing())) return false;
    FVector DoorAt;
    const int32 Door = DoorInTheWay(Now, DoorAt);
    if (Door == 3)
    {
        StopMovement(); bHasGoal = false; bWaitingForDoor = false;
        Creature->HunterMotion->PauseAndLook(DoorAt + FVector(0, 0, 60), Random.FRandRange(1.2f, 2.f));
        LastHeading = (Creature->GetActorLocation() - DoorAt).GetSafeNormal2D();
        NextDecision = Now + 1.5f;
        if (HuntMode == ETallowHuntMode::Pursue || HuntMode == ETallowHuntMode::Investigate) EnterMode(ETallowHuntMode::Search);
        LastDecision = TEXT("BlockedByLock: a locked door, going round");
        return true;
    }
    if (Door == 2)
    {
        // Boarded: never squeeze through (25 Oct). It stands off, stares at the boards a while, then searches
        // elsewhere. Once the last plank is off the door opens for it like any other.
        // It stays a few seconds at the boards, then gives the whole area up for 45 s (prying there no longer
        // calls it) and walks far off, so the player can finish the boards and get out (25 Oct).
        StopMovement(); bHasGoal = false; bWaitingForDoor = false; bTravellingToAmbush = false;
        AmbushFocus = DoorAt + FVector(0, 0, 60);
        AmbushUntil = Now + Random.FRandRange(3.f, 4.5f);
        EnterMode(ETallowHuntMode::Ambush);
        Creature->HunterMotion->LookAt(AmbushFocus, AmbushUntil - Now);
        LastHeading = (Creature->GetActorLocation() - DoorAt).GetSafeNormal2D();
        QuietAreaCenter = DoorAt; QuietUntil = Now + 45.f;
        bLeaveBoardsAfterStare = true; BoardedDoorAt = DoorAt;
        LastDecision = TEXT("BlockedByBoards");
        return true;
    }
    if (Door == 1)
    {
        if (!bWaitingForDoor) { StopMovement(); bWaitingForDoor = true; LastDecision = TEXT("WaitForDoor"); }
        ProgressAt = Now; ProgressAnchor = Creature->GetActorLocation();
        GoalDeadline = FMath::Max(GoalDeadline, Now + 3.f);
        return true;
    }
    if (bWaitingForDoor) { bWaitingForDoor = false; NextMoveRequest = 0.f; }
    return false;
}

void ATallowHunterAIController::Tick(float DeltaSeconds)
{
    TickBrain(DeltaSeconds);
    NoteDecision();
}

void ATallowHunterAIController::NoteDecision()
{
    if (LastDecision == LoggedDecision) return;
    LoggedDecision = LastDecision;
    DecisionLog.Add(FString::Printf(TEXT("%.1f %s"), GetWorld()->GetTimeSeconds(), *LastDecision.ToString()));
    if (DecisionLog.Num() > 60) DecisionLog.RemoveAt(0);
    // One line per change of mind, with the act and phase it happened in, so a whole run can be counted from the log.
    // Bare mode names repeat the decision that caused them and are left out.
    const FString Text = LastDecision.ToString();
    if (Text.StartsWith(TEXT("ETallowHuntMode::")) || LastDecision == LoggedTactic) return;
    LoggedTactic = LastDecision;
    const UTallowHunterDirector* Director = GetHuntDirector();
    const float Away = Creature && Player ? FVector::Dist(Creature->GetActorLocation(), Player->GetActorLocation()) : -1.f;
    const int32 Candle = Player && Player->GetCandleComponent() ? static_cast<int32>(Player->GetCandleComponent()->GetCandleState()) : -1;
    UE_LOG(LogTemp, Display, TEXT("TALLOW_TACTIC t=%.1f act=%d phase=%d mode=%d dist=%.0f candle=%d hidden=%d held=%d seen=%d %s"),
        GetWorld()->GetTimeSeconds(), Director ? static_cast<int32>(Director->GetAct()) : -1,
        Director ? static_cast<int32>(Director->GetPhase()) : -1, static_cast<int32>(HuntMode), Away, Candle,
        Player && Player->IsHiddenInSpot() ? 1 : 0, Player && Player->IsHoldingStill() ? 1 : 0, bVisible ? 1 : 0, *Text);
}

void ATallowHunterAIController::TickBrain(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Creature && bContractReview)
    {
        const float Now = GetWorld()->GetTimeSeconds();
        if (bHasGoal && FVector::Dist2D(Creature->GetActorLocation(), Goal) <= 12.f
            && FMath::Abs(Creature->GetActorLocation().Z - Creature->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - Goal.Z) < 10.f)
        {
            ContractReviewReached = true; bHasGoal = false; StopMovement();
            MovementContractStatus = TEXT("review destination reached");
        }
        else if (bHasGoal) UpdateContractMovement(Now);
        return;
    }
    if (!Creature || !HunterBrain->IsRunning() || HunterBrain->IsPaused()) return;
    if (!Player)
    {
        Player = Cast<ATallowCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
        if (Player) Player->GetCandleComponent()->OnCandleReset.AddUniqueDynamic(this, &ThisClass::ResetHunt);
    }
    if (!Player || Player->IsTiedToChair() || HuntMode == ETallowHuntMode::Captured) return;
    const float Now = GetWorld()->GetTimeSeconds();
    Observations.RemoveAll([Now](const FObservation& O) { return Now - O.Time > 35.f; });
    bHadVisible = bVisible;
    const bool bHidingNow = Player && Player->IsHiddenInSpot();
    if (bHidingNow && !bPlayerWasHiding)
    {
        auto* Spot = Player->GetCurrentHideVolume();
        const FVector EntryEye = Spot->GetEntryLocation() + FVector(0, 0, Player->BaseEyeHeight);
        const bool bSawEntry = bDirectPlayerSight && Now - LastSeenAt < .35f
            && FVector::Dist(EvidenceLocation, Spot->GetEntryLocation()) < 180.f
            && HasClearTrace(HeadLocation(), EntryEye, Spot);
        if (bSawEntry) { KnownOccupiedHide = Spot; KnownHideEvidenceAt = Now; }
        else if (KnownOccupiedHide.Get() != Spot || Now - KnownHideEvidenceAt > .25f) KnownOccupiedHide.Reset();
        if (auto* Overseer = GetWorld()->GetSubsystem<UTallowHunterDirector>()) Overseer->NoteHiding();
    }
    else if (!bHidingNow) KnownOccupiedHide.Reset();
    bPlayerWasHiding = bHidingNow;
    bVisible = SensePlayer(DeltaSeconds);
    // Gaze is established before tactics may return early. Only fresh sight
    // updates the target; occlusion grace retains the last seen position.
    if (bDirectPlayerSight) Creature->HunterMotion->LockGaze(VisiblePlayerGazeTarget, .8f);
    if (UpdateTacticalRetreat(Now, DeltaSeconds)) { NoteDecision(); return; }
    HoldForDoor(Now);
    ThinkAtDoorways(Now);
    SqueezeAtDoorways(Now);
    if (auto* Stealth = Cast<ATallowStealthCharacter>(Player))
    {
        Stealth->bSeenByHunter = bVisible;
        Stealth->HunterAwareness = SightConfidence;
    }
    // The seen signal lives on the base pawn for every pawn class: the played pawn is not the
    // stealth subclass, so writing it only inside that cast left the HUD reading chase state.
    Player->KeeperSight = SightConfidence;
    Player->bSeenByKeeper = bVisible;
    if (Creature) Player->KeeperSeenFrom = HeadLocation();
    ContinuousPursuit = bVisible ? ContinuousPursuit + DeltaSeconds
        : FMath::Max(0.f, ContinuousPursuit - DeltaSeconds * .6f);
    if (!bVisible) EvidenceStrength = FMath::Max(0.f, EvidenceStrength - DeltaSeconds * .018f);
    // A watcher that has her in its eyes but has not committed is still dread, not the peak.
    if (auto* Director = GetWorld()->GetSubsystem<UTallowHunterDirector>())
        if (bUseDirector) Director->UpdatePressure(DeltaSeconds, Creature, Player, bVisible && !bWatching && Now >= WatchSlipUntil);
    if (Creature->Voice) Creature->Voice->SetLurking(IsLurking());
    if (Creature->HunterTraversal && Creature->HunterTraversal->IsTraversing() && !Creature->HunterTraversal->IsHolding()
        && Creature->GetActorUpVector().Z < .5f && Now >= NextScratch && Creature->Voice)
    {
        NextScratch = Now + Random.FRandRange(2.5f, 4.5f);
        Creature->Voice->Foley(false, .3f);
    }
    UpdateWatch(Now, DeltaSeconds);
    if (UpdateCornerTactics(Now)) return;
    if (UpdateSpiderAmbush(Now, DeltaSeconds)) return;
    if (bVisible)
    {
        if (bDisguisePending || bHoldPending || PendingLink != INDEX_NONE) CancelSpiderPlans();
        // The moment it sees you: its call, and the candle and her breath answer (the diegetic "you are seen").
        if (!bHadVisible)
        {
            ++SightingCount; AlertPlayer(); FireEvent(ETallowKeeperEvent::Seen);
            // A fresh sighting (not a flicker): it freezes on her for a beat before it comes, so a player who reacts
            // within about two seconds can still break the line and get away (25 Oct: chase pace).
            const auto* Overseer = GetWorld()->GetSubsystem<UTallowHunterDirector>();
            const float Away = FVector::Dist(Creature->GetActorLocation(), Player->GetActorLocation());
            if (bWatching && !bWatchStaring && Away > 600.f && Overseer
                && Overseer->GetPhase() != ETallowTensionPhase::Peak)
            {
                // From a watch post the candle has given her away, and it stares a long breath first: put the light
                // out or get out of its sight and it slips away; hold the flame on it and it comes.
                bWatchStaring = true; WatchLookedAt = 0.f;
                WatchStareUntil = SpotFreezeUntil = Now + Random.FRandRange(1.8f, 2.4f);
                LastDecision = TEXT("Watch: her candle gave her away, staring");
            }
            else if (Now - LastSeenAt > 3.f)
                SpotFreezeUntil = Now + FMath::Lerp(.9f, .6f, Overseer ? Overseer->GetAggression() : .45f);
            PaceUntil = 0.f;
        }
        LastSeenAt = Now;
        Observe(Player->GetActorLocation(), 1.f, true);
        LastEvidenceType = TEXT("Sight");
        InspectionSpot = nullptr;
        SearchAnchor = nullptr;
        bTravellingToAmbush = false;
        // Just up from the heap slowly: it keeps creeping at her a few seconds before it charges.
        if (bWatchStaring && Now < WatchStareUntil)
        {
            // She turns and holds the light on it ("slip out of sight when the candle swings at her"): it goes.
            // If she never finds it, it comes when the stare ends; the call, the cold flame and her gasp said so.
            WatchLookedAt = PlayerLooksAt(HeadLocation(), .94f) ? WatchLookedAt + DeltaSeconds : FMath::Max(0.f, WatchLookedAt - DeltaSeconds);
            if (WatchLookedAt >= .4f) EndWatch(Now, true, TEXT("Watch: she turned the light on it, slipping away"));
            else { Creature->HunterMotion->LockGaze(VisiblePlayerGazeTarget, .8f); StopMovement(); }
            NoteDecision();
            return;
        }
        if (bWatching) EndWatch(Now, false, TEXT("Watch: she kept the flame on it, coming"));
        // Slipping away out of her sight: unless she is right on it, it keeps going.
        if (Now < WatchSlipUntil && FVector::Dist(Creature->GetActorLocation(), Player->GetActorLocation()) > 450.f) { NoteDecision(); return; }
        if (Now >= StalkSightUntil) EnterMode(ETallowHuntMode::Pursue);
        else if (HuntMode != ETallowHuntMode::Investigate) EnterMode(ETallowHuntMode::Investigate);
        if (bRecoveryDetour || bRecoveryTraversal || bRecoveryWaiting)
        {
            // Fresh sight may move the destination, but it does not cancel a
            // hand transfer or send the floor detour back into its obstacle.
            RecoveryFinalGoal = EvidenceLocation;
            LastPursuitGoal = EvidenceLocation;
        }
        else if ((!bHasGoal && Now >= NextPursuitGoalTry)
            || FVector::Dist2D(LastPursuitGoal, EvidenceLocation) > 90.f)
        {
            LastPursuitGoal = EvidenceLocation;
            NextPursuitGoalTry = Now + .6f;
            SetGoalNearest(EvidenceLocation);
        }
        Creature->HunterMotion->LockGaze(VisiblePlayerGazeTarget, .8f);
        if (Now < SpotFreezeUntil) { StopMovement(); NextMoveRequest = SpotFreezeUntil; }
    }
    else if (bHadVisible && bWatching)
    {
        // Out of its sight (or the flame went out) while it stared: it slips away rather than coming.
        EndWatch(Now, true, TEXT("Watch: she hid from it, slipping away"));
    }
    else if (bHadVisible)
    {
        if (ContinuousPursuit > 2.f)
        {
            if (auto* Overseer = GetWorld()->GetSubsystem<UTallowHunterDirector>()) Overseer->NotePursuitLost();
            FireEvent(ETallowKeeperEvent::Lost);
        }
        SearchUntil = Now + (bSecondPhase ? 40.f : 30.f);
        EnterMode(ETallowHuntMode::Investigate);
        // Keep running the way the player was going instead of stopping at the last sighting.
        // Players who have already slipped it get cut off instead of followed.
        const auto* Learned = GetWorld()->GetSubsystem<UTallowHunterDirector>();
        const float AmbushChance = FMath::Clamp(.4f + .2f * (Learned ? Learned->Escapes : 0), 0.f, .9f);
        if (bUseAmbushes && Random.FRand() < AmbushChance && RequestAmbushFromEvidence()) return;
        FVector Ahead = EvidenceLocation + EvidenceHeading * 380.f;
        FVector Projected;
        SetGoal(!EvidenceHeading.IsNearlyZero() && IsReachable(Ahead, Projected) ? Projected : EvidenceLocation);
    }
    if (Now < RushUntil && !bVisible && Player && HuntMode != ETallowHuntMode::Captured && Now >= NextRushRefresh)
    {
        // Noisy movement keeps the exact position known for as long as it continues.
        NextRushRefresh = Now + .3f;
        // New sound events update EvidenceLocation in HearNoise. Do not track
        // the live pawn after the footsteps have stopped or cover was reached.
        SearchUntil = Now + 20.f;
        EnterMode(ETallowHuntMode::Pursue);
        SetGoalNearest(EvidenceLocation);
    }
    if (HuntMode == ETallowHuntMode::Ambush && !bTravellingToAmbush && Now >= NextPeekAt && Creature && !bVisible)
    {
        // Slide out past the cover, hold the stare, slide back: a hunter testing the room.
        NextPeekAt = Now + Random.FRandRange(3.2f, 6.5f);
        Creature->HunterMotion->BeginPeek(AmbushFocus, Random.FRandRange(1.1f, 2.1f));
    }
    // Catching up from far off only at the peak of an evening; otherwise it arrives at its own pace.
    const auto* Pacing = GetWorld()->GetSubsystem<UTallowHunterDirector>();
    bCatchUp = Player && Creature && Now < RushUntil && FVector::Dist(Creature->GetActorLocation(), Player->GetActorLocation()) > 1100.f
        && (!Pacing || !bUseDirector || Pacing->GetPhase() == ETallowTensionPhase::Peak);
    if (Now >= NextDebugLog)
    {
        NextDebugLog = Now + 2.f;
        const auto* Motion = Creature->HunterMotion.Get();
        UE_LOG(LogTemp, Warning, TEXT("HUNTERDBG mode=%d pos=%s yaw=%.1f goal=%d target=%s evidence=%s speed=%.0f rush=%.1f catchup=%d recovery=%s waiting=%d detour=%d fit=%d fitblocked=%.2f fitreason=%s decision=%s"),
            int32(HuntMode), *Creature->GetActorLocation().ToCompactString(), Creature->GetActorRotation().Yaw,
            bHasGoal ? 1 : 0, *Goal.ToCompactString(), *EvidenceLocation.ToCompactString(),
            Creature->GetVelocity().Size(), FMath::Max(0.f, RushUntil - Now), bCatchUp ? 1 : 0,
            *RecoveryMode.ToString(), bRecoveryWaiting ? 1 : 0, bRecoveryDetour ? 1 : 0,
            Motion && Motion->IsFitPassable() ? 1 : 0, Motion ? Motion->GetFitBlockedSeconds() : 0.f,
            Motion ? *Motion->GetFitBlockReason() : TEXT("none"), *LastDecision.ToString());
    }
    // Echolocation in the dark (25 Oct): with the candle out it can't see, so it clicks and listens
    // for the echo. Anyone within range who isn't dead silent (breath held, barely moving) is found.
    // The clicks are also the player's cue to go quiet.
    if (Player && Creature && !bVisible && HuntMode != ETallowHuntMode::Captured && HuntMode != ETallowHuntMode::Ambush
        && !Player->IsTiedToChair() && !Player->IsHiddenInSpot() && Now >= NextEchoAt)
    {
        NextEchoAt = Now + (bHasEvidence ? Random.FRandRange(2.2f, 3.6f) : Random.FRandRange(4.5f, 7.5f));
        if (Player->GetCandleComponent()->GetCandleState() == ECandleState::Out)
        {
            // Listening for her in the dark. It is soft now: no clicks, just its breath, which is still the
            // player's cue that it is hunting by sound, close.
            const FVector Head = HeadLocation();
            if (Creature->Voice) Creature->Voice->Say(ETallowKeeperLine::Breath);
            const bool bSilent = Player->IsHoldingStill() && Player->GetVelocity().Size2D() < 140.f;
            const float Distance = FVector::Dist(Head, Player->GetActorLocation());
            if (!bSilent && Distance < EchoRange
                && (Distance < EchoRange * .5f || HasClearTrace(Head, Player->GetPawnViewLocation())))
            {
                const float Angle = Random.FRandRange(0.f, 2.f * PI);
                const FVector Echo = Player->GetActorLocation()
                    + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Random.FRandRange(20.f, 60.f);
                Observe(Echo, .8f, false);
                SearchUntil = Now + (bSecondPhase ? 28.f : 21.f);
                EnterMode(ETallowHuntMode::Investigate);
                SetGoalNearest(Echo);
                Creature->HunterMotion->LookAt(Echo + FVector(0, 0, 55), 1.2f);
                LastDecision = TEXT("Echolocated");
                if (Creature->Voice) Creature->Voice->Say(ETallowKeeperLine::Heard);
            }
        }
    }
    // Close in the dark, breathing: it hears exactly where you are and comes straight at it.
    if (IsDark() && !Player->IsHiddenInSpot() && HuntMode != ETallowHuntMode::Captured && Now >= NextBreathGoal)
    {
        // From its head, not its middle: climbing a wall, its middle sits inside the plaster and every trace failed
        // (the Keeper Lab's B, 25 Oct). Within 3 m breathing is heard through a door frame or a chair.
        const FVector Ear = HeadLocation();
        const float Distance = FVector::Dist(Ear, Player->GetActorLocation());
        if (HearsBreathing(Distance) && !BoardsBetween(Ear, Player->GetPawnViewLocation()) && (Distance < 300.f
            || HasClearTrace(Ear, Player->GetPawnViewLocation()) || HasClearTrace(Ear, Player->GetActorLocation())))
        {
            NextBreathGoal = Now + .5f;
            Observe(Player->GetActorLocation(), .95f, false);
            LastHeardAt = Now;
            SearchUntil = Now + (bSecondPhase ? 28.f : 21.f);
            if (HuntMode != ETallowHuntMode::Investigate && HuntMode != ETallowHuntMode::Pursue) EnterMode(ETallowHuntMode::Investigate);
            SetGoalNearest(Player->GetActorLocation());
            Creature->HunterMotion->LookAt(Player->GetPawnViewLocation(), .6f);
            LastDecision = TEXT("HearsBreathing");
        }
    }
    UpdateAttack(Now);
    if (HuntMode == ETallowHuntMode::Captured) return;
    if (HuntMode == ETallowHuntMode::Listen)
    {
        if (Now < ListenUntil) return;
        FinishListening(Now);
    }
    if (HuntMode == ETallowHuntMode::Ambush && bBooPending && !bTravellingToAmbush)
    {
        // Arrived unseen: a held breath of a wait, then out.
        if (BooRevealAt < 0.f) { BooRevealAt = Now + Random.FRandRange(1.2f, 2.6f); Creature->HunterMotion->LookAt(AmbushFocus, 2.6f); }
        else if (Now >= BooRevealAt) RevealBoo(Now);
    }
    if (HuntMode == ETallowHuntMode::Ambush)
    {
        if (Now >= AmbushUntil && bLeaveBoardsAfterStare)
        {
            // Done at the boards: forget the scent and go to the patrol point furthest from that door.
            bLeaveBoardsAfterStare = false;
            bHasEvidence = false; bHasGoal = false; bTravellingToAmbush = false;
            InspectionSpot = nullptr; SearchAnchor = nullptr; SearchUntil = Now; RushUntil = FMath::Min(RushUntil, Now);
            StopMovement();
            FVector Far = FVector::ZeroVector; float FarDistance = 0.f;
            for (TActorIterator<ATallowPatrolPoint> It(GetWorld()); It; ++It)
            {
                const float D = FVector::Dist(It->GetActorLocation(), BoardedDoorAt);
                if (D > FarDistance) { FarDistance = D; Far = It->GetActorLocation(); }
            }
            EnterMode(ETallowHuntMode::Withdraw);
            if (FarDistance > 0.f) SetGoal(Far);
            NextDecision = Now + 8.f;
            LastDecision = TEXT("LeftBoardedDoor");
        }
        else if (Now >= AmbushUntil)
        {
            bTravellingToAmbush = false;
            bHasGoal = false;
            StopMovement();
            EnterMode(ETallowHuntMode::Search);
            NextDecision = Now + .5f;
        }
        else if (!bTravellingToAmbush)
        {
            Creature->HunterMotion->LookAt(AmbushFocus, .25f);
            return;
        }
    }
    if (HuntMode == ETallowHuntMode::Inspect && Now >= InspectUntil) EnterMode(ETallowHuntMode::Search);
    // A rush that ends without sight becomes an ordinary search of the spot, so the clocks below can end it.
    if (HuntMode == ETallowHuntMode::Pursue && !bVisible && Now >= RushUntil && !bHasGoal)
    {
        EnterMode(ETallowHuntMode::Search);
        NextDecision = Now;
    }
    // Searching one area without finding you has a hard limit, however many small sounds it hears.
    if (bHasEvidence && !bVisible && HuntMode != ETallowHuntMode::Captured)
    {
        if (AreaSearchStartedAt < 0.f || FVector::Dist2D(EvidenceLocation, AreaCenter) > 600.f)
        {
            AreaCenter = EvidenceLocation;
            AreaSearchStartedAt = Now;
        }
        if (Now - FMath::Max(AreaSearchStartedAt, LastSeenAt) > (bSecondPhase ? 45.f : 35.f))
        {
            QuietAreaCenter = AreaCenter;
            QuietUntil = Now + 25.f;
            AreaSearchStartedAt = -1.f;
            SearchUntil = Now;
            RushUntil = FMath::Min(RushUntil, Now);
            if (HuntMode == ETallowHuntMode::Pursue) EnterMode(ETallowHuntMode::Search);
            LastDecision = TEXT("GaveUpArea");
        }
    }
    else if (!bHasEvidence) AreaSearchStartedAt = -1.f;
    if (bHasEvidence && !bVisible && Now > SearchUntil && HuntMode != ETallowHuntMode::Pursue)
    {
        bHasEvidence = false; bHasGoal = false; InspectionSpot = nullptr; SearchAnchor = nullptr;
        StopMovement();
        EnterMode(ETallowHuntMode::Roam);
    }
    if (bUseDirector && bHasEvidence && !bVisible && Now - FMath::Max(LastSeenAt, LastHeardAt) > 7.f)
        if (const auto* Director = GetHuntDirector(); Director && Director->bBreather)
        {
            bHasEvidence = false; bHasGoal = false; InspectionSpot = nullptr; SearchAnchor = nullptr;
            StopMovement();
            EnterMode(ETallowHuntMode::Withdraw);
            LastDecision = TEXT("ReliefAfterSearch");
            if (Random.FRand() < .5f) TryLinkEscape(Now);
        }
    if (bUseDirector && bHasGoal && !bHasEvidence && Now >= NextAreaDecision && !bWatchPending && !bWatching)
    {
        NextAreaDecision = Now + 1.f;
        if (auto* Director = GetHuntDirector())
        {
            const bool bChangePacing = Director->IsReturnOverdue()
                || (Director->bBreather && HuntMode != ETallowHuntMode::Withdraw)
                || (!Director->bBreather && HuntMode == ETallowHuntMode::Withdraw);
            FVector Hint; bool bRetreat = false;
            if (bChangePacing && Director->TakeAreaHint(Creature, Player, Hint, bRetreat))
            {
                const auto Mode = bRetreat ? ETallowHuntMode::Withdraw : ETallowHuntMode::Roam;
                if (Mode != HuntMode || FVector::Dist(Hint, Goal) > 120.f)
                {
                    StopMovement();
                    EnterMode(Mode);
                    SetGoal(Hint);
                }
            }
        }
    }
    if (!bHasGoal && Now >= NextDecision && Now >= InspectUntil && HuntMode != ETallowHuntMode::Pursue)
    {
        NextDecision = Now + 1.f;
        if (bHasEvidence)
        {
            const bool bBoo = TryDisguise(Now) || TryBoo(Now);
            const bool bWaitAtExit = !bBoo && SearchSteps >= 2 && Now - SearchStartedAt > 4.f
                && Random.FRand() < .45f && (AmbushExit(EvidenceLocation) || RequestAmbushFromEvidence());
            // Or stand off and watch the place it last knew her, while the evening is still building.
            const auto* Stalker = GetHuntDirector();
            const bool bWatch = !bBoo && !bWaitAtExit && Stalker && Stalker->WantsWatch()
                && Now - FMath::Max(LastSeenAt, LastHeardAt) > 5.f && Random.FRand() < .4f && TryWatch(Now, EvidenceLocation, true);
            if (!bBoo && !bWaitAtExit && !bWatch) ChooseSearchPoint();
        }
        else
        {
            if (TryDisguise(Now)) { UpdateMove(Now); return; }
            FVector Hint; bool bRetreat = false; bool bHint = false;
            auto* Director = GetWorld()->GetSubsystem<UTallowHunterDirector>();
            // Unaware of her and building: the stalk first (stand off in the dark where she can see it and watch),
            // else sometimes a route that crosses her view at a distance.
            if (bUseDirector && Director && Director->WantsWatch() && TryWatch(Now, Player->GetActorLocation(), false))
            {
                UpdateMove(Now);
                return;
            }
            if (bUseDirector && Director && Director->WantsPassBy() && Director->TakePassByHint(Creature, Player, Hint) && SetGoal(Hint))
            {
                EnterMode(ETallowHuntMode::Roam);
                SetPace(.85f, 25.f);
                LastDecision = TEXT("PassBy");
                UpdateMove(Now);
                return;
            }
            if (Director && bUseDirector) bHint = Director->TakeAreaHint(Creature, Player, Hint, bRetreat);
            if (bHint)
            {
                EnterMode(bRetreat ? ETallowHuntMode::Withdraw : ETallowHuntMode::Roam);
                if (!(bRetreat && Random.FRand() < .35f && TryLinkEscape(Now))) SetGoal(Hint);
            }
            else if (HuntMode != ETallowHuntMode::Withdraw)
            {
                TArray<ATallowPatrolPoint*> Points;
                for (TActorIterator<ATallowPatrolPoint> It(GetWorld()); It; ++It) Points.Add(*It);
                for (int32 I = 0; I < Points.Num(); ++I)
                    if (SetGoal(Points[Random.RandRange(0, Points.Num() - 1)]->GetActorLocation())) break;
            }
        }
    }
    UpdateMove(Now);
}
