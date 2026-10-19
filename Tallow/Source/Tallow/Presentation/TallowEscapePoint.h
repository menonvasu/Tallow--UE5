#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowEscapePoint.generated.h"
class UStaticMeshComponent;
/** Final release latch. Uses the project's existing interaction system. */
UCLASS()
class TALLOW_API ATallowEscapePoint : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ATallowEscapePoint();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Latch;
	virtual float GetInteractionDuration_Implementation() const override { return 1.4f; }
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;
	virtual void OnInteractComplete_Implementation(AActor* Interactor) override;
};
