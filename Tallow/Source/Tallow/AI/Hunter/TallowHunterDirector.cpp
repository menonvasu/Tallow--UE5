#include "AI/Hunter/TallowHunterDirector.h"
#include "AI/TallowCreature.h"
#include "Player/TallowCharacter.h"
#include "PatrolPoint/TallowPatrolPoint.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

FTallowArcTuning UTallowHunterDirector::TuningForAct(ETallowStoryAct InAct)
{
    // One row per act of the route. Reading down: the quiet shrinks, the peak lengthens, the
    // aggression ceiling rises, and new kinds of scare unlock. Relief never drops below 20 s.
    FTallowArcTuning T;
    switch (InAct)
    {
    case ETallowStoryAct::Dormant:
        T.QuietMin = 60.f; T.QuietMax = 90.f; T.BuildSeconds = 70.f; T.PeakSeconds = 40.f; T.RelaxMin = 35.f; T.RelaxMax = 50.f;
        T.AggressionFloor = .10f; T.AggressionCap = .25f; T.AggressionRate = .0005f; T.BuildDwellBeforePeak = 35.f;
        T.bAllowScare = T.bAllowDisguise = T.bAllowHouseEvents = T.bAllowLies = T.bAllowPassBy = T.bAllowWatch = false;
        T.LieGapScale = 1.5f;
        break;
    // Final pass (28 Oct: "a playthrough is only 20-30 mins... I want to see Mim stalk and ambush and hide more"):
    // the quiet and the relief are shorter, the build (where the stalk lives) is longer, and the tricks come sooner.
    case ETallowStoryAct::FirstContact:
        T.QuietMin = 30.f; T.QuietMax = 45.f; T.BuildSeconds = 60.f; T.PeakSeconds = 40.f; T.RelaxMin = 25.f; T.RelaxMax = 35.f;
        T.AggressionFloor = .15f; T.AggressionCap = .40f; T.AggressionRate = .0008f; T.BuildDwellBeforePeak = 30.f;
        T.ScareCooldownMin = 180.f; T.ScareCooldownMax = 240.f; T.PassByMin = 30.f; T.PassByMax = 45.f;
        T.WatchMin = 18.f; T.WatchMax = 30.f;
        T.bAllowScare = false; T.bAllowDisguise = false;   // first contact is a glimpse and a watcher, then a hunt: no tricks yet
        T.LieGapScale = 1.3f;
        break;
    case ETallowStoryAct::FuseHunt:
        T.QuietMin = 25.f; T.QuietMax = 40.f; T.BuildSeconds = 90.f; T.PeakSeconds = 50.f; T.RelaxMin = 22.f; T.RelaxMax = 30.f;
        T.AggressionFloor = .30f; T.AggressionCap = .60f; T.AggressionRate = .0010f; T.BuildDwellBeforePeak = 25.f;
        T.ScareCooldownMin = 100.f; T.ScareCooldownMax = 140.f; T.PassByMin = 30.f; T.PassByMax = 45.f;
        // The heap now joins the fuse hunt (it is most of the evening), held back for its first two minutes.
        T.DisguiseCooldownMin = 150.f; T.DisguiseCooldownMax = 210.f; T.DisguiseDebutSeconds = 150.f;
        T.WatchMin = 18.f; T.WatchMax = 30.f;
        break;
    case ETallowStoryAct::HouseWakes:
        T.QuietMin = 20.f; T.QuietMax = 32.f; T.BuildSeconds = 85.f; T.PeakSeconds = 55.f; T.RelaxMin = 20.f; T.RelaxMax = 28.f;
        T.AggressionFloor = .45f; T.AggressionCap = .75f; T.AggressionRate = .0012f; T.BuildDwellBeforePeak = 18.f;
        T.ScareCooldownMin = 90.f; T.ScareCooldownMax = 130.f;
        T.DisguiseCooldownMin = 110.f; T.DisguiseCooldownMax = 160.f; T.PassByMin = 30.f; T.PassByMax = 45.f;
        T.WatchMin = 18.f; T.WatchMax = 30.f;
        T.LieGapScale = .9f;
        break;
    case ETallowStoryAct::Reel:
        T.QuietMin = 18.f; T.QuietMax = 28.f; T.BuildSeconds = 70.f; T.PeakSeconds = 60.f; T.RelaxMin = 20.f; T.RelaxMax = 26.f;
        T.AggressionFloor = .55f; T.AggressionCap = .88f; T.AggressionRate = .0014f; T.BuildDwellBeforePeak = 14.f;
        T.ScareCooldownMin = 80.f; T.ScareCooldownMax = 120.f;
        T.DisguiseCooldownMin = 100.f; T.DisguiseCooldownMax = 150.f; T.PassByMin = 28.f; T.PassByMax = 40.f;
        T.WatchMin = 18.f; T.WatchMax = 30.f;
        T.LieGapScale = .8f;
        break;
    case ETallowStoryAct::EndRun:
        T.QuietMin = 15.f; T.QuietMax = 24.f; T.BuildSeconds = 45.f; T.PeakSeconds = 85.f; T.RelaxMin = 20.f; T.RelaxMax = 28.f;
        T.AggressionFloor = .65f; T.AggressionCap = 1.f; T.AggressionRate = .0018f; T.BuildDwellBeforePeak = 8.f;
        T.ScareCooldownMin = 70.f; T.ScareCooldownMax = 100.f; T.PassByMin = 35.f; T.PassByMax = 55.f;
        T.WatchMin = 16.f; T.WatchMax = 24.f; T.WatchesPerBuild = 2;   // a watcher is true: it is really there
        T.bAllowDisguise = false; T.bAllowHouseEvents = false; T.bAllowLies = false;  // every cue on the run out is the truth
        break;
    }
    return T;
}

float UTallowHunterDirector::ClampAggression(float Value) const
{
    return bReviewPinned ? FMath::Clamp(Value, 0.f, 1.f) : FMath::Clamp(Value, 0.f, Tuning().AggressionCap);
}

void UTallowHunterDirector::SetActForReview(ETallowStoryAct NewAct)
{
    bActPinned = false;
    SetStoryAct(NewAct);
    bActPinned = NewAct != ETallowStoryAct::Dormant;
}

void UTallowHunterDirector::SetStoryAct(ETallowStoryAct NewAct)
{
    if (NewAct == Act || !GetWorld() || bActPinned) return;
    const bool bRising = static_cast<uint8>(NewAct) > static_cast<uint8>(Act);
    Act = NewAct;
    const float Now = GetWorld()->GetTimeSeconds();
    const FTallowArcTuning T = Tuning();
    UE_LOG(LogTemp, Display, TEXT("TALLOW_PACING act=%d rising=%d aggression=%.2f cap=%.2f"), static_cast<int32>(Act), bRising ? 1 : 0, Aggression, T.AggressionCap);
    Aggression = ClampAggression(bRising ? FMath::Max(Aggression, T.AggressionFloor) : Aggression);
    if (!bRising) return;   // a capture reset the house: no beat, the evening just starts lower again
    switch (NewAct)
    {
    case ETallowStoryAct::FirstContact:
        // Authored first contact: time to finish prying, then a glimpse crossing her view, and
        // no jump-out until she has learned what a pass-by means.
        // The pry is answered by a watcher at the end of the hall (a noise response), not a charge at the door.
        SetPhase(ETallowTensionPhase::Build, Now);
        NextPassBy = Now + 9.f;
        NextWatch = Now;
        NextScare = Now + T.ScareCooldownMax;
        BeginProtectedWindow(8.f);
        break;
    case ETallowStoryAct::FuseHunt:
        // The cleared doorway is a win to feel, but only once it has really gone: relief declared over a live
        // chase read as the house lying (every logged first chase on 27 Oct was "relief" two seconds in).
        if (Phase != ETallowTensionPhase::Relax && Now - LastSeenBy > 10.f && WalkingDistance > 900.f) PendingRelaxAt = Now + 3.f;
        NextDisguise = FMath::Max(NextDisguise, Now + T.DisguiseDebutSeconds);
        break;
    case ETallowStoryAct::HouseWakes:
        // False calm: the hum comes on, the house holds its breath for 25 s, then the disguise debuts.
        SetPhase(ETallowTensionPhase::Quiet, Now);
        PhaseUntil = Now + 25.f;
        NextDisguise = FMath::Min(NextDisguise, Now + 50.f);
        NextPassBy = Now + 30.f;
        NextWatch = Now + 30.f;
        break;
    case ETallowStoryAct::Reel:
        BeginProtectedWindow(15.f);   // room to thread the film; the screening extends it
        break;
    case ETallowStoryAct::EndRun:
        SetPhase(ETallowTensionPhase::Build, Now);
        NextPassBy = Now + 18.f;
        NextWatch = Now + 8.f;
        break;
    default:
        break;
    }
}

void UTallowHunterDirector::NotePuzzlePayoff()
{
    if (!GetWorld() || Phase == ETallowTensionPhase::Relax) return;
    PendingRelaxAt = GetWorld()->GetTimeSeconds() + 4.f;
    UE_LOG(LogTemp, Display, TEXT("TALLOW_PACING puzzle payoff: relief in 4s if unseen"));
}

void UTallowHunterDirector::NoteProjectionComplete()
{
    if (!GetWorld()) return;
    const float Now = GetWorld()->GetTimeSeconds();
    Aggression = ClampAggression(Aggression + .08f);
    NextScare = FMath::Min(NextScare, Now + 15.f);   // a breath to stand up, then the jump-out is armed
    SetPhase(ETallowTensionPhase::Peak, Now);
    UE_LOG(LogTemp, Display, TEXT("TALLOW_PACING projection complete: surge to peak"));
}

void UTallowHunterDirector::BeginProtectedWindow(float Seconds)
{
    if (!GetWorld() || Seconds <= 0.f) return;
    ProtectedUntil = FMath::Max(ProtectedUntil, GetWorld()->GetTimeSeconds() + Seconds);
}

bool UTallowHunterDirector::IsProtected() const
{
    return GetWorld() && GetWorld()->GetTimeSeconds() < ProtectedUntil;
}

bool UTallowHunterDirector::AllowsHouseEvent(bool bMajorEvent) const
{
    if (!Tuning().bAllowHouseEvents || IsProtected() || Phase != ETallowTensionPhase::Build) return false;
    if (bMajorEvent && static_cast<uint8>(Act) < static_cast<uint8>(ETallowStoryAct::HouseWakes)) return false;
    return GetPhaseAge() > (bMajorEvent ? 20.f : 8.f);
}

bool UTallowHunterDirector::AllowsAmbientLie() const
{
    if (!Tuning().bAllowLies || IsProtected()) return false;
    if (Phase == ETallowTensionPhase::Build) return true;
    if (Phase != ETallowTensionPhase::Quiet) return false;
    const float Duration = PhaseUntil - PhaseStartedAt;
    return Duration > 1.f && GetPhaseAge() > .6f * Duration;   // late quiet: dread, not yet contact
}

void UTallowHunterDirector::ResetPressure()
{
    const float Now = GetWorld()->GetTimeSeconds();
    const FTallowArcTuning T = Tuning();
    Menace = 0.f;
    bBreather = false;
    NextHint = BreatherUntil = NextDistanceQuery = 0.f;
    LastNearby = Now;
    WalkingDistance = 100000.f;
    HintNumber = 0;
    PendingRelaxAt = CloseSince = -1.f;
    bWasClose = false;
    ProtectedUntil = 0.f;
    if (!bReviewPinned) Aggression = T.AggressionFloor;
    NextScare = Now + T.ScareCooldownMin;
    NextDisguise = Now + T.DisguiseCooldownMin;
    // Every start (and every restart after a capture) opens quiet: the first glimpse has to be earned.
    SetPhase(ETallowTensionPhase::Quiet, Now);
    NextPassBy = Now + FMath::FRandRange(T.PassByMin, T.PassByMax);
    NextWatch = Now + 20.f;
}

void UTallowHunterDirector::SetPhase(ETallowTensionPhase NewPhase, float Now)
{
    const FTallowArcTuning T = Tuning();
    // A fresh cycle (or an authored jump straight into a build) gets its own few watches.
    if (NewPhase == ETallowTensionPhase::Quiet || (NewPhase == ETallowTensionPhase::Build && Phase != ETallowTensionPhase::Quiet))
        WatchesThisBuild = 0;
    Phase = NewPhase;
    PhaseStartedAt = Now;
    switch (NewPhase)
    {
    case ETallowTensionPhase::Quiet: PhaseUntil = Now + FMath::FRandRange(T.QuietMin, T.QuietMax); bBreather = false; break;
    case ETallowTensionPhase::Build: PhaseUntil = Now + T.BuildSeconds; bBreather = false; break;
    case ETallowTensionPhase::Peak: PhaseUntil = Now + T.PeakSeconds; bBreather = false; break;
    // The breather is the existing retreat: area hints pull it away and a fruitless search ends in relief.
    case ETallowTensionPhase::Relax: PhaseUntil = BreatherUntil = Now + FMath::FRandRange(T.RelaxMin, T.RelaxMax); bBreather = true; break;
    }
    NextHint = 0.f;
    UE_LOG(LogTemp, Display, TEXT("TALLOW_PACING act=%d phase=%d for %.0fs aggression=%.2f"),
        static_cast<int32>(Act), static_cast<int32>(Phase), PhaseUntil - Now, Aggression);
}

float UTallowHunterDirector::GetPhaseAge() const
{
    return GetWorld()->GetTimeSeconds() - PhaseStartedAt;
}

void UTallowHunterDirector::NotePursuitLost()
{
    Aggression = ClampAggression(Aggression + .05f);
    ++Escapes;
    // A short hunt for her first, then the breather: losing her is a win the player should get to feel.
    if (Phase != ETallowTensionPhase::Relax) PendingRelaxAt = GetWorld()->GetTimeSeconds() + 8.f;
}

void UTallowHunterDirector::NoteCaught()
{
    // Being caught must not make the next attempt harder: the evening forgives down to the act's floor.
    if (!bReviewPinned) Aggression = Tuning().AggressionFloor;
}

void UTallowHunterDirector::NoteHiding()
{
    Aggression = ClampAggression(Aggression + .02f);
    ++Hides;
}

void UTallowHunterDirector::NoteNearMiss()
{
    if (Phase == ETallowTensionPhase::Peak || Phase == ETallowTensionPhase::Build)
        SetPhase(ETallowTensionPhase::Relax, GetWorld()->GetTimeSeconds());
}

bool UTallowHunterDirector::IsReturnOverdue() const
{
    // Away for too long only matters while the evening is building or peaking; quiet and relief are allowed to be long.
    if (Phase == ETallowTensionPhase::Quiet || Phase == ETallowTensionPhase::Relax) return false;
    return GetWorld()->GetTimeSeconds() - LastNearby > MaxAbsenceSeconds;
}

bool UTallowHunterDirector::CanScare() const
{
    return Tuning().bAllowScare && !IsProtected()
        && (Phase == ETallowTensionPhase::Build || Phase == ETallowTensionPhase::Peak) && GetWorld()->GetTimeSeconds() >= NextScare;
}

void UTallowHunterDirector::NoteScare()
{
    const FTallowArcTuning T = Tuning();
    NextScare = GetWorld()->GetTimeSeconds() + FMath::FRandRange(T.ScareCooldownMin, T.ScareCooldownMax);
}

bool UTallowHunterDirector::CanDisguise() const
{
    const float Now = GetWorld()->GetTimeSeconds();
    return Tuning().bAllowDisguise && !IsProtected()
        && (Phase == ETallowTensionPhase::Build || Phase == ETallowTensionPhase::Peak) && Now >= NextDisguise && Now >= NextScare - 120.f;
}

void UTallowHunterDirector::NoteDisguise()
{
    const FTallowArcTuning T = Tuning();
    NextDisguise = GetWorld()->GetTimeSeconds() + FMath::FRandRange(T.DisguiseCooldownMin, T.DisguiseCooldownMax);
}

bool UTallowHunterDirector::WantsPassBy() const
{
    return Tuning().bAllowPassBy && !IsProtected()
        && (Phase == ETallowTensionPhase::Build || (Phase == ETallowTensionPhase::Quiet && GetPhaseAge() > 10.f))
        && GetWorld()->GetTimeSeconds() >= NextPassBy;
}

bool UTallowHunterDirector::WantsWatch() const
{
    const FTallowArcTuning T = Tuning();
    return T.bAllowWatch && !IsProtected() && WatchesThisBuild < T.WatchesPerBuild
        && (Phase == ETallowTensionPhase::Build || (Phase == ETallowTensionPhase::Quiet && GetPhaseAge() > 10.f))
        && GetWorld()->GetTimeSeconds() >= NextWatch;
}

bool UTallowHunterDirector::CanWatchFromNoise() const
{
    // A sound is always answered; standing off to watch is one way of answering it. Protection does not forbid
    // it (a watcher is not a peak), but the budget does, and never once the peak or the relief has begun.
    const FTallowArcTuning T = Tuning();
    return T.bAllowWatch && WatchesThisBuild < T.WatchesPerBuild
        && (Phase == ETallowTensionPhase::Build || Phase == ETallowTensionPhase::Quiet)
        && GetWorld()->GetTimeSeconds() >= NextWatch - 10.f;
}

void UTallowHunterDirector::NoteWatch(bool bSlipped)
{
    const FTallowArcTuning T = Tuning();
    ++Watches; ++WatchesThisBuild;
    // Turned away by her light, it comes back sooner (and closer: GetWatchStage).
    NextWatch = GetWorld()->GetTimeSeconds() + FMath::FRandRange(T.WatchMin, T.WatchMax) * (bSlipped ? .6f : 1.f);
    UE_LOG(LogTemp, Display, TEXT("TALLOW_PACING watch %d (this build %d, slipped=%d)"), Watches, WatchesThisBuild, bSlipped ? 1 : 0);
}

bool UTallowHunterDirector::TakePassByHint(ATallowCreature* Keeper, ATallowCharacter* Player, FVector& OutPoint)
{
    if (!Keeper || !Player) return false;
    const float Now = GetWorld()->GetTimeSeconds();
    const FTallowArcTuning T = Tuning();
    NextPassBy = Now + FMath::FRandRange(T.PassByMin, T.PassByMax);
    const FVector Here = Player->GetActorLocation();
    const FVector Eye = Player->GetPawnViewLocation();
    const FVector FromKeeper = (Keeper->GetActorLocation() - Here).GetSafeNormal2D();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HunterPassBy), false, Player);
    Query.AddIgnoredActor(Keeper);
    float BestScore = -MAX_flt; bool bFound = false;
    for (TActorIterator<ATallowPatrolPoint> It(GetWorld()); It; ++It)
    {
        const FVector Point = It->GetActorLocation();
        // Far enough that it does not hear her breathe (550 cm) or echo her (700 cm) on the way past.
        UNavigationPath* PlayerPath = UNavigationSystemV1::FindPathToLocationSynchronously(this, Point, Here, Keeper->GetController());
        if (!PlayerPath || !PlayerPath->IsValid() || PlayerPath->IsPartial()) continue;
        const float Away = PlayerPath->GetPathLength();
        if (Away < 750.f || Away > 1300.f || FMath::Abs(Point.Z - Here.Z) > 150.f) continue;
        // She has to be able to see the spot from where she stands, or it is not a glimpse.
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Point + FVector(0, 0, 90), ECC_Visibility, Query)) continue;
        // Crossing her view: on the far side from where it is now, so the route sweeps past rather than at her.
        const FVector ToPoint = (Point - Here).GetSafeNormal2D();
        const float Across = -FVector::DotProduct(ToPoint, FromKeeper);
        if (Across < -.1f) continue;
        const float Score = Across * 200.f - FMath::Abs(Away - 950.f) * .3f + FMath::FRandRange(0.f, 120.f);
        if (Score > BestScore) { BestScore = Score; OutPoint = Point; bFound = true; }
    }
    if (bFound) ++PassBys;
    return bFound;
}

void UTallowHunterDirector::UpdatePressure(float DeltaTime, ATallowCreature* Keeper,
                                          ATallowCharacter* Player, bool bSeen)
{
    if (!Keeper || !Player) return;
    const float Now = GetWorld()->GetTimeSeconds();
    const FTallowArcTuning T = Tuning();
    if (Now >= NextDistanceQuery)
    {
        NextDistanceQuery = Now + 1.f;
        UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
            this, Keeper->GetActorLocation(), Player->GetActorLocation(), Keeper->GetController());
        WalkingDistance = Path && Path->IsValid() && !Path->IsPartial()
            ? Path->GetPathLength() : 100000.f;
    }
    // It learns across the act it is in, up to that act's ceiling -- not to full strength in minutes.
    if (!bReviewPinned) Aggression = ClampAggression(Aggression + DeltaTime * T.AggressionRate * (bSeen ? 2.f : 1.f));
    const bool bNear = WalkingDistance < 800.f;
    if (bNear || bSeen) LastNearby = Now;
    const float Rate = bBreather && !bSeen ? -.07f : bSeen ? .042f : bNear ? .022f : -.034f;
    Menace = FMath::Clamp(Menace + Rate * DeltaTime, 0.f, 1.f);
    // The tension cycle. Quiet runs out into a build; the build turns into a peak when it gets close or sees her;
    // a peak ends in relief (lost her, a near miss, or simply too long), and relief settles back into quiet.
    if (PendingRelaxAt > 0.f && Now >= PendingRelaxAt && !bSeen && Now - LastSeenBy > 4.f) { PendingRelaxAt = -1.f; SetPhase(ETallowTensionPhase::Relax, Now); }
    if (bSeen) LastSeenBy = Now;
    if (WalkingDistance < 450.f)
    {
        if (CloseSince < 0.f) CloseSince = Now;
        bWasClose = true;
    }
    else if (bWasClose && WalkingDistance > 900.f)
    {
        // It was right on top of her for a while, never saw her, and has gone again: a near miss.
        if (Now - CloseSince > 2.f && Now - LastSeenBy > 6.f) NoteNearMiss();
        bWasClose = false; CloseSince = -1.f;
    }
    if (Act == ETallowStoryAct::Dormant && !bSeen)
    {
        // Asleep: hold the quiet no matter how the clocks run. The play director wakes the arc.
        if (Phase != ETallowTensionPhase::Quiet) SetPhase(ETallowTensionPhase::Quiet, Now);
        PhaseUntil = Now + 30.f;
    }
    else switch (Phase)
    {
    case ETallowTensionPhase::Quiet:
        if (Now >= PhaseUntil) SetPhase(ETallowTensionPhase::Build, Now);
        else if (bSeen) SetPhase(ETallowTensionPhase::Peak, Now);
        break;
    case ETallowTensionPhase::Build:
        // The build is where dread lives. Being seen ends it at once; merely being close must wait
        // out the dwell, and a protected moment (reading, the screening) holds the peak off entirely.
        if (bSeen) SetPhase(ETallowTensionPhase::Peak, Now);
        else if (!IsProtected() && GetPhaseAge() >= T.BuildDwellBeforePeak && WalkingDistance < 700.f) SetPhase(ETallowTensionPhase::Peak, Now);
        else if (Now >= PhaseUntil)
        {
            if (IsProtected()) PhaseUntil = Now + 10.f;
            else SetPhase(ETallowTensionPhase::Peak, Now);
        }
        break;
    case ETallowTensionPhase::Peak:
        if (Now >= PhaseUntil && !bSeen) SetPhase(ETallowTensionPhase::Relax, Now);
        break;
    case ETallowTensionPhase::Relax:
        if (Now >= PhaseUntil && !bSeen) SetPhase(ETallowTensionPhase::Quiet, Now);
        break;
    }
    // The old pressure valve still works, as a relief: too much menace without being seen.
    if (Phase != ETallowTensionPhase::Relax && Menace >= .93f && !bSeen) SetPhase(ETallowTensionPhase::Relax, Now);
}

bool UTallowHunterDirector::TakeAreaHint(ATallowCreature* Keeper, ATallowCharacter* Player,
                                       FVector& OutArea, bool& bOutRetreat)
{
    if (!Keeper || !Player) return false;
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now < NextHint) return false;
    // It knows which part of the house she is in, never where exactly: hints come more often as the evening builds.
    NextHint = Now + (bBreather ? 11.f : Phase == ETallowTensionPhase::Peak ? 5.f : Phase == ETallowTensionPhase::Build ? 8.f : 14.f);
    const bool bOverdue = IsReturnOverdue();
    float BestScore = -MAX_flt;
    bool bFound = false;
    for (TActorIterator<ATallowPatrolPoint> It(GetWorld()); It; ++It)
    {
        const FVector Point = It->GetActorLocation();
        // A room above the player can be close through a floor but several
        // staircases away. Pacing must use the actual traversable route.
        UNavigationPath* PlayerPath = UNavigationSystemV1::FindPathToLocationSynchronously(
            this, Point, Player->GetActorLocation(), Keeper->GetController());
        if (!PlayerPath || !PlayerPath->IsValid() || PlayerPath->IsPartial()) continue;
        const float PlayerDistance = PlayerPath->GetPathLength();
        // Only existing area markers leave this subsystem. Never supply the
        // player's position, hiding spot, velocity, or a chase actor request.
        const float Farthest = Phase == ETallowTensionPhase::Quiet ? 2000.f : bOverdue ? 950.f : 1400.f;
        if (!bBreather && (PlayerDistance < 220.f || PlayerDistance > Farthest)) continue;
        UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
            this, Keeper->GetActorLocation(), Point, Keeper->GetController());
        if (!Path || !Path->IsValid() || Path->IsPartial()) continue;
        const float Length = Path->GetPathLength();
        if (Length < 100.f) continue;
        // Quiet keeps it in the far rooms; the build draws it in over a minute; the peak keeps it close.
        const float BuildPull = FMath::Clamp(GetPhaseAge() / 60.f, 0.f, 1.f);
        const float Preferred = bBreather ? 1300.f
            : Phase == ETallowTensionPhase::Quiet ? 1300.f
            : Phase == ETallowTensionPhase::Build ? FMath::Lerp(1400.f, 650.f, BuildPull)
            : bOverdue ? 450.f : 600.f;
        const float Variation = 80.f * FMath::Sin(float(GetTypeHash(It->GetFName()) % 997) + HintNumber * 2.1f);
        const float Score = -FMath::Abs(PlayerDistance - Preferred) - Length * .12f + Variation;
        if (Score > BestScore)
        {
            BestScore = Score;
            OutArea = Point;
            bFound = true;
        }
    }
    if (bFound) ++HintNumber;
    bOutRetreat = bBreather;
    return bFound;
}
