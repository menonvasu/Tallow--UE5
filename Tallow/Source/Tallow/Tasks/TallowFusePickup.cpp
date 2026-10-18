#include "TallowFusePickup.h"
#include "Player/TallowCharacter.h"

// Sets default values
ATallowFusePickup::ATallowFusePickup()
{
	PrimaryActorTick.bCanEverTick = false;
	
	FuseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FuseMesh"));
	RootComponent = FuseMesh; 
	FuseMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	FuseMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ATallowFusePickup::OnInteract_Implementation(AActor* Interactor)
{
	if (!Interactor) { return; }
	ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) { return; }
	
	// if carrying item, ignore
	if (Player->GetCarriedItem() != ECarriedItemType::None) { return; }
	
	// give fuse and broadcast sound/change
	Player->SetCarriedItem(ECarriedItemType::Fuse);
	OnFusePickedUp.Broadcast();
	
	// Deactivate it
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}	

bool ATallowFusePickup::CanInteract_Implementation(AActor* Interactor) const
{
	if (!Interactor) { return false; }
	const ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) { return false; }
	
	// if player is holding item - return cannot interact
	if (Player->GetCarriedItem() != ECarriedItemType::None) 
	{
		return false;
	}
	return true;
}
	
FText ATallowFusePickup::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
	if (!Interactor) { return FText::GetEmpty(); }
	const ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) { return FText::GetEmpty(); }
	
	if (Player->GetCarriedItem() != ECarriedItemType::None)
	{
		return FText::FromString(TEXT("HANDS FULL"));
	}
	return FText::FromString(TEXT("TAKE FUSE"));
	
}

void ATallowFusePickup::ResetPickup()
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
}

