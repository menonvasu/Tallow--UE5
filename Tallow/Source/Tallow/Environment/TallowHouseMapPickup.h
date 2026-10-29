#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowHouseMapPickup.generated.h"
class UStaticMeshComponent;
UCLASS()
class TALLOW_API ATallowHouseMapPickup : public AActor,public IInteractable
{
 GENERATED_BODY()
public:
 ATallowHouseMapPickup();
 virtual bool CanInteract_Implementation(AActor* Interactor)const override;
 virtual void OnInteract_Implementation(AActor* Interactor)override;
 virtual FText GetInteractionPromptText_Implementation(AActor* Interactor)const override;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Map")TObjectPtr<UStaticMeshComponent>Paper;
};
