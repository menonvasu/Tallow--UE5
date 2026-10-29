#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowCandlePickup.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

UCLASS()
class TALLOW_API ATallowCandlePickup : public AActor, public IInteractable
{
	
	GENERATED_BODY()

public:
	ATallowCandlePickup();

	virtual void OnInteract_Implementation(AActor* Interactor) override;

	virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;
	
	UFUNCTION(BlueprintCallable, Category = "Pickup")
	void ResetPickup(AActor* Interactor);

    void SetTakenForChapter(bool bTaken);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UPointLightComponent> TableLightComponent;

};