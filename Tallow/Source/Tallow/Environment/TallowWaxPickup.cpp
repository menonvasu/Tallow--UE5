#include "Environment/TallowWaxPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Candle/TallowCandleComponent.h"
#include "Player/TallowCharacter.h"
ATallowWaxPickup::ATallowWaxPickup()
{
 PrimaryActorTick.bCanEverTick=false;Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpareCandle"));RootComponent=Mesh;
 Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);Mesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);Mesh->SetCanEverAffectNavigation(false);
}
bool ATallowWaxPickup::CanInteract_Implementation(AActor* Interactor)const
{
 const auto* P=Cast<ATallowCharacter>(Interactor);const auto* C=P?P->GetCandleComponent():nullptr;
 return !IsHidden() && P && P->CanChangeInventory() && C && C->IsAcquired()&&!C->IsGivenUp()&&C->GetWaxFraction()<.95f;
}
void ATallowWaxPickup::OnInteract_Implementation(AActor* Interactor)
{
 if(!CanInteract_Implementation(Interactor))return;
 Cast<ATallowCharacter>(Interactor)->GetCandleComponent()->AddWax(RefillFraction);SetActorHiddenInGame(true);SetActorEnableCollision(false);
}
FText ATallowWaxPickup::GetInteractionPromptText_Implementation(AActor* Interactor)const
{return FText::FromString(TEXT("take spare candle"));}
