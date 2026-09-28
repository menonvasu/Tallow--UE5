#include "Audio/TallowKeeperVoiceComponent.h"
#include "AI/TallowCreature.h"
#include "GameMode/TallowGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"

UTallowKeeperVoiceComponent::UTallowKeeperVoiceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	for (const TCHAR* Name : {TEXT("S_Keeper_Groan_00"), TEXT("S_Keeper_Groan_01"), TEXT("S_Keeper_Groan_02"), TEXT("S_Keeper_Groan_03"),
		TEXT("S_Keeper_Groan_04"), TEXT("S_Keeper_Groan_06"), TEXT("S_Keeper_Breath_00"), TEXT("S_Keeper_Breath_01"), TEXT("S_Keeper_Breath_02"), TEXT("S_Keeper_Breath_03")})
	{
		Vocalisations.Add(FSoftObjectPath(FString::Printf(TEXT("/Game/Audio/Keeper/%s.%s"), Name, Name)));
	}
	CaptureRoar = FSoftObjectPath(TEXT("/Game/Audio/Keeper/S_Capture_Roar.S_Capture_Roar"));
	CaptureScream = FSoftObjectPath(TEXT("/Game/Audio/Keeper/S_Capture_Scream.S_Capture_Scream"));
}

void UTallowKeeperVoiceComponent::BeginPlay()
{
	Super::BeginPlay();
	for (const FSoftObjectPath& Path : Vocalisations)
	{
		if (USoundBase* Sound = Cast<USoundBase>(Path.TryLoad())) Loaded.Add(Sound);
	}
	Attenuation = NewObject<USoundAttenuation>(this);
	Attenuation->Attenuation.bAttenuate = true;
	Attenuation->Attenuation.bSpatialize = true;
	Attenuation->Attenuation.AttenuationShapeExtents = FVector(150.f);
	Attenuation->Attenuation.FalloffDistance = HearingDistance;
	NextVoice = GetWorld()->GetTimeSeconds() + FMath::FRandRange(3.f, 7.f);
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

	const ATallowCreature* Keeper = Cast<ATallowCreature>(GetOwner());
	const bool bChasing = Keeper && Keeper->CurrentState == ETallowCreatureState::Chasing;
	int32 Index = FMath::RandRange(0, Loaded.Num() - 1);
	if (Index == LastIndex) Index = (Index + 1) % Loaded.Num();
	LastIndex = Index;
	UGameplayStatics::SpawnSoundAttached(Loaded[Index], GetOwner()->GetRootComponent(), NAME_None, FVector(0, 0, 120), EAttachLocation::KeepRelativeOffset,
		false, bChasing ? 1.f : 0.7f, FMath::FRandRange(0.85f, 1.05f), 0.f, Attenuation);
	NextVoice = Now + (bChasing ? FMath::FRandRange(2.5f, 5.f) : FMath::FRandRange(6.f, 12.f));
}

void UTallowKeeperVoiceComponent::PlayCapture()
{
	if (USoundBase* Roar = Cast<USoundBase>(CaptureRoar.TryLoad()))
	{
		UGameplayStatics::SpawnSoundAttached(Roar, GetOwner()->GetRootComponent());
	}
	if (USoundBase* Scream = Cast<USoundBase>(CaptureScream.TryLoad()))
	{
		UGameplayStatics::PlaySound2D(this, Scream, 0.8f);
	}
}
