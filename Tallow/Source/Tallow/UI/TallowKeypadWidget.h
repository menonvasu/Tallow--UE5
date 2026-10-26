#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TallowKeypadWidget.generated.h"

class ATallowEscapePoint;
class UTallowEscapePoint;
class UTextBlock;
class UCanvasPanel;

UCLASS(Blueprintable, BlueprintType)
class TALLOW_API UTallowKeypadWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	/** connects UI to the game simulation */
	void InitializeKeypad(ATallowEscapePoint* EscapePoint);

private:
	virtual  void NativeConstruct() override;
	
	virtual void NativeDestruct() override;
	
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	
	UFUNCTION(BlueprintCallable, Category = "Keypad")
	void Button_DigitPressed(int32 Digit);
	UFUNCTION(BlueprintCallable, Category = "Keypad")
	void Button_ClearPressed();
	UFUNCTION(BlueprintCallable, Category = "Keypad")
	void Button_SubmitPressed();
	UFUNCTION(BlueprintCallable, Category = "Keypad")
	void ConvertArray();
	
	/** background grid */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> KeypadCanvas;
	
	/** holds text */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* InfoTextBlock;
	
	/** holds numbers */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* NumberBlock;
	
	TWeakObjectPtr<ATallowEscapePoint> LinkedLatch;

	TArray<int32> EnteredDigits;
};
