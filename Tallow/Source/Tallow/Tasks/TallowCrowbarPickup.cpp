#include "TallowCrowbarPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Player/TallowCharacter.h"

// Sets default values
ATallowCrowbarPickup::ATallowCrowbarPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	CrowbarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CrowbarMesh"));
	RootComponent = CrowbarMesh;
	CrowbarMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CrowbarMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

// Called when the game starts or when spawned
void ATallowCrowbarPickup::BeginPlay()
{
	Super::BeginPlay();
}

bool ATallowCrowbarPickup::CanInteract_Implementation(AActor* Interactor) const
{
	if (!Interactor) return false;
	ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) return false;
	
	return !IsHidden() && Player->CanCollectItem(ECarriedItemType::Crowbar);
}

FText ATallowCrowbarPickup::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
	if (!Interactor) return FText::GetEmpty();
	ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) return FText::GetEmpty();
	
	// hands free!
	if (Player->CanCollectItem(ECarriedItemType::Crowbar))
	{
		return FText::FromString("TAKE CROWBAR");
	}
	return FText::FromString("HANDS FULL");
}

void ATallowCrowbarPickup::OnInteract_Implementation(AActor* Interactor)
{
	if (!Interactor) return; 
	ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) return;
	
	// sets carried item, hides actor and disables collision
	if (IsHidden() || !Player->CollectItem(ECarriedItemType::Crowbar)) return;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	
	//broadcast sound & animation
	OnCrowbarAction.Broadcast(true);	
}

void ATallowCrowbarPickup::ResetPickup()
{
	// enable visibility and collision
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
}