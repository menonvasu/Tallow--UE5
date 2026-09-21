#include "Environment/TallowDraftVolume.h"
#include "Components/BoxComponent.h"
#include "Candle/TallowCandleComponent.h"

ATallowDraftVolume::ATallowDraftVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	RootComponent = TriggerBox;
}

void ATallowDraftVolume::BeginPlay()
{
	Super::BeginPlay();

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATallowDraftVolume::HandleBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ATallowDraftVolume::HandleEndOverlap);
}

void ATallowDraftVolume::HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool FromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this) return;

	if (UTallowCandleComponent* Candle = OtherActor->FindComponentByClass<UTallowCandleComponent>())
	{
		Candle->EnterDraft();
	}
}

void ATallowDraftVolume::HandleEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor || OtherActor == this) return;

	if (UTallowCandleComponent* Candle = OtherActor->FindComponentByClass<UTallowCandleComponent>())
	{
		Candle->ExitDraft();
	}
}