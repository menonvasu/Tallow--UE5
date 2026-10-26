#include "TallowFilmReelPickup.h"
#include "Player/TallowCharacter.h"
#include "Components/StaticMeshComponent.h"

// Sets default values
ATallowFilmReelPickup::ATallowFilmReelPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	
	FilmReelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FilmReelMesh"));
	RootComponent = FilmReelMesh;
	FilmReelMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	FilmReelMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECR_Block);
}

bool ATallowFilmReelPickup::CanInteract_Implementation(AActor* Interactor) const
{
	if (!Interactor) { return false; }
	ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) { return false; }
	
	// if player holding item, cannot interact
	if (Player->GetCarriedItem() != ECarriedItemType::None)
	{
		return false;
	}
	return true;
}

void ATallowFilmReelPickup::OnInteract_Implementation(AActor* Interactor)
{
	if (!Interactor) { return; }
	ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) { return; }
	
	// if not carrying item, return
	if (Player->GetCarriedItem() != ECarriedItemType::None) { return; }
	
	// set carried item and broadacast change
	Player->SetCarriedItem(ECarriedItemType::FilmReel);
	OnFilmReelPickup.Broadcast();
	
	// deactivate it
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

FText ATallowFilmReelPickup::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
	if (!Interactor) { return FText::GetEmpty(); }
	const ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) { return FText::GetEmpty(); }
	
	if (Player->GetCarriedItem() != ECarriedItemType::None)
	{
		return FText::FromString("HANDS FULL");
	}
	return FText::FromString("TAKE FILM REEL");
}

void ATallowFilmReelPickup::ResetReelPickup()
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
}
