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
	
	if (Player->HasFuse()) return;
	
	// give fuse and broadcast sound/change
	Player->GiveFuse();
	OnFusePickedUp.Broadcast();
	
	// Deactivate it
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}	

void ATallowFusePickup::ResetPickup()
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
}

