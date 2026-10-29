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
	if (bChapterDormant || IsHidden() || !Player->CollectItem(ECarriedItemType::Fuse)) return;
	
	// give fuse and broadcast sound/change
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
	
	return !bChapterDormant && !IsHidden() && Player->CanCollectItem(ECarriedItemType::Fuse);
}
	
FText ATallowFusePickup::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
	if (!Interactor) { return FText::GetEmpty(); }
	const ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) { return FText::GetEmpty(); }
	
	if (!Player->CanCollectItem(ECarriedItemType::Fuse))
	{
		return FText::FromString(TEXT("POCKETS FULL"));
	}
	return FText::FromString(TEXT("TAKE ") + (ChapterNoun.IsEmpty() ? FString(TEXT("FUSE")) : ChapterNoun.ToString().ToUpper()));
	
}

void ATallowFusePickup::ResetPickup()
{
	bChapterDormant = false;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
}


void ATallowFusePickup::SetDormant(bool bDormant)
{
    bChapterDormant = bDormant;
    SetActorHiddenInGame(bDormant);
    SetActorEnableCollision(!bDormant);
}
