#include "TallowKeypadWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Presentation/TallowEscapePoint.h"

void UTallowKeypadWidget::InitializeKeypad(ATallowEscapePoint* EscapePoint)
{
	LinkedLatch = EscapePoint;
	EnteredDigits.Empty();
	ConvertArray();
	if (InfoTextBlock)
	{
		InfoTextBlock->SetText(FText::FromString(TEXT("ENTER DIGIT")));
		InfoTextBlock->SetColorAndOpacity(FLinearColor::White);
	}
}

void UTallowKeypadWidget::Button_DigitPressed(int32 Digit)
{
	// add digit to array
	if (EnteredDigits.Num() >= 5) { return; }
	EnteredDigits.Add(Digit);
	if (InfoTextBlock)
	{
		InfoTextBlock->SetText(FText::FromString(TEXT("ENTER DIGIT")));	
		InfoTextBlock->SetColorAndOpacity(FLinearColor::White);
	}
	ConvertArray();	
}

void UTallowKeypadWidget::Button_ClearPressed()
{
	// clear
	EnteredDigits.Empty();
	if (InfoTextBlock)
	{
		InfoTextBlock->SetText(FText::FromString(TEXT("ENTER DIGIT")));
		InfoTextBlock->SetColorAndOpacity(FLinearColor::White);
	}
	ConvertArray();
}

void UTallowKeypadWidget::Button_SubmitPressed()
{
	if (!LinkedLatch.IsValid()) { return; }
	const bool bCorrect = LinkedLatch->CheckSubmittedCode(EnteredDigits);
	
	// set info text to read if fail
	if (!bCorrect)
	{
		if (InfoTextBlock)
		{
			InfoTextBlock->SetColorAndOpacity(FLinearColor(0.95f, 0.15f, 0.12f));
			InfoTextBlock->SetText(FText::FromString(TEXT("ERROR")));	
		}
		EnteredDigits.Empty();
		ConvertArray();
	}
}

void UTallowKeypadWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	SetIsFocusable(true);
}

void UTallowKeypadWidget::NativeDestruct()
{
	if (LinkedLatch.IsValid())
	{
		LinkedLatch->CloseKeypad();
	}
	Super::NativeDestruct();
}

FReply UTallowKeypadWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	FKey Key = InKeyEvent.GetKey();
	
	if (Key == EKeys::Enter)
	{
		Button_SubmitPressed();
		return FReply::Handled();
	}
	if (Key == EKeys::BackSpace)
	{
		Button_ClearPressed();
		return FReply::Handled();
	}
	if (Key == EKeys::Escape || Key == EKeys::E)
	{
		// Close the widget and cancel
		if (LinkedLatch.IsValid()) LinkedLatch->CloseKeypad();
		else RemoveFromParent();
		return FReply::Handled();
	}
	
	int32 TargetDigit = INDEX_NONE;
	if (Key == EKeys::Zero || Key == EKeys::NumPadZero) { TargetDigit = 0; }
	else if (Key == EKeys::One || Key == EKeys::NumPadOne) { TargetDigit = 1; }
	else if (Key == EKeys::Two || Key == EKeys::NumPadTwo) { TargetDigit = 2; }
	else if (Key == EKeys::Three || Key == EKeys::NumPadThree) { TargetDigit = 3; }
	else if (Key == EKeys::Four || Key == EKeys::NumPadFour) { TargetDigit = 4; }
	else if (Key == EKeys::Five || Key == EKeys::NumPadFive) { TargetDigit = 5; }
	else if (Key == EKeys::Six || Key == EKeys::NumPadSix) { TargetDigit = 6; }
	else if (Key == EKeys::Seven || Key == EKeys::NumPadSeven) { TargetDigit = 7; }
	else if (Key == EKeys::Eight || Key == EKeys::NumPadEight) { TargetDigit = 8; }
	else if (Key == EKeys::Nine || Key == EKeys::NumPadNine) { TargetDigit = 9; }
	
	if (TargetDigit != INDEX_NONE)
	{
		Button_DigitPressed(TargetDigit);
		return FReply::Handled();
	}
	
	return FReply::Unhandled();
}

void UTallowKeypadWidget::ConvertArray()
{
	// convert array to string 
	FString ResultString;
	for (int32 i = 0; i < 5; i++)
	{
		if (i < EnteredDigits.Num())
		{
			ResultString.Append(FString::FromInt(EnteredDigits[i]));
		}
		else
		{
			ResultString.Append(TEXT("_"));
		}
		ResultString.Append(TEXT("  "));
	}
	if (NumberBlock)
	{
		NumberBlock->SetText(FText::FromString(ResultString));	
	}
}
