#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowProjector.generated.h"

class UStaticMeshComponent;

UCLASS()
class TALLOW_API ATallowProjector : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ATallowProjector();

	virtual float GetInteractionDuration_Implementation() const override;
	
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	
	virtual void OnInteractComplete_Implementation(AActor* Interactor) override;
	
	virtual void OnInteractCancel_Implementation(AActor* Interactor) override;
	
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	
	virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;
	
	UFUNCTION(BlueprintCallable, Category = "FuseBox|Reset")
	void ResetVisual();
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projector")
	TObjectPtr<UStaticMeshComponent> ProjectorMesh;
};
