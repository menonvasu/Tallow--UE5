#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowInteractionComponent.generated.h"

class UCameraComponent;
class UTallowInteractWidget;
class AActor;
class APlayerController;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TALLOW_API UTallowInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UTallowInteractionComponent();

	UFUNCTION(BlueprintCallable, Category = "Interact")
	void StartInteract();
	
	UFUNCTION(BlueprintCallable, Category = "Interact")
	void CancelInteract();

	/** Read-only presentation hooks; interaction timing and completion remain owned here. */
	UFUNCTION(BlueprintPure, Category="Interact")
	bool IsInteracting() const { return bIsHoldingInteract; }
	UFUNCTION(BlueprintPure, Category="Interact")
	AActor* GetInteractionTarget() const { return CurrentInteractTarget.Get(); }

protected:
	virtual void BeginPlay() override;
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	/** Timer handles */
	FTimerHandle HoverCheckTimerHandle;
	FTimerHandle HoldTimerHandle;
	
	static constexpr float HoverCheckInterval = 0.1f;
	static constexpr float HoldTickInterval = 0.02f;
	
	TWeakObjectPtr<AActor> CurrentHoverActor;
	TWeakObjectPtr<AActor> CurrentInteractTarget;
	
	float CurrentHoldTime = 0.0f;
	
	float TargetHoldDuration = 0.0f;
	
	bool bIsHoldingInteract = false;
	
	bool bCanInteract = false;
	
	bool bIsHoldType = false;
	
	bool bHasProgress = false;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact|UI")
	TSubclassOf<UTallowInteractWidget> HoldProgressWidgetClass;
	
	UPROPERTY(Transient)
	TObjectPtr<UTallowInteractWidget> HoldProgressWidgetInstance;
	
	/** Pointer to cache Player Controller */
	UPROPERTY()
	TWeakObjectPtr<APlayerController> PC = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact|Tunables")
	float InteractionDistance = 200.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact|Tunables")
	float HoldLookThreshold = 0.8f;
	
	FText CachedPrompt;
	
	void DoHoverCheck();
	
	void UpdateHoldStep();
	
	void ResetHoldState();
	
	AActor* RunRaycast() const;
	
	UCameraComponent* GetOwnerCamera() const;
};
