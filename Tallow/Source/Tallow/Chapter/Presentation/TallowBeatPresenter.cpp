#include "Chapter/Presentation/TallowBeatPresenter.h"
#include "Chapter/Presentation/TallowBloodWriting.h"
#include "Chapter/TallowChapterSubsystem.h"
#include "Chapter/TallowChairSpot.h"
#include "AI/TallowCreature.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Environment/TallowCandlePickup.h"
#include "Environment/TallowDoor.h"
#include "GameFramework/PlayerController.h"
#include "House/TallowHouseRooms.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Player/TallowCharacter.h"
#include "Presentation/TallowPlayDirector.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Tasks/TallowChairEscape.h"
#include "Tasks/TallowFuseBox.h"
#include "UnrealClient.h"
namespace
{
    using B = ETallowChapterBeat;
    /** Eyelid keys (seconds, closure) eased between; closure 1 is shut. */
    float Lid(TConstArrayView<FVector2f> Keys, float T)
    {
        if (T <= Keys[0].X) return Keys[0].Y;
        for (int32 I = 1; I < Keys.Num(); ++I)
            if (T < Keys[I].X) return FMath::Lerp(Keys[I - 1].Y, Keys[I].Y, FMath::SmoothStep(0.f, 1.f, (T - Keys[I - 1].X) / (Keys[I].X - Keys[I - 1].X)));
        return Keys.Last().Y;
    }
    // Restraint: two heavy half-blinks that come back up, then the lids go all the way. Shut while she is tied. They
    // part from a slit, the upper lid first, sink once, then hold open.
    const FVector2f RestraintLids[] = {{0.f, 0.f}, {.45f, 0.f}, {.9f, .52f}, {1.3f, .06f}, {1.7f, .1f}, {2.15f, .58f}, {2.45f, .5f},
        {2.8f, .16f}, {3.05f, .2f}, {3.6f, 1.f}, {5.6f, 1.f}, {5.95f, .9f}, {6.6f, .36f}, {6.95f, .46f}, {7.6f, 0.f}};
    constexpr float RestraintCut = 3.65f, RestraintEnd = 8.2f;
    // Release: she cannot look away from it, and then she blinks.
    const FVector2f ReleaseLids[] = {{0.f, 0.f}, {2.9f, 0.f}, {3.f, .3f}, {3.1f, 1.f}, {3.38f, 1.f}, {3.62f, 0.f}};
    constexpr float ReleaseCut = 3.2f, ReleaseEnd = 7.4f;
    constexpr float WritingEnd = 7.6f;
    constexpr float FuseBlow = 2.72f, FuseCut = 2.86f, FuseEnd = 5.3f;
    constexpr float ResetSlam = 1.3f, ResetCut = 1.55f, ResetEnd = 6.6f;
    const TCHAR* const Stings = TEXT("/Game/FinalPass/Beats/Audio/");
    bool IsStaged(ETallowChapterBeat Beat)
    { return Beat == B::BloodWriting || Beat == B::Restraint || Beat == B::HuntRelease || Beat == B::FuseEvent || Beat == B::ChapterReset; }
    float Flicker(float T, int32 Seed) { return FMath::Frac(FMath::Sin(FMath::FloorToFloat(T * 24.f) * 12.9898f + Seed * 78.233f) * 43758.5453f); }
}
bool UTallowBeatPresenter::DoesSupportWorldType(const EWorldType::Type WorldType) const { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }
TStatId UTallowBeatPresenter::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(UTallowBeatPresenter, STATGROUP_Tickables); }
void UTallowBeatPresenter::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (auto* Chapters = InWorld.GetSubsystem<UTallowChapterSubsystem>())
    {
        Chapters->OnBeatRequested.AddUniqueDynamic(this, &UTallowBeatPresenter::OnBeatRequested);
        Chapters->OnRoundReset.AddUniqueDynamic(this, &UTallowBeatPresenter::OnRoundReset);
    }
    // Shut eyes: the house goes far away and dull. The silence after the fuse takes everything but her.
    Muffle = NewObject<USoundMix>(this, TEXT("BeatEyesShut")); Muffle->FadeInTime = 1.1f; Muffle->FadeOutTime = 1.6f; Muffle->Duration = -1.f;
    Silence = NewObject<USoundMix>(this, TEXT("BeatSilence")); Silence->FadeInTime = .03f; Silence->FadeOutTime = 2.6f; Silence->Duration = -1.f;
    for (const TCHAR* Name : {TEXT("Ambience"), TEXT("Tension"), TEXT("Distractions"), TEXT("Moments"), TEXT("PlayerFoley"), TEXT("Mim"), TEXT("MimTells"), TEXT("Story")})
        if (auto* Class = LoadObject<USoundClass>(nullptr, *FString::Printf(TEXT("/Game/Audio/Mix/SC_Tallow_%s.SC_Tallow_%s"), Name, Name)))
        {
            FSoundClassAdjuster Shut; Shut.SoundClassObject = Class; Shut.VolumeAdjuster = .4f; Shut.LowPassFilterFrequency = 380.f;
            Muffle->SoundClassEffects.Add(Shut);
            FSoundClassAdjuster Gone; Gone.SoundClassObject = Class; Gone.VolumeAdjuster = .02f;
            Silence->SoundClassEffects.Add(Gone);
        }
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("TallowBeatsRehearse"))) RehearseStep = 0;
    if (FParse::Param(FCommandLine::Get(), TEXT("TallowBeatsWords"))) { RehearseStep = 0; WordsChapter = 0; }
    FParse::Value(FCommandLine::Get(), TEXT("TallowBeatsFilm="), FilmDir);
#endif
}
void UTallowBeatPresenter::Deinitialize() { Clear(); Super::Deinitialize(); }
ATallowCharacter* UTallowBeatPresenter::GetPlayer() const { return Cast<ATallowCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)); }
AActor* UTallowBeatPresenter::GetMim() const
{
    if (auto* Director = Cast<ATallowPlayDirector>(UGameplayStatics::GetActorOfClass(this, ATallowPlayDirector::StaticClass()))) return Director->GetKeeper();
    return UGameplayStatics::GetActorOfClass(this, ATallowCreature::StaticClass());
}
void UTallowBeatPresenter::OnBeatRequested(ETallowChapterBeat InBeat, const FTallowBeatContext& InContext)
{
    if (!IsStaged(InBeat)) return;
    auto* Chapters = UTallowChapterSubsystem::GetActive(this);
    auto* Player = GetPlayer();
    if (!Chapters || !Player) return;
    if ((InBeat == B::Restraint || InBeat == B::HuntRelease) && !Lids)
    {
        if (auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FinalPass/Beats/M_BeatEyelids.M_BeatEyelids")))
            Lids = UMaterialInstanceDynamic::Create(Material, this);
        if (!Lids) { UE_LOG(LogTemp, Warning, TEXT("TALLOW_BEATS missing M_BeatEyelids; beat %d left to the fade"), int32(InBeat)); return; }
    }
    if (Beat != B::None) Abort();
    if (!Chapters->ClaimBeat(InBeat, InContext.Serial)) return;
    Beat = InBeat; Serial = InContext.Serial; Context = InContext; StartedAt = GetWorld()->GetTimeSeconds();
    Fired.Reset(); bCutDone = false; bHoldingBlack = false;
    UE_LOG(LogTemp, Display, TEXT("TALLOW_BEATS claimed beat=%s serial=%d"), *StaticEnum<ETallowChapterBeat>()->GetNameStringByValue(int64(Beat)), Serial);
    if (Beat == B::Restraint || Beat == B::HuntRelease)
    {
        LowerLid = 0.f; LidsAt = GetWorld()->GetTimeSeconds(); SetLids(0.f, 0.f, 5.f, 0.f, 0.f);
        Player->GetFirstPersonCameraComponent()->AddOrUpdateBlendable(Lids, 1.f);
    }
    if (const AActor* Mim = GetMim()) MimWas = Mim->GetActorLocation();
    if (Beat == B::BloodWriting) BeginWriting();
    if (Beat == B::FuseEvent)
    {
        AActor* Box = Context.Subject ? Context.Subject.Get() : UGameplayStatics::GetActorOfClass(this, ATallowFuseBox::StaticClass());
        FuseAt = Box ? Box->GetActorLocation() + Box->GetActorForwardVector() * 35.f + FVector(0, 0, 25.f) : Player->GetActorLocation();
        Spark = NewObject<UPointLightComponent>(Box ? (UObject*)Box : (UObject*)Player);
        Spark->SetMobility(EComponentMobility::Movable); Spark->RegisterComponentWithWorld(GetWorld());
        Spark->SetWorldLocation(FuseAt); Spark->SetIntensityUnits(ELightUnits::Candelas); Spark->SetIntensity(0.f);
        Spark->SetAttenuationRadius(1400.f); Spark->SetLightColor(FLinearColor(.72f, .84f, 1.f)); Spark->SetSourceRadius(1.5f);
        Spark->SetCastShadows(true);
        // The house's own dead fittings near the box and near her catch the surge too.
        const FName PlayerRoom = TallowHouseRooms::RoomAt(Player->GetActorLocation()), BoxRoom = TallowHouseRooms::RoomAt(FuseAt);
        for (TActorIterator<AActor> It(GetWorld()); It && Surged.Num() < 12; ++It)
        {
            if (*It == Player || It->IsA<ATallowCandlePickup>() || (It->ActorHasTag(TEXT("Tallow.AuthoredLight")) && !It->ActorHasTag(TEXT("Tallow.PowerLight")))) continue;
            for (UPointLightComponent* Light : TInlineComponentArray<UPointLightComponent*>(*It))
            {
                const FName Room = TallowHouseRooms::RoomAt(Light->GetComponentLocation());
                const bool bNear = FVector::Dist(Light->GetComponentLocation(), FuseAt) < 1600.f || FVector::Dist(Light->GetComponentLocation(), Player->GetActorLocation()) < 1200.f;
                if (Light->Intensity > 0.f || (Room != PlayerRoom && Room != BoxRoom && !bNear) || Surged.Num() >= 12) continue;
                Surged.Add({Light, Light->Intensity, Light->IsVisible()});
            }
        }
        UE_LOG(LogTemp, Display, TEXT("TALLOW_BEATS fuse surge lights=%d box=%s"), Surged.Num(), *FuseAt.ToCompactString());
    }
}
void UTallowBeatPresenter::OnRoundReset(int32, ETallowRestartReason)
{
    // A new round clears the wall; a retry or jump also ends whatever was staged. The reset's own cut is expected.
    if (Writing) { Writing->Destroy(); Writing = nullptr; }
    if (!bInCut && Beat != B::None) Abort();
}
void UTallowBeatPresenter::BeginWriting()
{
    auto* Player = GetPlayer();
    FVector Point, Normal;
    if (!ATallowBloodWriting::FindWall(GetWorld(), Context.Where, Context.Subject, Point, Normal))
    {
        // No wall found by the anchor: write it on what she is looking at.
        FHitResult Hit; FVector Eye; FRotator View; Player->GetController()->GetPlayerViewPoint(Eye, View);
        FCollisionQueryParams Params(TEXT("TallowBloodWall"), false, Player);
        if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + View.Vector() * 900.f, ECC_Visibility, Params)) { Point = Hit.ImpactPoint; Normal = Hit.ImpactNormal; }
        else { Point = Context.Where.GetLocation(); Normal = -View.Vector(); }
    }
    if (Writing) Writing->Destroy();
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Writing = GetWorld()->SpawnActor<ATallowBloodWriting>(Point, FRotator::ZeroRotator, Spawn);
    if (Writing) Writing->Write(Context.Text, Point, Normal, Context.ChapterIndex * 131 + 17, Context.Where.GetScale3D().X);
    // Her eyes are drawn by the sound and the wall, not taken: look and move stay hers while it forms.
    if (auto* PC = Cast<APlayerController>(Player->GetController()); PC && PC->IsMoveInputIgnored())
    { PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false); bInputReleased = true; }
#if !UE_BUILD_SHIPPING
    if (RehearseStep >= 0)
    {
        // Stand where the clue was heard (the machine's controls side), facing the wall.
        const AActor* Story = Context.Subject.Get();
        FVector Foot = Story ? Story->GetActorLocation() + Story->GetActorForwardVector() * 110.f : Point + Normal.GetSafeNormal2D() * 300.f;
        FHitResult Floor; FCollisionQueryParams Params(TEXT("TallowBeatsFloor"), false, Player);
        if (GetWorld()->LineTraceSingleByChannel(Floor, Foot + FVector(0, 0, 50.f), Foot - FVector(0, 0, 400.f), ECC_Visibility, Params)) Foot = Floor.ImpactPoint;
        Player->TeleportTo(Foot + FVector(0, 0, 92.f), FRotator::ZeroRotator, false, true);
        Player->GetController()->SetControlRotation((Point + FVector(0, 0, 20.f) - Player->GetFirstPersonCameraComponent()->GetComponentLocation()).Rotation());
        UE_LOG(LogTemp, Display, TEXT("TALLOW_BEATS rehearse listening at %s, wall %.0f cm away"), *Foot.ToCompactString(), FVector::Dist2D(Foot, Point));
    }
#endif
}
bool UTallowBeatPresenter::Once(float T, float At)
{
    if (T < At) return false;
    bool bAlready = false; Fired.Add(FMath::RoundToInt(At * 1000.f), &bAlready);
    return !bAlready;
}
void UTallowBeatPresenter::Tick(float DeltaTime)
{
    auto* Chapters = GetWorld()->GetSubsystem<UTallowChapterSubsystem>();
#if !UE_BUILD_SHIPPING
    if (!UGameplayStatics::IsGamePaused(this)) { Rehearse(); Film(); }
#endif
    if (Beat == B::None) return;
    // A pause holds the moment; the menu should not be heard through shut eyes.
    if (UGameplayStatics::IsGamePaused(this))
    {
        if (!bPausedMixes) { for (USoundMix* Mix : Pushed) UGameplayStatics::PopSoundMixModifier(this, Mix); bPausedMixes = true; }
        return;
    }
    if (bPausedMixes) { for (USoundMix* Mix : Pushed) UGameplayStatics::PushSoundMixModifier(this, Mix); bPausedMixes = false; }
    // Caught, a retry, a jump or the subsystem's timeout ended it: let go at once.
    if ((!bCutDone || Beat != B::ChapterReset) && (!Chapters || Chapters->GetActiveBeat() != Beat)) { Abort(); return; }
    const float T = float(GetWorld()->GetTimeSeconds() - StartedAt);
    switch (Beat)
    {
    case B::BloodWriting: StageWriting(T); break;
    case B::Restraint: StageRestraint(T); break;
    case B::HuntRelease: StageRelease(T); break;
    case B::FuseEvent: StageFuse(T); break;
    case B::ChapterReset: StageReset(T); break;
    default: break;
    }
    if (Beat != B::None && Context.MaxSeconds > 1.f && T > Context.MaxSeconds - .5f) { UE_LOG(LogTemp, Warning, TEXT("TALLOW_BEATS overran beat=%d"), int32(Beat)); Finish(); }
}
void UTallowBeatPresenter::StageWriting(float T)
{
    const FVector Wall = Writing ? Writing->GetActorLocation() : Context.Where.GetLocation();
    // It lands with a shock, then is heard being written: a wet drag across the plaster, letter after letter.
    if (Once(T, 0.f)) Play(*FString::Printf(TEXT("%sS_Beat_StingHigh1"), Stings), .62f);
    if (Once(T, .1f)) Play(TEXT("/Game/Audio/Tension/T_Scratch_Wall_01"), 1.f, &Wall, .82f);
    if (Once(T, 1.6f)) Play(TEXT("/Game/Audio/Tension/T_Scratch_Wall_02"), .95f, &Wall, .78f);
    if (Once(T, 3.1f)) Play(TEXT("/Game/Audio/Tension/T_Scratch_Wall_03"), .8f, &Wall, .74f);
    if (Once(T, 4.6f)) Play(TEXT("/Game/Audio/Tension/T_Whisper_01"), .45f, &Wall);
    if (T >= WritingEnd) Finish();
}
void UTallowBeatPresenter::StageRestraint(float T)
{
    const bool bTremble = (T > 2.f && T < 3.5f) || (T > 5.6f && T < 7.6f);
    // Warm with the light through them while they move; black once shut a moment; warm again as they part.
    const float Dark = FMath::Clamp((T - 3.8f) / .8f, 0.f, 1.f) * (1.f - FMath::Clamp((T - 5.45f) / .4f, 0.f, 1.f));
    SetLids(Lid(RestraintLids, T), bTremble ? 1.f : 0.f, .45f, Dark, T > 5.6f ? 1.f - FMath::Clamp((T - 5.6f) / 2.2f, 0.f, 1.f) : 0.f);
    if (Once(T, 1.6f)) SetMix(Muffle, true);
    if (Once(T, 1.8f)) Play(TEXT("/Game/Audio/Player/P_Gasp_01"), .35f, nullptr, .9f);
    if (Once(T, RestraintCut)) Cut();
    // Under the lids: only her breath, and the house a long way off.
    if (Once(T, 3.9f)) Play(TEXT("/Game/Audio/Player/P_Breath_Out"), .3f, nullptr, .85f);
    if (Once(T, 5.4f)) SetMix(Muffle, false);
    if (Once(T, 6.55f)) Play(*FString::Printf(TEXT("%sS_Beat_StingLow9"), Stings), .55f);
    if (T >= RestraintEnd) Finish();
}
void UTallowBeatPresenter::StageRelease(float T)
{
    SetLids(Lid(ReleaseLids, T), T > 1.8f && T < 2.95f ? .6f : 0.f, 5.f, 0.f, 0.f);
    const FVector Mim = MimWas;
    if (Once(T, .35f)) Play(TEXT("/Game/Audio/Creature/V_Mim_Breath_mim_breath_03"), .7f, &Mim);
    if (Once(T, 1.7f)) Play(TEXT("/Game/Audio/Creature/V_Mim_Search_mim_whisper_02"), .55f, &Mim);
    if (Once(T, ReleaseCut)) Cut();
    // Gone. Somewhere past the door it runs, on all its hands, and a door closes. Then nothing.
    const FVector Away = Context.Where.GetLocation().IsNearlyZero() ? Mim : Context.Where.GetLocation();
    for (int32 Step = 0; Step < 9; ++Step)
        if (Once(T, 3.95f + Step * .14f))
        {
            const FVector At = FMath::Lerp(Mim, Away, .25f + Step * .08f);
            Play(*FString::Printf(TEXT("/Game/Audio/CC0/K_Step_%02d"), Step % 4), FMath::Lerp(.95f, .35f, Step / 8.f), &At, 1.15f);
        }
    if (Once(T, 5.45f)) Play(TEXT("/Game/Audio/CC0/D_Close_01"), .75f, &Away, .8f);
    if (Once(T, 6.1f)) Play(*FString::Printf(TEXT("%sS_Beat_StingHigh11"), Stings), .42f);
    if (T >= ReleaseEnd) Finish();
}
void UTallowBeatPresenter::StageFuse(float T)
{
    const FVector Box = FuseAt;
    if (Once(T, .22f)) Play(TEXT("/Game/Audio/CC0/P_Breaker_Clunk"), 1.f, &Box, .82f);
    if (Once(T, .3f))
    {
        if (auto* Hum = Hold(TEXT("/Game/Audio/CC0/H_Power_Hum"), 1.f, &Box)) Hum->SetPitchMultiplier(.7f);
        Hold(*FString::Printf(TEXT("%sS_Beat_StingHighBig1"), Stings), .5f);
    }
    if (!Held.IsEmpty() && Held[0] && T < FuseBlow) Held[0]->SetPitchMultiplier(FMath::Lerp(.7f, 1.35f, FMath::Clamp((T - .3f) / 2.4f, 0.f, 1.f)));
    // The lamps try: stuttering, climbing, the box spitting blue light. Somewhere Mim feels it.
    float Level = 0.f;
    if (T > .3f && T < FuseBlow)
    {
        const float Climb = FMath::SmoothStep(.3f, 2.5f, T);
        Level = Flicker(T, 3) > .42f ? Climb * (.35f + .65f * Flicker(T, 7)) : .04f * Climb;
    }
    else if (T >= FuseBlow && T < FuseBlow + .07f) Level = 3.f;
    SurgeLights(Level);
    const FVector Mim = MimWas;
    if (Once(T, .95f)) Play(TEXT("/Game/Audio/Tension/K_Shriek_Distant_02"), 1.f, &Mim);
    for (int32 Step = 0; Step < 9; ++Step)
        if (Once(T, 1.45f + Step * .13f)) Play(*FString::Printf(TEXT("/Game/Audio/CC0/K_Step_%02d"), Step % 4), .8f, &Mim, 1.2f);
    // It blows. Everything stops at once: the hum, the running, the house.
    if (Once(T, FuseBlow + .07f))
    {
        for (UAudioComponent* Audio : Held) if (Audio) Audio->Stop();
        Held.Reset(); SetMix(Silence, true); Fade(0.f, 1.f, .06f);
    }
    if (Once(T, FuseCut)) Cut();
    if (Once(T, 3.7f)) Fade(1.f, 0.f, 1.4f);
    if (Once(T, 4.3f)) SetMix(Silence, false);
    if (T >= FuseEnd) Finish();
}
void UTallowBeatPresenter::StageReset(float T)
{
    auto* Player = GetPlayer();
    if (!Player) { Finish(); return; }
    if (Once(T, .45f)) { const FVector Behind = Player->GetActorLocation() - Player->GetActorForwardVector() * 300.f + FVector(0, 0, 150.f); Play(TEXT("/Game/Audio/CC0/H_Settle_01"), 1.f, &Behind, .72f); }
    if (Once(T, ResetSlam))
    {
        // The nearest door in the house slams; the dark comes down with it.
        FVector Where = Player->GetActorLocation() - Player->GetActorForwardVector() * 250.f; float Best = 900.f;
        for (TActorIterator<ATallowDoor> It(GetWorld()); It; ++It)
            if (const float Distance = FVector::Dist(It->GetActorLocation(), Player->GetActorLocation()); Distance < Best) { Best = Distance; Where = It->GetActorLocation(); }
        Play(TEXT("/Game/Audio/CC0/D_Close_00"), 1.f, &Where, .82f);
        Play(*FString::Printf(TEXT("%sS_Beat_StingLow3"), Stings), .75f);
        Fade(0.f, 1.f, .05f);
    }
    if (Once(T, ResetCut)) Cut();
    // The chapter card is drawn on the dark by the run widget as the new chapter starts.
    auto* Chapters = UTallowChapterSubsystem::GetActive(this);
    const bool bOtherBeat = bCutDone && Chapters && Chapters->GetActiveBeat() != B::None;
    if (bOtherBeat) { bHoldingBlack = false; Finish(); return; }   // the final chapter's intro owns the dark now
    if (Once(T, 4.4f)) Fade(1.f, 0.f, 2.f);
    if (T >= ResetEnd) Finish();
}
void UTallowBeatPresenter::Cut()
{
    if (bCutDone) return;
    auto* Chapters = UTallowChapterSubsystem::GetActive(this);
    UE_LOG(LogTemp, Display, TEXT("TALLOW_BEATS cut beat=%d serial=%d t=%.2f"), int32(Beat), Serial, float(GetWorld()->GetTimeSeconds() - StartedAt));
    bCutDone = true;
    if (!Chapters) return;
    TGuardValue<bool> InCut(bInCut, true);
    Chapters->NotifyBeatCut(Beat, Serial);
}
void UTallowBeatPresenter::Finish()
{
    const auto Done = Beat; const int32 DoneSerial = Serial;
    auto* Chapters = UTallowChapterSubsystem::GetActive(this);
    if (bInputReleased)
        if (auto* PC = UGameplayStatics::GetPlayerController(this, 0)) { PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true); }
    bInputReleased = false;
    Clear();
    UE_LOG(LogTemp, Display, TEXT("TALLOW_BEATS finished beat=%d serial=%d"), int32(Done), DoneSerial);
    // Finishing can request the next beat at once (restraint -> release); the stage is already clear for it.
    // After the reset's cut the round is already new and this is a quiet no-op.
    if (Chapters) Chapters->NotifyBeatFinished(Done, DoneSerial);
}
void UTallowBeatPresenter::Abort()
{
    UE_LOG(LogTemp, Display, TEXT("TALLOW_BEATS released beat=%d serial=%d"), int32(Beat), Serial);
    const auto* Chapters = UTallowChapterSubsystem::GetActive(this);
    const bool bCaught = Chapters && Chapters->GetPhase() == ETallowChapterPhase::Caught;
    if (bHoldingBlack && !bCaught) Fade(1.f, 0.f, .5f);
    bInputReleased = false;   // the subsystem already gave input back
    Clear();
}
void UTallowBeatPresenter::Clear()
{
    if (Lids)
        if (auto* Player = GetPlayer()) Player->GetFirstPersonCameraComponent()->RemoveBlendable(Lids);
    for (USoundMix* Mix : Pushed) if (!bPausedMixes) UGameplayStatics::PopSoundMixModifier(this, Mix);
    Pushed.Reset(); bPausedMixes = false;
    for (UAudioComponent* Audio : Held) if (Audio) Audio->Stop();
    Held.Reset();
    SurgeLights(0.f);
    for (const FSurged& Light : Surged) if (Light.Light.IsValid()) { Light.Light->SetIntensity(Light.Intensity); Light.Light->SetVisibility(Light.bVisible); }
    Surged.Reset();
    if (Spark) { Spark->DestroyComponent(); Spark = nullptr; }
    Beat = B::None; bCutDone = false; bHoldingBlack = false;
}
void UTallowBeatPresenter::SetLids(float Close, float Tremble, float LowerRate, float Dark, float Haze)
{
    if (!Lids) return;
    // The lower lid closes with the upper one but opens behind it, at its own slower rate.
    const double Now = GetWorld()->GetTimeSeconds();
    LowerLid = Close >= LowerLid ? Close : FMath::Max(Close, LowerLid - float(Now - LidsAt) * LowerRate); LidsAt = Now;
    Lids->SetScalarParameterValue(TEXT("Close"), Close);
    Lids->SetScalarParameterValue(TEXT("Lower"), LowerLid);
    Lids->SetScalarParameterValue(TEXT("Dark"), Dark);
    Lids->SetScalarParameterValue(TEXT("Haze"), Haze);
    Lids->SetScalarParameterValue(TEXT("Tremble"), Tremble);
}
void UTallowBeatPresenter::SetMix(USoundMix* Mix, bool bOn)
{
    if (!Mix || bOn == Pushed.Contains(Mix)) return;
    if (bOn) { Pushed.Add(Mix); UGameplayStatics::PushSoundMixModifier(this, Mix); }
    else { Pushed.Remove(Mix); UGameplayStatics::PopSoundMixModifier(this, Mix); }
}
void UTallowBeatPresenter::SurgeLights(float Level)
{
    if (Spark) Spark->SetIntensity(Level * 90.f);
    for (const FSurged& Light : Surged)
    {
        if (!Light.Light.IsValid()) continue;
        const float Peak = Light.Light->IntensityUnits == ELightUnits::Candelas ? 10.f : Light.Light->IntensityUnits == ELightUnits::Lumens ? 125.f : 900.f;
        Light.Light->SetVisibility(Level > 0.f || Light.bVisible);
        Light.Light->SetIntensity(Level > 0.f ? Level * Peak : Light.Intensity);
    }
}
void UTallowBeatPresenter::Fade(float From, float To, float Seconds)
{
    bHoldingBlack = To > .5f;
    if (auto* PC = UGameplayStatics::GetPlayerController(this, 0); PC && PC->PlayerCameraManager)
        PC->PlayerCameraManager->StartCameraFade(From, To, Seconds, FLinearColor::Black, false, To > .5f);
}
USoundBase* UTallowBeatPresenter::Sound(const TCHAR* Path)
{
    if (const TObjectPtr<USoundBase>* Found = Sounds.Find(Path)) return *Found;
    const FString Name = FPaths::GetBaseFilename(Path);
    USoundBase* Loaded = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("%s.%s"), Path, *Name));
    if (!Loaded) UE_LOG(LogTemp, Warning, TEXT("TALLOW_BEATS missing sound %s"), Path);
    Sounds.Add(Path, Loaded);
    return Loaded;
}
void UTallowBeatPresenter::Play(const TCHAR* Path, float Volume, const FVector* Where, float Pitch)
{
    USoundBase* Cue = Sound(Path);
    if (!Cue) return;
    if (!Falloff)
    {
        Falloff = NewObject<USoundAttenuation>(this);
        Falloff->Attenuation.bAttenuate = true; Falloff->Attenuation.bSpatialize = true;
        Falloff->Attenuation.AttenuationShapeExtents = FVector(200.f); Falloff->Attenuation.FalloffDistance = 2600.f;
    }
    if (Where) UGameplayStatics::PlaySoundAtLocation(this, Cue, *Where, Volume, Pitch, 0.f, Falloff);
    else UGameplayStatics::PlaySound2D(this, Cue, Volume, Pitch);
}
UAudioComponent* UTallowBeatPresenter::Hold(const TCHAR* Path, float Volume, const FVector* Where)
{
    USoundBase* Cue = Sound(Path);
    if (!Cue) return nullptr;
    UAudioComponent* Audio = Where ? UGameplayStatics::SpawnSoundAtLocation(this, Cue, *Where, FRotator::ZeroRotator, Volume) : UGameplayStatics::SpawnSound2D(this, Cue, Volume);
    if (Audio) Held.Add(Audio);
    return Audio;
}
#if !UE_BUILD_SHIPPING
void UTallowBeatPresenter::Rehearse()
{
    if (RehearseStep < 0) return;
    auto* Chapters = UTallowChapterSubsystem::GetActive(this);
    auto* Player = GetPlayer();
    if (!Chapters || !Player) return;
    using P = ETallowChapterPhase; using E = ETallowChapterEvent;
    const double Now = GetWorld()->GetTimeSeconds();
    const P Phase = Chapters->GetPhase();
    const bool bQuiet = Chapters->GetActiveBeat() == B::None;
    auto Step = [this, Now](int32 Next) { RehearseStep = Next; RehearseAt = Now; UE_LOG(LogTemp, Display, TEXT("TALLOW_BEATS rehearse step=%d"), Next); };
    if (WordsChapter >= 0)
    {
        // -TallowBeatsWords: each hunt chapter's writing in turn, seen from where its recording is heard.
        if (RehearseStep == 0 && Beat == B::None && (WordsChapter > 0 || (bQuiet && Now - RehearseAt > 3.0)))
        {
            // Straight on from the last writing; the transformation's fade it started is dropped with the jump.
            if (auto* PC = UGameplayStatics::GetPlayerController(this, 0); PC && PC->PlayerCameraManager) PC->PlayerCameraManager->StopCameraFade();
            Chapters->JumpToChapter(WordsChapter, P::Exploration);
            Chapters->NotifyEvent(E::StoryStarted); Chapters->NotifyEvent(E::StoryClueDone); Step(1);
        }
        else if (RehearseStep == 1 && Beat == B::None)
        {
            if (++WordsChapter > 2) { RehearseStep = -1; FPlatformMisc::RequestExit(false, TEXT("TallowBeatsWords")); }
            else Step(0);
        }
        return;
    }
    switch (RehearseStep)
    {
    case 0:   // the first chapter is up: out of the chair, the clue heard
        if ((Phase == P::InitialRestraint || Phase == P::Exploration) && bQuiet)
        {
            if (RehearseAt <= 0.0) { RehearseAt = Now; return; }
            if (Now - RehearseAt < 3.0) return;
            Chapters->NotifyEvent(E::ChairEscaped); Chapters->NotifyEvent(E::StoryStarted); Chapters->NotifyEvent(E::StoryClueDone);
            Step(1);
        }
        break;
    case 1:   // writing done; walk her into the chair room if the transformation waits for it
        if ((Phase == P::Exploration || Phase == P::StoryInteraction) && bQuiet && Now - RehearseAt > 9.0)
            for (TActorIterator<ATallowChairSpot> It(GetWorld()); It; ++It)
                if (It->GetRoom() == Chapters->GetLayout().ChairRoom)
                { Player->TeleportTo(It->GetActorLocation() + It->GetActorForwardVector() * 120.f + FVector(0, 0, 95.f), It->GetActorRotation(), false, true); Step(2); break; }
        if (Phase == P::Transformation) Step(2);
        break;
    case 2:   // tied, released, then straight to the box with the battery
        if (Phase == P::HuntPreparation && bQuiet)
        {
            Step(3);
        }
        break;
    case 3:
        if (Now - RehearseAt < 2.0) return;
        for (TActorIterator<ATallowChairEscape> It(GetWorld()); It; ++It) It->ReleasePlayerQuietly();
        Chapters->NotifyEvent(E::ChairEscaped);
        if (const AActor* Box = UGameplayStatics::GetActorOfClass(this, ATallowFuseBox::StaticClass()))
        {
            const FVector Foot = Box->GetActorLocation() + Box->GetActorForwardVector() * 190.f;
            Player->TeleportTo(FVector(Foot.X, Foot.Y, Box->GetActorLocation().Z), FRotator::ZeroRotator, false, true);
            Player->GetController()->SetControlRotation((Box->GetActorLocation() - Player->GetFirstPersonCameraComponent()->GetComponentLocation()).Rotation());
        }
        Step(4);
        break;
    case 4:
        if (Now - RehearseAt < .6f) return;
        Chapters->NotifyEvent(E::BatteryCollected); Chapters->NotifyEvent(E::FuseActivated);
        Step(5);
        break;
    case 5:   // the next chapter has started and its card has had time
        if (Chapters->GetChapterIndex() == 1 && bQuiet && Beat == B::None && Now - RehearseAt > 15.0)
        { UE_LOG(LogTemp, Display, TEXT("TALLOW_BEATS rehearse done frames=%d"), Frames); RehearseStep = -1; FPlatformMisc::RequestExit(false, TEXT("TallowBeatsRehearse")); }
        break;
    default: break;
    }
}
void UTallowBeatPresenter::Film()
{
    if (FilmDir.IsEmpty()) return;
    const auto* Chapters = UTallowChapterSubsystem::GetActive(this);
    const double Real = FPlatformTime::Seconds();
    if (Beat != B::None || (Chapters && Chapters->GetActiveBeat() != B::None)) FilmUntil = Real + 2.0;
    if (Real > FilmUntil || Real < NextFrame) return;
    NextFrame = Real + .25;
    const FString Name = Beat != B::None ? StaticEnum<ETallowChapterBeat>()->GetNameStringByValue(int64(Beat)) : Chapters && Chapters->GetActiveBeat() != B::None ? TEXT("Fallback") : TEXT("After");
    const float T = Beat != B::None ? float(GetWorld()->GetTimeSeconds() - StartedAt) : 0.f;
    FScreenshotRequest::RequestScreenshot(FPaths::Combine(FilmDir, FString::Printf(TEXT("%04d_%s_%05.2f.png"), Frames++, *Name, T)), true, false);
}
#endif
