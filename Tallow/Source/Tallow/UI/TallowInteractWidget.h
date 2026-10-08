#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TallowInteractWidget.generated.h"

class UTextBlock;
class UProgressBar;

UCLASS()
class TALLOW_API UTallowInteractWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Updates the circular fill*/
	UFUNCTION(BlueprintCallable, Category = "UI|Interactions")
	void SetHoldProgress(float InPercent);

	/** Show or hides prompt*/
	UFUNCTION(BlueprintCallable, Category = "UI|Interactions")
	void ShowPrompt(const FText& ActionText, const bool bIsHold, const bool bCanInteract);
	
	UFUNCTION(BlueprintCallable, Category = "UI|Interactions")
	void HidePrompt();
	
protected:
	virtual void NativeConstruct() override;
	
	/** Text to show */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* ActionLabelText;
	
	/** Progress Bar */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HoldProgressBar;
	
	// Configurable colors in Defaults / Blueprint
	UPROPERTY(EditDefaultsOnly, Category = "UI|Colors")
	FLinearColor NormalColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "UI|Colors")
	FLinearColor WarningColor = FLinearColor(0.95f, 0.15f, 0.12f, 1.0f);
};
