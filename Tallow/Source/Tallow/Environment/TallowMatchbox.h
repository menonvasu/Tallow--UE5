#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowMatchbox.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** A box of matches (25 Oct). The first, by the candle, holds 10; boxes around the house add more. */
UCLASS()
class TALLOW_API ATallowMatchbox : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ATallowMatchbox();

	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;

	/** A capture restarts the house; the box goes back where it was. */
	UFUNCTION(BlueprintCallable, Category = "Matches")
	void ResetMatchbox();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Matches")
	int32 Matches = 5;

protected:
	/** Larger than the box itself so the interaction line finds it without pixel hunting. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Matches")
	TObjectPtr<UBoxComponent> Grab;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Matches")
	TObjectPtr<UStaticMeshComponent> Box;
};
