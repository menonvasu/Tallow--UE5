#include "Environment/TallowHousePresence.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "Player/TallowCharacter.h"
#include "Sound/SoundAttenuation.h"
#include "Environment/TallowCandlePickup.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "AI/Hunter/TallowHunterDirector.h"
#include "Environment/TallowHouseBoardClearance.h"
#include "Tasks/TallowPlank.h"
#include "Scare/TallowScareDirector.h"

namespace
{
    // Keep the flame dim, but let its pool reveal the floor and doorway ahead.
    constexpr float HeldLitRadiusCm = 560.f;
    constexpr float HeldCuppedRadiusCm = 180.f;
}

void UTallowHousePresenceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    InWorld.GetTimerManager().SetTimer(DarknessHandle, this, &UTallowHousePresenceSubsystem::ApplyHouseDarkness, .35f, false);
}

void UTallowHousePresenceSubsystem::ApplyHouseDarkness()
{
    if (!UGameplayStatics::GetActorOfClass(this, ATallowHousePresence::StaticClass())) return;
    AActor* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    for (TActorIterator<ATallowPlank> It(GetWorld()); It; ++It)
        if (!It->FindComponentByClass<UTallowHouseBoardClearance>())
        {
            auto* Clearance = NewObject<UTallowHouseBoardClearance>(*It);
            It->AddInstanceComponent(Clearance); Clearance->RegisterComponent();
        }
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (It->IsA<ATallowHousePresence>()) continue;
        // Authored candle/window sources retain their saved lighting. Electrical
        // fixtures still obey the powerless-house rule, even if tagged in error.
        // This affects rendering only; Hunter perception remains candle-driven.
        if (It->ActorHasTag(TEXT("Tallow.AuthoredLight")) && !It->ActorHasTag(TEXT("Tallow.PowerLight"))) continue;
        for (ULightComponent* Light : TInlineComponentArray<ULightComponent*>(*It))
        {
            Light->SetIndirectLightingIntensity(0.f); Light->SetVolumetricScatteringIntensity(0.f);
            if (*It == Player)
            {
                if (UPointLightComponent* Point = Cast<UPointLightComponent>(Light))
                {
                    if (Point->GetName().Contains(TEXT("Candle")))
                    {
                        Point->SetAttenuationRadius(HeldLitRadiusCm);
                        Point->SetIndirectLightingIntensity(.65f);
                        Point->SetSoftSourceRadius(3.f);
                        HeldLight = Point;
                    }
                    else if (Point->GetName().Contains(TEXT("NightEyes"))) Point->SetIntensity(0.f);
                }
            }
            else if (It->IsA<ATallowCandlePickup>())
            {
                Light->SetIntensity(4.f);
                if (UPointLightComponent* Point = Cast<UPointLightComponent>(Light)) Point->SetAttenuationRadius(95.f);
            }
            else Light->SetIntensity(0.f);
        }
        for (USkyLightComponent* Sky : TInlineComponentArray<USkyLightComponent*>(*It)) Sky->SetIntensity(0.f);
    }
    if (Player) HeldCandle = Player->FindComponentByClass<UTallowCandleComponent>();
    if (HeldCandle.IsValid())
    {
        // These existing EditDefaultsOnly tunables are reapplied by the candle's
        // state switch after its delegate fires; configure them at their source.
        if (FFloatProperty* Radius = FindFProperty<FFloatProperty>(HeldCandle->GetClass(), TEXT("LitLightRadius")))
            Radius->SetPropertyValue_InContainer(HeldCandle.Get(), HeldLitRadiusCm);
        if (FFloatProperty* Radius = FindFProperty<FFloatProperty>(HeldCandle->GetClass(), TEXT("CuppedLightRadius")))
            Radius->SetPropertyValue_InContainer(HeldCandle.Get(), HeldCuppedRadiusCm);
        HeldCandle->OnCandleStateChanged.AddUniqueDynamic(this, &UTallowHousePresenceSubsystem::OnCandleLightState);
        OnCandleLightState(HeldCandle->GetCandleState());
    }
}

void UTallowHousePresenceSubsystem::OnCandleLightState(ECandleState State)
{
    if (HeldLight.IsValid())
    {
        HeldLight->SetAttenuationRadius(State == ECandleState::Cupped ? HeldCuppedRadiusCm : HeldLitRadiusCm);
        HeldLight->SetVisibility(State != ECandleState::Out && HeldCandle.IsValid() && HeldCandle->IsAcquired());
    }
}

bool UTallowHousePresenceSubsystem::ClaimEvent(float QuietSeconds)
{
    if (!GetWorld() || GetWorld()->GetTimeSeconds() < QuietUntil) return false;
    QuietUntil = GetWorld()->GetTimeSeconds() + FMath::Max(QuietSeconds, 20.f);
    ++EventCount;
    return true;
}

ATallowHousePresence::ATallowHousePresence()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .04f;
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    SetRootComponent(Body);
    Body->SetMobility(EComponentMobility::Movable);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetCanEverAffectNavigation(false);
    MovingProp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MovingProp"));
    MovingProp->SetupAttachment(Body);
    MovingProp->SetMobility(EComponentMobility::Movable);
    MovingProp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MovingProp->SetCanEverAffectNavigation(false);
    Practical = CreateDefaultSubobject<UPointLightComponent>(TEXT("Practical"));
    Practical->SetupAttachment(Body);
    Practical->SetMobility(EComponentMobility::Movable);
    Practical->IntensityUnits = ELightUnits::Lumens;
    Practical->SetIntensity(0.f);
    Practical->SetLightColor(FLinearColor(1.f, .57f, .24f));
    Practical->SetAttenuationRadius(100.f);
    Practical->SetIndirectLightingIntensity(0.f);
    Practical->SetSourceRadius(3.f);
    Practical->CastShadows = true;
    EventAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("EventAudio"));
    EventAudio->SetupAttachment(Body);
    EventAudio->bAutoActivate = false;
    EventAudio->bOverrideAttenuation = true;
    EventAudio->AttenuationOverrides.bAttenuate = true;
    EventAudio->AttenuationOverrides.AttenuationShapeExtents = FVector(70.f);
    EventAudio->AttenuationOverrides.FalloffDistance = 850.f;
    EventAudio->SetVolumeMultiplier(.55f);
}

void ATallowHousePresence::BeginPlay()
{
    Super::BeginPlay();
    Random.Initialize(int32(GetTypeHash(GetFName())) ^ FMath::Rand());
    Phase = Random.FRandRange(0.f, 200.f);
    NextCheck = GetWorld()->GetTimeSeconds() + Random.FRandRange(InitialDelayMin, InitialDelayMax);
    Rest = MovingProp->GetRelativeTransform();
    ChapterRest = Rest;
    EventAudio->SetSound(EventSound);
    Practical->SetVisibility(bCandlePractical);
}

bool ATallowHousePresence::PlayerCanExperienceEvent() const
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || !PC->GetPawn()) return false;
    if (const ATallowCharacter* Player = Cast<ATallowCharacter>(PC->GetPawn()))
        if (Player->IsTiedToChair()) return false;
    FVector Eye; FRotator Rotation;
    PC->GetPlayerViewPoint(Eye, Rotation);
    const FVector Target = MovingProp->GetComponentLocation();
    const float Distance = FVector::Dist(Eye, Target);
    if (Distance < MinimumDistance || Distance > MaximumDistance || FMath::Abs(Eye.Z - Target.Z) > 220.f) return false;
    if (!bRequirePlayerLooking) return true;
    if (FVector::DotProduct(Rotation.Vector(), (Target - Eye).GetSafeNormal()) < .60f) return false;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HousePresenceSight), false, this);
    Params.AddIgnoredActor(PC->GetPawn());
    return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Params);
}

void ATallowHousePresence::ResetForChapter()
{
    EventsTriggered = 0; SelfScheduledEvents = 0; bActive = false; EventAge = 0.f; NextRing = 0.f;
    Rest = ChapterRest; MovingProp->SetRelativeTransform(Rest); EventAudio->Stop(); bChapterSilent = false;
    NextCheck = GetWorld()->GetTimeSeconds() + Random.FRandRange(InitialDelayMin, InitialDelayMax);
}

bool ATallowHousePresence::TryTriggerEvent()
{
    if (bCandlePractical || bActive || EventsTriggered >= MaximumEvents || !PlayerCanExperienceEvent()) return false;
    UTallowHousePresenceSubsystem* Presence = GetWorld()->GetSubsystem<UTallowHousePresenceSubsystem>();
    const auto* Scares = UTallowScareDirector::Get(this);
    if ((!Scares || !Scares->OwnsScheduling()) && (!Presence || !Presence->ClaimEvent(QuietSeconds + Random.FRandRange(0.f, 25.f)))) return false;
    bActive = true;
    EventAge = 0.f;
    NextRing = 0.f;
    ++EventsTriggered;
    if (!bChapterSilent && !bTelephone && EventSound) EventAudio->Play();
    if (bAlertsMim)
        if (UTallowNoiseSubsystem* Noise = UTallowNoiseSubsystem::Get(this))
            Noise->EmitCustomNoise(GetActorLocation(), this, NoiseLoudness, NoiseRange, true);
    return true;
}

int32 ATallowHousePresence::GetReportedNoiseCount() const
{
    int32 Count = 0;
    if (const UTallowNoiseSubsystem* Noise = UTallowNoiseSubsystem::Get(this))
        for (const FTallowNoiseEvent& Event : Noise->GetRecentNoises(5.f))
            if (Event.Instigator == this && Event.bAlertsAI) ++Count;
    return Count;
}

void ATallowHousePresence::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float Now = GetWorld()->GetTimeSeconds();
    if (bCandlePractical)
    {
        const float Flicker = 1.f + .07f * FMath::Sin(Now * 7.3f + Phase) + .04f * FMath::Sin(Now * 13.1f + Phase * .71f);
        Practical->SetIntensity(PracticalLumens * Flicker);
        return;
    }
    const auto* Scares = UTallowScareDirector::Get(this);
    if ((!Scares || !Scares->OwnsScheduling()) && !bActive && Now >= NextCheck)
    {
        NextCheck = Now + Random.FRandRange(5.f, 13.f);
        // Random house scares are build texture now, paced by the hunter director: they warn that the
        // evening is rising, never shout over a chase or puncture earned relief. Authored callers of
        // TryTriggerEvent keep their own timing. The telephone is a major event and waits for power.
        const UTallowHunterDirector* Pacing = GetWorld()->GetSubsystem<UTallowHunterDirector>();
        if (Random.FRand() < .42f && (!Pacing || Pacing->AllowsHouseEvent(bTelephone)) && TryTriggerEvent()) ++SelfScheduledEvents;
    }
    if (!bActive) return;
    EventAge += DeltaSeconds;
    const float T = FMath::Clamp(EventAge / FMath::Max(Duration, .1f), 0.f, 1.f);
    const float Envelope = bStayDisplaced ? FMath::SmoothStep(0.f, 1.f, T) : FMath::Sin(PI * T) * FMath::Exp(-1.6f * T);
    const float Oscillation = bStayDisplaced ? Envelope : Envelope * FMath::Sin(EventAge * Frequency * 2.f * PI);
    MovingProp->SetRelativeLocation(Rest.GetLocation() + Movement * Oscillation);
    MovingProp->SetRelativeRotation(Rest.Rotator() + Swing * Oscillation);
    if (bTelephone && EventAge >= NextRing && T < .8f)
    {
        if (!bChapterSilent && EventSound) EventAudio->Play();
        NextRing += .65f;
    }
    if (T >= 1.f)
    {
        bActive = false;
        if (bStayDisplaced) Rest = MovingProp->GetRelativeTransform();
        else MovingProp->SetRelativeTransform(Rest);
        NextCheck = Now + Random.FRandRange(80.f, 160.f);
    }
}
