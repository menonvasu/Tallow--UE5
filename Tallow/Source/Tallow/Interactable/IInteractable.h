#pragma once

#include "CoreMinimal.h"
#include "StateTreeNodeDescriptionHelpers.h"
#include "UObject/Interface.h"
#include "IInteractable.generated.h"

UINTERFACE(MinimalAPI)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

class TALLOW_API IInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interactions")
	void OnInteract(AActor* Interactor);
	virtual void OnInteract_Implementation(AActor* Interactor) {}
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interactions")
	float GetInteractionDuration() const;
	virtual float GetInteractionDuration_Implementation() const { return 0.0f; }
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interactions")
	void OnInteractStart(AActor* Interactor);
	virtual void OnInteractStart_Implementation(AActor* Interactor) {}
	
	// called if player completes duration
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interactions")
	void OnInteractComplete(AActor* Interactor); 
	virtual void OnInteractComplete_Implementation(AActor* Interactor) {}
	
	// called if player cancels action	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interactions")
	void OnInteractCancel(AActor* Interactor);
	virtual void OnInteractCancel_Implementation(AActor* Interactor) {}
	
	// returns if player is able to interact
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interactions")
	bool CanInteract(AActor* Interactor) const;
	virtual bool CanInteract_Implementation(AActor* Interactor) const { return true; }
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interactions")
	FText GetInteractionPromptText(AActor* Interactor) const;
	virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const { return FText::GetEmpty(); }
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interactions")
	void OnInteractTick(AActor* Interactor, float DeltaTime, float Progress);
	virtual void OnInteractTick_Implementation(AActor* Interactor, float DeltaTime, float Progress) {}
};
