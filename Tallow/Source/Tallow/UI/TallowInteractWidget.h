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
	/** Drawn in native code in the handmade style (TallowPaper): the WBP's designer widgets stay bound but unseen. */
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	FText Label;
	bool bHold = false;
	bool bCanAct = true;
	float Progress = 0.f;
	
	/** Text to show */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* ActionLabelText;
	
	/** Progress Bar */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HoldProgressBar;
	
	/** Input key badge */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> KeyPromptBadge;
	
	// Configurable colors in Defaults / Blueprint
	UPROPERTY(EditDefaultsOnly, Category = "UI|Colors")
	FLinearColor NormalColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "UI|Colors")
	FLinearColor WarningColor = FLinearColor(0.95f, 0.15f, 0.12f, 1.0f);
};
