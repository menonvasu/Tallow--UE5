#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowPlank.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPryPlank);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlankBeingBroken);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlankDestroyed, bool, bBroken);

class UStaticMeshComponent;
class ATallowDoor;

UCLASS()
class TALLOW_API ATallowPlank : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	/** Sets default properties */
	ATallowPlank();
	bool IsBroken() const { return bBroken; }

	virtual float GetInteractionDuration_Implementation() const override;
	
	virtual void OnInteractStart_Implementation(AActor* Interactor) override;
	
	virtual void OnInteractComplete_Implementation(AActor* Interactor) override;
	
	virtual void OnInteractCancel_Implementation(AActor* Interactor) override;
	
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;

	virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;
	
	virtual void OnInteractTick_Implementation(AActor* Interactor, float DeltaTime, float Progress) override;
	
	virtual void ResetPlankState();
	void SetChapterRetired(bool bRetired);
	
	/** Optional barricades open side routes without enlarging the final escape requirement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Barricade")
	bool bRequiredForEscape = true;

	/** Plank breaking loudness */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Plank|Tunables")
	float PlankPryLoudness = 0.8f;
	
	/** Hold duration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plank|Tunables")
	float PryHoldDuration = 2.0f;
	
	/** Noise intermittent duration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plank|Tunables")
	float NoisePerTime = 0.5f;
	
	/** Events */
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "Plank|Events")
	FOnPryPlank OnPryPlank;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "Plank|Events")
	FOnPlankDestroyed OnPlankDestroyed;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "Plank|Events")
	FOnPlankBeingBroken OnPlankBeingBroken;
	
protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plank")
	TObjectPtr<UStaticMeshComponent> PlankMesh;
	
	UPROPERTY(EditInstanceOnly, Category = "Barricade")
	TObjectPtr<class ATallowDoor> TargetDoor = nullptr;
	
	/** Initial transform location of plank */
	FTransform InitialPlankTransform;
	
	/** Is plank broken? */
	bool bBroken = false;
	
	/** How long it has been since plank last made noise during breaking */
	float TimeSinceLastNoise = 0.0f;
};
