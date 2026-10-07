#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TallowHoldProgressWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;

UCLASS()
class TALLOW_API UTallowHoldProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Updates the circular fill*/
	UFUNCTION(BlueprintCallable, Category = "UI|Interactions")
	void SetHoldProgress(float InPercent);

	/** Show or hides prompt*/
	UFUNCTION(BlueprintCallable, Category = "UI|Interactions")
	void SetPromptVisibility(bool bShow);
	
protected:
	virtual void NativeConstruct() override;
	
	/** Bound to the image widget in UMG */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ProgressRingImage;
	
	/** The runtime material instance driving the 'Percent' parameter*/
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicRingMaterial;
};
