#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowCollectionSlot.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** Maisie's place in the collection. Once the physical escape route is open, choosing to look for her
 * starts The Finished Child ending. Nell keeps her candle; the earlier candle-sacrifice branch is retired. */
UCLASS()
class TALLOW_API ATallowCollectionSlot : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ATallowCollectionSlot();
	virtual void BeginPlay() override;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;
	virtual float GetInteractionDuration_Implementation() const override { return 1.6f; }
	virtual void OnInteractComplete_Implementation(AActor* Interactor) override;

	UFUNCTION(BlueprintCallable, Category = "Collection")
	void ResetSlot();

	UFUNCTION(BlueprintPure, Category = "Collection")
	bool IsFilled() const { return bFilled; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collection")
	TObjectPtr<UBoxComponent> Grab;

	/** Nell's candle, unlit, standing in the hollow once she gives it back (copies the candle pickup's mesh). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collection")
	TObjectPtr<UStaticMeshComponent> LeftCandle;

private:
	bool bFilled = false;
};
