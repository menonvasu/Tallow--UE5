#include "TallowCandlePickup.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Candle/TallowCandleComponent.h"

ATallowCandlePickup::ATallowCandlePickup()
{
	PrimaryActorTick.bCanEverTick = false;
	
	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CandlePickup Mesh"));
	RootComponent = PickupMesh;

	TableLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("TableLight Component"));
	TableLightComponent->SetupAttachment(PickupMesh);
	TableLightComponent->SetAttenuationRadius(800.0f);
}

void ATallowCandlePickup::OnInteract_Implementation(AActor* Interactor)
{
	if (!Interactor) return;

	if (UTallowCandleComponent* Candle = Interactor->FindComponentByClass<UTallowCandleComponent>())
	{
		Candle->AcquireCandle(true);
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
	}
}

FText ATallowCandlePickup::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
	if (!Interactor) return FText::GetEmpty();
	
	return FText::FromString("PICKUP CANDLE");
}

void ATallowCandlePickup::ResetPickup(AActor* Interactor)
{
	if (!Interactor) return;

	if (UTallowCandleComponent* Candle = Interactor->FindComponentByClass<UTallowCandleComponent>())
	{
		Candle->ResetCandle();
		SetActorHiddenInGame(false);
		SetActorEnableCollision(true);
	}
}

void ATallowCandlePickup::SetTakenForChapter(bool bTaken)
{
    SetActorHiddenInGame(bTaken); SetActorEnableCollision(!bTaken);
}
