#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TallowLockpickWidget.generated.h"

class UTallowLockpickGame;
class UCanvasPanel;
class UBorder;
class UProgressBar;
class UTextBlock;

UCLASS()
class TALLOW_API UTallowLockpickWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	/** connects UI to the game simulation */
	void InitializeLockpickWidget(UTallowLockpickGame* InGameComponent);
	
protected:
	virtual void NativeTick(const FGeometry& Geometry, float InDeltaTime) override;
	
	virtual void NativeDestruct() override;
	
	/** Shows dynamic feedback */
	void ShowStatus(const FText& Message);
	
	/** Handlers for feedback */
	UFUNCTION()
	void HandleOnFalseSet();
	UFUNCTION()
	void HandleOnOverSet();
	UFUNCTION()
	void HandleOnTensionReleasedOrOnSnap();
	UFUNCTION()
	void HandleOnPinSet();
	UFUNCTION()
	void HandleOnLockpickSet();
	
	/** background grid */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> LockCanvas;
	
	/** player lockpick */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> PickImage;
	
	/** tension bar */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> TensionBar;
	
	TWeakObjectPtr<UTallowLockpickGame> LockpickGame;
	
	/** Tracked boxes*/
	UPROPERTY(meta = (BindWidget)) 
	TObjectPtr<UBorder> Pin0;
	
	UPROPERTY(meta = (BindWidget)) 
	TObjectPtr<UBorder> Pin1;
	
	UPROPERTY(meta = (BindWidget)) 
	TObjectPtr<UBorder> Pin2;
	
	UPROPERTY(meta = (BindWidget)) 
	TObjectPtr<UBorder> Pin3;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> PinWidgets;
	
	UPROPERTY(meta = (BindWidget))
	UTextBlock* EventFeedbackText;
	
	float StatusTimeRemaining = 0.0f;
	
	/** Tunables for layout */
	UPROPERTY(EditDefaultsOnly, Category = "Lockpick|Layout")
	float PinSpacing = 60.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Lockpick|Layout")
	float MaxLiftPixels = 50.0f;
	
	UPROPERTY(EditAnywhere, Category = "Lockpick|Layout")
	float MaxShakePixels = 2.5f;
	
	float ShakeAlpha = 0.0f;
	float ShakeTime = 0.0f;
	
	UPROPERTY(EditAnywhere, Category = "Lockpick|Layout") 
	float ShakeFadeSpeed = 12.0f;
};
