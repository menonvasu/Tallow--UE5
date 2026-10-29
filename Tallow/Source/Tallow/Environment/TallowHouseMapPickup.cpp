#include "Environment/TallowHouseMapPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Player/Map/TallowHouseMapComponent.h"
ATallowHouseMapPickup::ATallowHouseMapPickup()
{
 PrimaryActorTick.bCanEverTick=false;Paper=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FoldedMap"));RootComponent=Paper;
 Paper->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Paper->SetCollisionResponseToAllChannels(ECR_Ignore);Paper->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);Paper->SetCanEverAffectNavigation(false);
}
bool ATallowHouseMapPickup::CanInteract_Implementation(AActor* Interactor)const
{const auto* M=Interactor?Interactor->FindComponentByClass<UTallowHouseMapComponent>():nullptr;return !IsHidden()&&M&&!M->HasMap();}
void ATallowHouseMapPickup::OnInteract_Implementation(AActor* Interactor)
{
 if(!CanInteract_Implementation(Interactor))return;
 if(auto* M=Interactor->FindComponentByClass<UTallowHouseMapComponent>();M&&M->CollectMap()){SetActorHiddenInGame(true);SetActorEnableCollision(false);}
}
FText ATallowHouseMapPickup::GetInteractionPromptText_Implementation(AActor* Interactor)const
{return FText::FromString(TEXT("take the scribbled map"));}
