#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowWaxPickup.generated.h"
class UStaticMeshComponent;
UCLASS()
class TALLOW_API ATallowWaxPickup : public AActor, public IInteractable
{
 GENERATED_BODY()
public:
 ATallowWaxPickup();
 virtual bool CanInteract_Implementation(AActor* Interactor)const override;
 virtual void OnInteract_Implementation(AActor* Interactor)override;
 virtual FText GetInteractionPromptText_Implementation(AActor* Interactor)const override;
 void ResetSupply(){SetActorHiddenInGame(false);SetActorEnableCollision(true);}
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Wax")float RefillFraction=.6f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Wax")TObjectPtr<UStaticMeshComponent>Mesh;
};
