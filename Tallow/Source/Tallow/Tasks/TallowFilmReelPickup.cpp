#include "TallowFilmReelPickup.h"
#include "Player/TallowCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

// Sets default values
ATallowFilmReelPickup::ATallowFilmReelPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	
	FilmReelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FilmReelMesh"));
	RootComponent = FilmReelMesh;
	FilmReelMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Presentation/ProjectorFinal/SM_FilmReel.SM_FilmReel")));
	FilmReelMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	FilmReelMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECR_Block);
}

bool ATallowFilmReelPickup::CanInteract_Implementation(AActor* Interactor) const
{
	if (!Interactor) { return false; }
	ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) { return false; }
	
	return !IsHidden() && Player->CanCollectItem(ECarriedItemType::FilmReel);
}

void ATallowFilmReelPickup::OnInteract_Implementation(AActor* Interactor)
{
	if (!Interactor) { return; }
	ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) { return; }
	
	if (IsHidden() || !Player->CollectItem(ECarriedItemType::FilmReel)) return;
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
	
	if (!Player->CanCollectItem(ECarriedItemType::FilmReel))
	{
		return FText::FromString("FILM REEL ALREADY TAKEN");
	}
	return FText::FromString("TAKE FILM REEL");
}

void ATallowFilmReelPickup::ResetReelPickup()
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
}
