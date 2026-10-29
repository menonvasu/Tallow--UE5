#include "Environment/TallowMatchbox.h"

#include "Candle/TallowCandleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/TallowCharacter.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "TallowMatchbox"

ATallowMatchbox::ATallowMatchbox()
{
	PrimaryActorTick.bCanEverTick = false;
	Grab = CreateDefaultSubobject<UBoxComponent>(TEXT("Grab"));
	RootComponent = Grab;
	Grab->SetBoxExtent(FVector(12.f, 9.f, 7.f));
	Grab->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Grab->SetCollisionResponseToAllChannels(ECR_Ignore);
	Grab->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Box = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Box"));
	Box->SetupAttachment(Grab);
	Box->SetRelativeLocation(FVector(0.f, 0.f, -7.f));   // the actor sits 7 cm above the surface, the box on it
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetCastShadow(true);
}

bool ATallowMatchbox::CanInteract_Implementation(AActor* Interactor) const
{
	return Cast<ATallowCharacter>(Interactor) != nullptr;
}

FText ATallowMatchbox::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
	return LOCTEXT("Take", "TAKE MATCHES");
}

void ATallowMatchbox::OnInteract_Implementation(AActor* Interactor)
{
	const auto* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player || !Player->GetCandleComponent()) return;
	Player->GetCandleComponent()->AddMatches(Matches);
	if (auto* Rattle = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/CC0/P_Cup_Rustle_00.P_Cup_Rustle_00")))
		UGameplayStatics::PlaySoundAtLocation(this, Rattle, GetActorLocation(), .3f, 1.4f);
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void ATallowMatchbox::ResetMatchbox()
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
}

#undef LOCTEXT_NAMESPACE
