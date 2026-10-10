#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowCrowbarPickup.generated.h"

class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FonCrowbarAction, bool, bOnAction);

UCLASS()
class TALLOW_API ATallowCrowbarPickup : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ATallowCrowbarPickup();
	
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	
	virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;
	
	void ResetPickup();
	
	/** Events */
	UPROPERTY(BlueprintAssignable, Category = "Crowbar|Events")
	FonCrowbarAction OnCrowbarAction;

protected:
	virtual void BeginPlay() override;

	/** Crowbar mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crowbar")
	TObjectPtr<UStaticMeshComponent> CrowbarMesh;
};
