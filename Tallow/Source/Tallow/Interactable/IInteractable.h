#pragma once

#include "CoreMinimal.h"
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
};
