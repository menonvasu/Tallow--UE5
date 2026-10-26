#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowEscapePoint.generated.h"
class UStaticMeshComponent;
class ATallowProjector;
class UTallowKeypadWidget;
/** Final release latch. Uses the project's existing interaction system. */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWrongAccessCode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCorrectAccessCode);
UCLASS()
class TALLOW_API ATallowEscapePoint : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ATallowEscapePoint();
	virtual void BeginPlay() override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Latch;
	virtual float GetInteractionDuration_Implementation() const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual void OnInteractComplete_Implementation(AActor* Interactor) override;
	
	UFUNCTION(BlueprintCallable, Category = "Escape")
	bool CheckSubmittedCode(const TArray<int32>& EnteredCode);
	
	bool GetIsCurrentlyInputtingKeypad() const { return bIsCurrentlyInputtingKeypad; }
	
	void SetCodeVerified(bool bNewState) { bCodeVerified = bNewState; }
	
	UPROPERTY(BlueprintCallable, Category = "Escape|Events")
	FOnWrongAccessCode OnWrongAccessCode;
	
	UPROPERTY(BlueprintCallable, Category = "Escape|Events")
	FOnCorrectAccessCode OnCorrectAccessCode;

protected:
	UPROPERTY(EditInstanceOnly, Category = "Escape")
	TObjectPtr<ATallowProjector> LinkedProjector;
	
	UPROPERTY(EditDefaultsOnly, Category = "Escape|UI")
	TSubclassOf<UTallowKeypadWidget> KeypadWidgetClass;
	
	UPROPERTY(Transient)
	TObjectPtr<UTallowKeypadWidget> ActiveWidgetInstance;
	
	TWeakObjectPtr<APlayerController> CachedPC;
	
	UPROPERTY(EditDefaultsOnly, Category = "Escape|Sound")
	float InputFailedLoudness = 0.8f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Escape|Sound")
	float InputFailedRange = 1.0f;
	
	TArray<int32> CurrentCode;
	
	bool bCodeVerified = false;
	bool bIsCurrentlyInputtingKeypad = false;
};
