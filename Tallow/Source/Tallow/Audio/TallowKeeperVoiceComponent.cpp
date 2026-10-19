#include "Audio/TallowKeeperVoiceComponent.h"
#include "AI/TallowCreature.h"
#include "Animation/TallowCreatureAnimationComponent.h"
#include "TimerManager.h"
#include "GameMode/TallowGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"

UTallowKeeperVoiceComponent::UTallowKeeperVoiceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Breath only: the groans read as a roaring monster, and the Keeper is a silent collector you mostly hear up close.
	for (const TCHAR* Name : {TEXT("S_Keeper_Breath_00"), TEXT("S_Keeper_Breath_01"), TEXT("S_Keeper_Breath_02"), TEXT("S_Keeper_Breath_03")})
	{
		Vocalisations.Add(FSoftObjectPath(FString::Printf(TEXT("/Game/Audio/Keeper/%s.%s"), Name, Name)));
	}
	CaptureRoar = FSoftObjectPath(TEXT("/Game/Audio/Keeper/S_Capture_Roar.S_Capture_Roar"));
	CaptureScream = FSoftObjectPath(TEXT("/Game/Audio/Keeper/S_Capture_Scream.S_Capture_Scream"));
	// Interim CC0 cues: a sniff and a porcelain tick while it hunts, the mask splitting under the capture roar.
	HuntingCues = { FSoftObjectPath(TEXT("/Game/Audio/CC0/K_Sniff.K_Sniff")), FSoftObjectPath(TEXT("/Game/Audio/CC0/K_Click.K_Click")) };
	MaskCrack = FSoftObjectPath(TEXT("/Game/Audio/CC0/K_Mask_Crack.K_Mask_Crack"));
}

void UTallowKeeperVoiceComponent::BeginPlay()
{
	Super::BeginPlay();
	for (const FSoftObjectPath& Path : Vocalisations)
	{
		if (USoundBase* Sound = Cast<USoundBase>(Path.TryLoad())) Loaded.Add(Sound);
	}
	for (const FSoftObjectPath& Path : HuntingCues)
	{
		if (USoundBase* Sound = Cast<USoundBase>(Path.TryLoad())) LoadedHunting.Add(Sound);
	}
	Attenuation = NewObject<USoundAttenuation>(this);
	Attenuation->Attenuation.bAttenuate = true;
	Attenuation->Attenuation.bSpatialize = true;
	Attenuation->Attenuation.AttenuationShapeExtents = FVector(150.f);
	Attenuation->Attenuation.FalloffDistance = HearingDistance;
	NextVoice = GetWorld()->GetTimeSeconds() + FMath::FRandRange(10.f, 20.f);
	if (ATallowGameMode* Mode = Cast<ATallowGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		Mode->OnCaught.AddUniqueDynamic(this, &UTallowKeeperVoiceComponent::PlayCapture);
	}
}

void UTallowKeeperVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ATallowGameMode* Mode = Cast<ATallowGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		Mode->OnCaught.RemoveDynamic(this, &UTallowKeeperVoiceComponent::PlayCapture);
	}
	Super::EndPlay(EndPlayReason);
}

void UTallowKeeperVoiceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextVoice || Loaded.IsEmpty()) return;

	// Rare and quiet while it wanders, closer together while it hunts for you, ragged only in a chase.
	const ATallowCreature* Keeper = Cast<ATallowCreature>(GetOwner());
	const ETallowCreatureState State = Keeper ? Keeper->CurrentState : ETallowCreatureState::Patrol;
	const bool bChasing = State == ETallowCreatureState::Chasing;
	const bool bHunting = !bChasing && State != ETallowCreatureState::Patrol && State != ETallowCreatureState::SuspiciousIdle;
	int32 Index = FMath::RandRange(0, Loaded.Num() - 1);
	if (Index == LastIndex) Index = (Index + 1) % Loaded.Num();
	LastIndex = Index;
	USoundBase* Sound = Loaded[Index];
	if (bHunting && !LoadedHunting.IsEmpty() && FMath::FRand() < .35f) Sound = LoadedHunting[FMath::RandRange(0, LoadedHunting.Num() - 1)];
	// Heard through the porcelain: the mask takes the top off every breath.
	if (UAudioComponent* Voice = UGameplayStatics::SpawnSoundAttached(Sound, GetOwner()->GetRootComponent(), NAME_None, FVector(0, 0, 200), EAttachLocation::KeepRelativeOffset,
		false, bChasing ? .8f : bHunting ? .45f : .3f, FMath::FRandRange(0.85f, 1.f), 0.f, Attenuation))
	{
		Voice->SetLowPassFilterEnabled(true);
		Voice->SetLowPassFilterFrequency(2200.f);
	}
	NextVoice = Now + (bChasing ? FMath::FRandRange(2.5f, 4.5f) : bHunting ? FMath::FRandRange(7.f, 12.f) : FMath::FRandRange(16.f, 30.f));
}

void UTallowKeeperVoiceComponent::PlayCapture()
{
	// The mask splits and it shrieks on the jumpscare's strike frame; her scream follows a beat after.
	const auto* Anim = GetOwner()->FindComponentByClass<UTallowCreatureAnimationComponent>();
	const float Strike = Anim ? Anim->GetCaptureStrikeTime() : 0.f;
	auto Strike_ = [this]
	{
		if (USoundBase* Crack = Cast<USoundBase>(MaskCrack.TryLoad())) UGameplayStatics::SpawnSoundAttached(Crack, GetOwner()->GetRootComponent());
		if (USoundBase* Roar = Cast<USoundBase>(CaptureRoar.TryLoad())) UGameplayStatics::SpawnSoundAttached(Roar, GetOwner()->GetRootComponent());
	};
	auto Scream_ = [this] { if (USoundBase* Scream = Cast<USoundBase>(CaptureScream.TryLoad())) UGameplayStatics::PlaySound2D(this, Scream, 0.8f); };
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	if (Strike > .05f) Timers.SetTimer(StrikeHandle, FTimerDelegate::CreateWeakLambda(this, Strike_), Strike, false); else Strike_();
	Timers.SetTimer(ScreamHandle, FTimerDelegate::CreateWeakLambda(this, Scream_), Strike + .12f, false);
}
