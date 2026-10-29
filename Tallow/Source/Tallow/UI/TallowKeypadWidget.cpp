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

void UTallowKeypadWidget::PressPadCell()
{
 if (PadCell==9) Button_ClearPressed();
 else if (PadCell==11) Button_SubmitPressed();
 else Button_DigitPressed(PadCell==10?0:PadCell+1);
}
FReply UTallowKeypadWidget::NativeOnKeyDown(const FGeometry&,const FKeyEvent& Event)
{
 const FKey Key=Event.GetKey();
 const ETallowPadIntent Intent=TallowUIInput::ClassifyGamepad(Key);
 if (Intent!=ETallowPadIntent::None)
 {
  int32 Col=PadCell%3,Row=PadCell/3;
  switch (Intent)
  {
  case ETallowPadIntent::Left: Col=(Col+2)%3; break;
  case ETallowPadIntent::Right: Col=(Col+1)%3; break;
  case ETallowPadIntent::Up: Row=(Row+3)%4; break;
  case ETallowPadIntent::Down: Row=(Row+1)%4; break;
  case ETallowPadIntent::Confirm: PressPadCell(); break;
  case ETallowPadIntent::Secondary: if (!EnteredDigits.IsEmpty()) EnteredDigits.Pop(); bError=false; ConvertArray(); break;
  case ETallowPadIntent::Tertiary: Button_ClearPressed(); break;
  case ETallowPadIntent::NextTab: Button_SubmitPressed(); break;
  case ETallowPadIntent::Back: case ETallowPadIntent::Menu:
   if (LinkedLatch.IsValid()) LinkedLatch->CloseKeypad(); else RemoveFromParent();
   break;
  default: break;
  }
  PadCell=Row*3+Col;
  return FReply::Handled();
 }
 if (Key==EKeys::Enter) Button_SubmitPressed();
 else if (Key==EKeys::BackSpace)
 {
  if (!EnteredDigits.IsEmpty()) EnteredDigits.Pop();
  bError=false;
  ConvertArray();
 }
 else if (Key==EKeys::Delete || Key==EKeys::C) Button_ClearPressed();
 else if (Key==EKeys::Escape || Key==EKeys::E)
 {
  if (LinkedLatch.IsValid()) LinkedLatch->CloseKeypad(); else RemoveFromParent();
 }
 else
 {
  const FKey Main[]={EKeys::Zero,EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight,EKeys::Nine};
  const FKey Pad[]={EKeys::NumPadZero,EKeys::NumPadOne,EKeys::NumPadTwo,EKeys::NumPadThree,EKeys::NumPadFour,EKeys::NumPadFive,EKeys::NumPadSix,EKeys::NumPadSeven,EKeys::NumPadEight,EKeys::NumPadNine};
  for (int32 I=0;I<10;++I) if (Key==Main[I] || Key==Pad[I]) { Button_DigitPressed(I); break; }
 }
 // UI owns every key until it closes, including inventory, map and pause shortcuts.
 return FReply::Handled();
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
