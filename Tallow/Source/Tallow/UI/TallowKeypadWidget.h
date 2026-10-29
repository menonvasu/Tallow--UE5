#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TallowKeypadWidget.generated.h"
class ATallowEscapePoint;
class UTextBlock;
class UCanvasPanel;

/** Native, fully operable release keypad. Existing Blueprint button bindings remain supported. */
UCLASS(Blueprintable, BlueprintType)
class TALLOW_API UTallowKeypadWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void InitializeKeypad(ATallowEscapePoint* EscapePoint);
 UFUNCTION(BlueprintCallable, Category="Keypad") void Button_DigitPressed(int32 Digit);
 UFUNCTION(BlueprintCallable, Category="Keypad") void Button_ClearPressed();
 UFUNCTION(BlueprintCallable, Category="Keypad") void Button_SubmitPressed();
 UFUNCTION(BlueprintCallable, Category="Keypad") void ConvertArray();
protected:
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual FReply NativeOnKeyDown(const FGeometry& InGeometry,const FKeyEvent& InKeyEvent) override;
private:
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UCanvasPanel> KeypadCanvas;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> InfoTextBlock;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> NumberBlock;
 TWeakObjectPtr<ATallowEscapePoint> LinkedLatch;
 TArray<int32> EnteredDigits;
 FString DisplayDigits() const;
 /** Gamepad cursor over the 3x4 grid: 1-9, then clear, 0, enter. */
 int32 PadCell=4;
 void PressPadCell();
 FString Status=TEXT("five digits, in the order the film showed them");
 bool bError=false;
};
