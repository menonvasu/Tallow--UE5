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
		Destroy();
	}
}