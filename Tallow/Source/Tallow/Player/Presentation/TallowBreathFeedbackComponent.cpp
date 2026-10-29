#include "Player/Presentation/TallowBreathFeedbackComponent.h"

#include "AI/TallowCreature.h"
#include "Components/StaticMeshComponent.h"
#include "GameMode/TallowGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Player/Presentation/TallowPlayerPresentationComponent.h"
#include "Player/TallowCharacter.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

UTallowBreathFeedbackComponent::UTallowBreathFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTallowBreathFeedbackComponent::Setup(ATallowCharacter* InPlayer)
{
	Player = InPlayer;
	if (!InPlayer) return;
	InPlayer->OnStill.AddUniqueDynamic(this, &UTallowBreathFeedbackComponent::OnStill);
	InPlayer->OnBreathReleased.AddUniqueDynamic(this, &UTallowBreathFeedbackComponent::OnReleased);
	Mim = UGameplayStatics::GetActorOfClass(this, ATallowCreature::StaticClass());
	// The house goes dull and far away while she holds it: everything but Mim, her own voice and the readings.
	Muffle = NewObject<USoundMix>(this, TEXT("HeldBreathMuffle"));
	Muffle->FadeInTime = .3f; Muffle->FadeOutTime = .6f; Muffle->Duration = -1.f;
	for (const TCHAR* Name : {TEXT("SC_Tallow_Ambience"), TEXT("SC_Tallow_Tension"), TEXT("SC_Tallow_Distractions"),
		TEXT("SC_Tallow_Moments"), TEXT("SC_Tallow_PlayerFoley")})
		if (auto* Class = LoadObject<USoundClass>(nullptr, *FString::Printf(TEXT("/Game/Audio/Mix/%s.%s"), Name, Name)))
		{
			FSoundClassAdjuster Adjuster;
			Adjuster.SoundClassObject = Class;
			Adjuster.VolumeAdjuster = MuffleVolume;
			Adjuster.LowPassFilterFrequency = MuffleHz;
			Muffle->SoundClassEffects.Add(Adjuster);
		}
}

USoundBase* UTallowBreathFeedbackComponent::Sound(const TCHAR* Path)
{
	if (const TObjectPtr<USoundBase>* Found = Sounds.Find(Path)) return *Found;
	USoundBase* Loaded = LoadObject<USoundBase>(nullptr, Path);
	if (!Loaded) UE_LOG(LogTemp, Warning, TEXT("TALLOW_BREATH missing %s"), Path);
	Sounds.Add(Path, Loaded);
	return Loaded;
}

void UTallowBreathFeedbackComponent::Play(USoundBase* Cue, float Volume)
{
	if (Cue) UGameplayStatics::PlaySound2D(this, Cue, Volume, 1.f, 0.f, nullptr, GetOwner(), false);
}

float UTallowBreathFeedbackComponent::MimDistance() const
{
	return Mim.IsValid() && Player.IsValid() ? FVector::Dist(Mim->GetActorLocation(), Player->GetActorLocation()) : 1e7f;
}

void UTallowBreathFeedbackComponent::SetMuffled(bool bOn)
{
	if (bOn == bMuffled || !Muffle) return;
	bMuffled = bOn;
	if (bOn) UGameplayStatics::PushSoundMixModifier(this, Muffle);
	else UGameplayStatics::PopSoundMixModifier(this, Muffle);
}

void UTallowBreathFeedbackComponent::OnStill(bool bStill)
{
	if (!bStill || !Player.IsValid() || Player->IsTiedToChair()) return;
	// The sharp intake, then the world closes in.
	Play(Sound(TEXT("/Game/Audio/VO/VO_Nell_Effort_02.VO_Nell_Effort_02")), IntakeVolume);
	NextBeat = Time + .55f; bStrainSounded = false; NearestDuringHold = MimDistance();
	SetMuffled(true);
	UE_LOG(LogTemp, Display, TEXT("TALLOW_BREATH hold air=%.2f mim=%.0f"), Player->GetHeldAir(), NearestDuringHold);
}

void UTallowBreathFeedbackComponent::OnReleased(bool bForced)
{
	SetMuffled(false);
	if (!Player.IsValid()) return;
	const auto* Mode = GetWorld()->GetAuthGameMode<ATallowGameMode>();
	if (Player->IsTiedToChair() || (Mode && Mode->IsCaptureInProgress())) return;
	const float Distance = MimDistance();
	const float Held = (1.f - Player->GetHeldAir()) * Player->BreathHoldSeconds;
	if (bForced)
		Play(Sound(TEXT("/Game/Audio/VO/VO_Nell_Effort_15.VO_Nell_Effort_15")), GaspVolume);   // one broken breath, gulped
	else if (NearestDuringHold < 700.f && Distance > 850.f)
	{
		// It came close and went on by without hearing her: the relief beat, and Nell's notes take it down.
		Play(Sound(TEXT("/Game/Audio/VO/VO_Nell_Effort_07.VO_Nell_Effort_07")), ExhaleVolume * 1.1f);
		bHeldPast = true; LastPastAt = GetWorld()->GetTimeSeconds();
	}
	else if (Held > 1.2f)
		Play(Sound(TEXT("/Game/Audio/VO/VO_Nell_Effort_08.VO_Nell_Effort_08")), ExhaleVolume * (Distance < 850.f ? .6f : 1.f));
	UE_LOG(LogTemp, Display, TEXT("TALLOW_BREATH release forced=%d held=%.1f nearest=%.0f mim=%.0f beats=%d"),
		bForced ? 1 : 0, Held, NearestDuringHold, Distance, Beats);
}

void UTallowBreathFeedbackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Player.IsValid()) return;
	Time += DeltaTime;
	const bool bHolding = Player->IsHoldingStill();
	Hold = FMath::FInterpConstantTo(Hold, bHolding ? 1.f : 0.f, DeltaTime, bHolding ? 4.f : 2.f);
	if (!bHolding && bMuffled) SetMuffled(false);
	if (!Candle.IsValid())
		for (auto* Mesh : TInlineComponentArray<UStaticMeshComponent*>(GetOwner()))
			if (Mesh->GetName() == TEXT("IntegratedCandle")) { Candle = Mesh; CandleBase = Mesh->GetRelativeLocation(); }
	// Her hands go still with her breath (they still move if she creeps).
	if (auto* Hands = GetOwner()->FindComponentByClass<UTallowPlayerPresentationComponent>())
		Hands->PlaybackRateScale = 1.f - .85f * Hold * FMath::Clamp(1.f - Player->GetVelocity().Size2D() / 60.f, 0.f, 1.f);
	const float Strain = Player->GetHeldStrain();
	// The strain: the candle shakes a little in her fist (a few millimetres, never into the hand).
	if (Candle.IsValid())
	{
		const FVector Shake(FMath::PerlinNoise1D(Time * 23.f), FMath::PerlinNoise1D(Time * 19.f + 5.f), FMath::PerlinNoise1D(Time * 29.f + 9.f));
		Candle->SetRelativeLocation(CandleBase + Shake * .32f * Strain);
	}
	if (!bHolding) return;
	NearestDuringHold = FMath::Min(NearestDuringHold, MimDistance());
	if (Strain > .35f && !bStrainSounded)
	{
		bStrainSounded = true;   // a small sound she can't keep in; only a warning, no noise for the Keeper
		Play(Sound(TEXT("/Game/Audio/VO/VO_Nell_Effort_17.VO_Nell_Effort_17")), StrainVolume);
	}
	// Her heartbeat: soft at first, quicker and louder the longer she holds it (72 to about 130 a minute).
	if (Time >= NextBeat)
	{
		const float Held = FMath::Clamp((1.f - Player->GetHeldAir()), 0.f, 1.f);
		const float Bpm = 72.f + 46.f * FMath::SmoothStep(0.f, 1.f, Held) + 12.f * Strain;
		NextBeat = Time + 60.f / Bpm;
		if (auto* Beat = Sound(TEXT("/Game/FinalPass/sense/S_Sense_Heartbeat.S_Sense_Heartbeat")))
			UGameplayStatics::PlaySound2D(this, Beat, HeartbeatVolume * (.45f + .55f * Held), 1.f + .06f * Strain, 0.f, nullptr, GetOwner(), false);
		++Beats;
	}
}

void UTallowBreathFeedbackComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	SetMuffled(false);
	if (Candle.IsValid()) Candle->SetRelativeLocation(CandleBase);
	Super::EndPlay(Reason);
}
