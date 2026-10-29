#include "Scare/TallowScareDirector.h"
#include "Environment/TallowHousePresence.h"
#include "Environment/TallowDoor.h"
#include "Plush/TallowPlushMim.h"
#include "Chapter/TallowChapterSubsystem.h"
#include "AI/Hunter/TallowHunterDirector.h"
#include "AI/TallowCreature.h"
#include "Audio/TallowSoundscapeComponent.h"
#include "Components/AudioComponent.h"
#include "House/TallowHouseRooms.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "Presentation/TallowPlayDirector.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
    // The house: x 0..1575, y -1300..0; anything placed inside it stays a little off the outer walls.
    FVector InsideHouse(FVector P) { P.X = FMath::Clamp(P.X, 60.f, 1515.f); P.Y = FMath::Clamp(P.Y, -1240.f, -60.f); return P; }
    // The floor overhead: the ground floor over the cellar, the bedrooms over the ground floor, the attic over them.
    float FloorAbove(int32 Floor) { return Floor == 0 ? 20.f : Floor == 1 ? 410.f : 770.f; }
}

bool UTallowScareDirector::DoesSupportWorldType(const EWorldType::Type Type) const { return Type == EWorldType::Game || Type == EWorldType::PIE; }
TStatId UTallowScareDirector::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(UTallowScareDirector, STATGROUP_Tickables); }
UTallowScareDirector* UTallowScareDirector::Get(const UObject* Context) { const auto* World = Context ? Context->GetWorld() : nullptr; return World ? World->GetSubsystem<UTallowScareDirector>() : nullptr; }
void UTallowScareDirector::BindChapter()
{
    if (!BoundChapter.IsValid()) if (auto* Chapters = UTallowChapterSubsystem::GetActive(this))
    {
        BoundChapter = Chapters;
        Chapters->OnPhaseChanged.AddUniqueDynamic(this, &UTallowScareDirector::OnPhaseChanged);
    }
}
void UTallowScareDirector::BeginChapter(int32 Index, const TArray<FTallowScareGroupDef>& Groups, const TArray<FName>& Enabled, const FTallowScareRules& Rules, int32 Seed)
{
    Stop(); Chapter = Index; ChapterEvents = 0; SampleAge = 0.f; Random.Initialize(Seed ^ 0x14735);
    Scheduler.Begin(Index, Groups, Enabled, Rules, Seed, GetWorld()->GetTimeSeconds());
    bOwnsScheduling = true; CurrentPhase = ETallowChapterPhase::None; ResumeAt = 0.f; BlockedSince = -1.f;
    for (TActorIterator<ATallowHousePresence> It(GetWorld()); It; ++It)
    {
        if (It->bCandlePractical) continue;
        OriginalSettings.Add({*It, It->bAlertsMim, It->NoiseLoudness, It->NoiseRange, It->EventSound});
        It->ResetForChapter();
        for (const auto& G : Scheduler.GetGroups())
        {
            if (G.bPlush || G.Place != ETallowScarePlace::Prop) continue;
            for (const auto& Pattern : G.ActorNamePatterns)
                if (It->GetName().MatchesWildcard(Pattern)) { Emitters.FindOrAdd(G.Group).AddUnique(*It); break; }
        }
    }
    for (auto& Pair : Emitters) Pair.Value.Sort([](const auto& A, const auto& B) { return A->GetName() < B->GetName(); });
    BindChapter();
}
void UTallowScareDirector::SetPhase(ETallowChapterPhase Phase) { CurrentPhase = Phase; }
void UTallowScareDirector::OnPhaseChanged(ETallowChapterPhase, ETallowChapterPhase Phase) { SetPhase(Phase); }
void UTallowScareDirector::Stop()
{
    bOwnsScheduling = false;
    if (BoundChapter.IsValid()) BoundChapter->OnPhaseChanged.RemoveDynamic(this, &UTallowScareDirector::OnPhaseChanged);
    BoundChapter.Reset();
    for (const auto& S : OriginalSettings) if (auto* E = S.Emitter.Get())
    {
        E->ResetForChapter(); E->bAlertsMim = S.bAlerts; E->NoiseLoudness = S.Loudness; E->NoiseRange = S.Range; E->SetEventSound(S.Sound.Get());
    }
    OriginalSettings.Reset(); Emitters.Reset(); CurrentPhase = ETallowChapterPhase::None;
    ClearSights();
}
bool UTallowScareDirector::IsCuePlaying() const { return Cue && Cue->IsPlaying(); }
UTallowSoundscapeComponent* UTallowScareDirector::FindSoundscape() const
{
    if (!Soundscape.IsValid())
        for (TActorIterator<ATallowPlayDirector> It(GetWorld()); It; ++It)
            if (auto* Mix = It->FindComponentByClass<UTallowSoundscapeComponent>()) { Soundscape = Mix; break; }
    return Soundscape.Get();
}
bool UTallowScareDirector::HouseAllowsScare(bool& bNormal, bool& bMajor) const
{
    bNormal = bMajor = false;
    if (IsCuePlaying()) return false;
    if (const UTallowSoundscapeComponent* Mix = FindSoundscape(); Mix && !Mix->AllowsMinorScare()) return false;
    // A protected moment (reading, threading the film, the screening) is never interrupted, hunt or not.
    const auto* Pacing = GetWorld()->GetSubsystem<UTallowHunterDirector>();
    if (Pacing && Pacing->IsProtected()) return false;
    if (!FTallowScareScheduler::IsHunt(CurrentPhase)) { bNormal = bMajor = true; return true; }
    // In a hunt the house plays its tricks in the quiet and the build, the dread before contact: never over the peak
    // (the chase), never inside the relief after it.
    if (!Pacing) { bNormal = true; return true; }
    if (Pacing->GetPhase() != ETallowTensionPhase::Quiet && Pacing->GetPhase() != ETallowTensionPhase::Build) return false;
    bNormal = true; bMajor = Pacing->AllowsHouseEvent(true);
    return true;
}
USoundBase* UTallowScareDirector::PickSound(const FTallowScareGroupDef& G)
{
    if (G.Sounds.IsEmpty()) return nullptr;
    const FString* Last = LastSound.Find(G.Group);
    int32 Pick = Random.RandRange(0, G.Sounds.Num() - 1);
    if (G.Sounds.Num() > 1 && Last && G.Sounds[Pick] == *Last) Pick = (Pick + 1 + Random.RandRange(0, G.Sounds.Num() - 2)) % G.Sounds.Num();
    const FString& Path = G.Sounds[Pick];
    TObjectPtr<USoundBase>& Sound = Loaded.FindOrAdd(Path);
    if (!Sound) Sound = LoadObject<USoundBase>(nullptr, *Path);
    if (!Sound) { UE_LOG(LogTemp, Warning, TEXT("TALLOW_SCARE missing %s"), *Path); return nullptr; }
    LastSound.Add(G.Group, Path);
    return Sound;
}
bool UTallowScareDirector::PlaceSound(const FTallowScareGroupDef& G, FVector& From, FVector& To)
{
    const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!Pawn) return false;
    FRandomStream& R = Random;
    const FVector Here = Pawn->GetActorLocation(), Ear = Here + FVector(0.f, 0.f, 40.f);
    const int32 Floor = TallowHouseRooms::FloorAt(Here.Z);
    FVector Mim(1e7f);
    if (TActorIterator<ATallowCreature> It(GetWorld()); It) Mim = It->GetActorLocation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ScarePlace), false, Pawn);
    auto Hidden = [&](const FVector& At) { FHitResult Hit; return GetWorld()->LineTraceSingleByChannel(Hit, Ear, At, ECC_Visibility, Query); };
    switch (G.Place)
    {
    case ETallowScarePlace::Above:
    {
        // Someone crossing the room overhead: a line through a point near above her head, walked while it sounds.
        const float Z = FloorAbove(Floor), Yaw = R.FRandRange(0.f, 360.f);
        const FVector Mid = InsideHouse(FVector(Here.X, Here.Y, Z) + FRotator(0.f, Yaw, 0.f).Vector() * R.FRandRange(60.f, 240.f));
        const FVector Dir = FRotator(0.f, Yaw + R.FRandRange(60.f, 120.f), 0.f).Vector();
        From = InsideHouse(Mid - Dir * 230.f); To = InsideHouse(Mid + Dir * 230.f); From.Z = To.Z = Z;
        return FVector::Dist(From, Mim) > 600.f;
    }
    case ETallowScarePlace::InWall:
    {
        // Inside the nearest wall within reach, a hand's depth behind the plaster.
        float Best = 260.f; bool bFound = false;
        for (int32 I = 0; I < 16; ++I)
        {
            const FVector Dir = FRotator(0.f, I * 22.5f + R.FRandRange(-6.f, 6.f), 0.f).Vector();
            FHitResult Hit;
            if (!GetWorld()->LineTraceSingleByChannel(Hit, Ear, Ear + Dir * 260.f, ECC_Visibility, Query) || Hit.Distance < 40.f || Hit.Distance >= Best) continue;
            if (Hit.GetActor() && (Hit.GetActor()->IsA<ATallowDoor>() || Hit.GetActor()->IsA<ATallowHousePresence>())) continue;
            if (FMath::Abs(Hit.ImpactNormal.Z) > .3f) continue;
            Best = Hit.Distance; bFound = true;
            From = To = Hit.ImpactPoint - Hit.ImpactNormal * 15.f + FVector(0.f, 0.f, R.FRandRange(-40.f, 50.f));
        }
        return bFound && FVector::Dist(From, Mim) > 600.f;
    }
    case ETallowScarePlace::Elsewhere:
    {
        // A door in another room: out of sight, not too near, not where she is.
        TArray<FVector> Doors;
        for (TActorIterator<ATallowDoor> It(GetWorld()); It; ++It)
        {
            const FVector At = It->GetComponentsBoundingBox().GetCenter();
            const float D = FVector::Dist(At, Ear);
            if (D > 350.f && D < 1100.f && FMath::Abs(At.Z - Ear.Z) < 450.f && FVector::Dist(At, Mim) > 700.f && Hidden(At)) Doors.Add(At);
        }
        if (Doors.IsEmpty()) return false;
        From = To = Doors[R.RandRange(0, Doors.Num() - 1)];
        return true;
    }
    case ETallowScarePlace::Near:
    {
        // A room close by, out of sight: the preferred rooms first (the music box lives with the toys).
        const FName Mine = TallowHouseRooms::RoomAt(Here);
        TArray<FVector> Spots, Preferred;
        for (const FTallowRoomDef& Room : TallowHouseRooms::All())
        {
            if (Room.Id == Mine || FMath::Abs(Room.Floor - Floor) > 1) continue;
            const FVector At = Room.Anchor() + FVector(R.FRandRange(-120.f, 120.f), R.FRandRange(-140.f, 140.f), 0.f);
            const float D = FVector::Dist(At, Ear);
            if (D < 350.f || D > 1050.f || FVector::Dist(At, Mim) < 700.f || !Hidden(At)) continue;
            (G.PreferredRooms.Contains(Room.Id) ? Preferred : Spots).Add(At);
        }
        const TArray<FVector>& Use = Preferred.IsEmpty() ? Spots : Preferred;
        if (Use.IsEmpty()) return false;
        From = To = Use[R.RandRange(0, Use.Num() - 1)];
        return true;
    }
    default: return false;
    }
}
void UTallowScareDirector::MakeAttenuation()
{
    if (PlacedAttenuation) return;
    // Somewhere in the house: falls off with distance, darkens with it, and the walls between muffle it.
    PlacedAttenuation = NewObject<USoundAttenuation>(this);
    FSoundAttenuationSettings& S = PlacedAttenuation->Attenuation;
    S.bAttenuate = S.bSpatialize = true; S.AttenuationShapeExtents = FVector(200.f); S.FalloffDistance = 2600.f;
    S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound; S.dBAttenuationAtMax = -24.f;
    S.bAttenuateWithLPF = true; S.LPFRadiusMin = 500.f; S.LPFRadiusMax = 3000.f; S.LPFFrequencyAtMin = 20000.f; S.LPFFrequencyAtMax = 3500.f;
    S.bEnableOcclusion = true; S.OcclusionLowPassFilterFrequency = 2200.f; S.OcclusionVolumeAttenuation = .75f;
    // Through a ceiling or inside a wall: the recording is already filtered for that, so no second muffling.
    ThroughAttenuation = NewObject<USoundAttenuation>(this);
    FSoundAttenuationSettings& T = ThroughAttenuation->Attenuation;
    T = S; T.bEnableOcclusion = false; T.FalloffDistance = 1800.f; T.bAttenuateWithLPF = false;
}
bool UTallowScareDirector::IsSeenGroup(int32 I) const
{
    const auto& G = Scheduler.GetGroups()[I];
    return G.bPlush || G.Place == ETallowScarePlace::Prop || IsSight(G.Place);
}
bool UTallowScareDirector::FireSound(const FTallowScareGroupDef& G, bool bAlert, FString& Where)
{
    UTallowSoundscapeComponent* Mix = FindSoundscape();
    if (G.Place == ETallowScarePlace::Head) { Where = TEXT("whisper"); return Mix && Mix->PlayWhisperPass(); }
    if (G.Place == ETallowScarePlace::Herring) { Where = TEXT("herring"); return Mix && Mix->RequestDistraction(); }
    FVector From, To;
    if (!PlaceSound(G, From, To)) return false;
    USoundBase* Sound = PickSound(G);
    if (!Sound) return false;
    MakeAttenuation();
    const bool bThrough = G.Place == ETallowScarePlace::Above || G.Place == ETallowScarePlace::InWall;
    Cue = UGameplayStatics::SpawnSoundAtLocation(this, Sound, From, FRotator::ZeroRotator, G.Volume, Random.FRandRange(.95f, 1.03f), 0.f,
        bThrough ? ThroughAttenuation.Get() : PlacedAttenuation.Get());
    if (!Cue) return false;
    CueFrom = From; CueTo = To; CueStarted = GetWorld()->GetTimeSeconds(); CueSeconds = From.Equals(To, 1.f) ? 0.f : FMath::Max(Sound->GetDuration(), .5f);
    // In a hunt some of the house's noises are real enough for Mim to come and look.
    if (bAlert) if (UTallowNoiseSubsystem* Noise = UTallowNoiseSubsystem::Get(this)) Noise->EmitCustomNoise(From, nullptr, G.NoiseLoudness, G.NoiseRange, true);
    Where = FString::Printf(TEXT("%s@%s"), *Sound->GetName(), *From.ToCompactString());
    return true;
}
bool UTallowScareDirector::Fire(int32 Index)
{
    const auto& G = Scheduler.GetGroups()[Index];
    AActor* Fired = nullptr;
    FString Where;
    const bool bAlert = !G.bPlush && G.bAlertsMimInHunt && FTallowScareScheduler::IsHunt(CurrentPhase);
    if (G.bPlush)
    {
        for (TActorIterator<ATallowPlushMim> It(GetWorld()); It; ++It)
            if (It->TryMinorScare(G.Group)) { Fired = *It; break; }
    }
    else if (G.Place == ETallowScarePlace::Prop)
    {
        if (auto* List = Emitters.Find(G.Group); List && !List->IsEmpty())
        {
            const int32 Start = Random.RandRange(0, List->Num() - 1);
            for (int32 N = 0; N < List->Num(); ++N) if (auto* E = (*List)[(Start + N) % List->Num()].Get())
            {
                if (E->IsHidden()) continue;   // a prop a sight scare has taken away for the chapter
                E->bChapterSilent = G.bSilent; E->bAlertsMim = bAlert; E->NoiseLoudness = G.NoiseLoudness; E->NoiseRange = G.NoiseRange;
                if (!G.Sounds.IsEmpty() && !G.bSilent) { E->SetEventSound(PickSound(G)); E->SetEventVolume(G.Volume); }
                if (E->TryTriggerEvent()) { Fired = E; break; }
            }
        }
    }
    else if (IsSight(G.Place)) { MakeAttenuation(); if (!FireSight(G, Where, Fired)) return false; }
    else if (!FireSound(G, bAlert, Where)) return false;
    if (!Fired && Where.IsEmpty()) return false;
    Scheduler.Commit(Index, GetWorld()->GetTimeSeconds()); ChapterEvents = Scheduler.GetCount();
    OnScareFired.Broadcast(G.Group, Fired, bAlert);
    UE_LOG(LogTemp, Display, TEXT("TALLOW_SCARE ch=%d group=%s emitter=%s alerted=%d next=%.0f"), Chapter, *G.Group.ToString(), !Where.IsEmpty() ? *Where : Fired ? *Fired->GetName() : TEXT("none"), bAlert, Scheduler.GetGap());
    return true;
}
bool UTallowScareDirector::TryFireGroup(FName Group)
{
    if (!bOwnsScheduling || GetWorld()->IsPaused()) return false;
    bool bNormal = false, bMajor = false;
    if (!HouseAllowsScare(bNormal, bMajor)) return false;
    const float Now = GetWorld()->GetTimeSeconds();
    for (int32 I = 0; I < Scheduler.GetGroups().Num(); ++I)
        if (Scheduler.GetGroups()[I].Group == Group && Scheduler.CanFire(I, CurrentPhase, Now, bNormal, bMajor)) return Fire(I);
    return false;
}
bool UTallowScareDirector::FireGroupForReview(FName Group)
{
    for (int32 I = 0; I < Scheduler.GetGroups().Num(); ++I)
        if (Scheduler.GetGroups()[I].Group == Group) return Fire(I);
    return false;
}
void UTallowScareDirector::Tick(float Dt)
{
    // An overhead cue walks across the room while it sounds.
    if (Cue && Cue->IsPlaying() && CueSeconds > 0.f)
        Cue->SetWorldLocation(FMath::Lerp(CueFrom, CueTo, FMath::Clamp((GetWorld()->GetTimeSeconds() - CueStarted) / CueSeconds, 0.f, 1.f)));
    if (GetWorld()->IsPaused()) return;
    UpdateSights(GetWorld()->GetTimeSeconds());
    if (!bOwnsScheduling) return;
    BindChapter(); SampleAge += Dt;
    if (SampleAge < 2.f) return;
    SampleAge = 0.f;
    const float Now = GetWorld()->GetTimeSeconds();
    bool bNormal = false, bMajor = false;
    if (!HouseAllowsScare(bNormal, bMajor))
    {
        if (BlockedSince < 0.f) BlockedSince = Now;
        SightWaitSince = -1.f;
        return;
    }
    // After a long hold (a chase, a reading, a relief) the house waits a little before it starts again, so the first
    // thing after the danger is never on a beat the player could learn.
    if (BlockedSince >= 0.f)
    {
        if (Now - BlockedSince > 6.f) ResumeAt = Now + Random.FRandRange(8.f, 26.f);
        BlockedSince = -1.f;
    }
    if (Now < ResumeAt) return;
    // Something she can see comes first. When the one drawn cannot happen where she is looking, the others she could see
    // are tried; sounds out of sight wait until the house has held out for a sight a while (SightPatienceSeconds).
    TArray<int32> Available;
    for (int32 I = 0; I < Scheduler.GetGroups().Num(); ++I) Available.Add(I);
    bool bSightMissed = false;
    while (!Available.IsEmpty())
    {
        const int32 I = Scheduler.Choose(CurrentPhase, Now, bNormal, bMajor, &Available);
        if (I == INDEX_NONE) break;
        if (Fire(I)) { SightWaitSince = -1.f; return; }
        Available.Remove(I);
        if (!bSightMissed && IsSeenGroup(I))
        {
            bSightMissed = true;
            if (SightWaitSince < 0.f) SightWaitSince = Now;
            if (Now - SightWaitSince < Scheduler.GetRules().SightPatienceSeconds) Available.RemoveAll([this](int32 J) { return !IsSeenGroup(J); });
        }
    }
}
void UTallowScareDirector::Describe(TArray<FString>& Lines) const
{
    Lines.Add(FString::Printf(TEXT("TALLOW_SCARE ch=%d owned=%d phase=%d count=%d last=%.2f gap=%.0f"), Chapter, bOwnsScheduling, int32(CurrentPhase), ChapterEvents, Scheduler.GetLastFireTime(), Scheduler.GetGap()));
    for (int32 I = 0; I < Scheduler.GetGroups().Num(); ++I)
        Lines.Add(FString::Printf(TEXT("group=%s ready=%.2f"), *Scheduler.GetGroups()[I].Group.ToString(), Scheduler.GetReadyAt(I)));
}
