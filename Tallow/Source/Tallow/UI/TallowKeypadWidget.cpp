#include "TallowKeypadWidget.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Presentation/TallowEscapePoint.h"
#include "UI/Presentation/TallowPaper.h"
#include "UI/Presentation/STallowMark.h"
#include "Input/TallowInputPrompts.h"
#include "Input/TallowUIInput.h"
#include "Input/TallowInputDeviceSubsystem.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

void UTallowKeypadWidget::InitializeKeypad(ATallowEscapePoint* EscapePoint)
{
 LinkedLatch=EscapePoint;
 Button_ClearPressed();
}
void UTallowKeypadWidget::Button_DigitPressed(int32 Digit)
{
 if (Digit<0 || Digit>9 || EnteredDigits.Num()>=5) return;
 EnteredDigits.Add(Digit);
 bError=false;
 Status=TEXT("five digits, in the order the film showed them");
 ConvertArray();
}
void UTallowKeypadWidget::Button_ClearPressed()
{
 EnteredDigits.Reset();
 bError=false;
 Status=TEXT("five digits, in the order the film showed them");
 ConvertArray();
}
void UTallowKeypadWidget::Button_SubmitPressed()
{
 if (!LinkedLatch.IsValid()) return;
 if (!LinkedLatch->CheckSubmittedCode(EnteredDigits))
 {
  bError=true;
  Status=EnteredDigits.Num()==5?TEXT("wrong sequence. the alarm carries through the house."):TEXT("the release needs all five digits.");
  EnteredDigits.Reset();
  ConvertArray();
 }
}
void UTallowKeypadWidget::NativeConstruct()
{
 Super::NativeConstruct();
 SetIsFocusable(true);
 ConvertArray();
}
void UTallowKeypadWidget::NativeDestruct()
{
 if (LinkedLatch.IsValid()) LinkedLatch->CloseKeypad();
 Super::NativeDestruct();
}

FString UTallowKeypadWidget::DisplayDigits() const
{
 FString Result;
 for (int32 I=0;I<5;++I)
 {
  if (I) Result+=TEXT("   ");
  Result+=EnteredDigits.IsValidIndex(I)?FString::FromInt(EnteredDigits[I]):TEXT("—");
 }
 return Result;
}
void UTallowKeypadWidget::ConvertArray()
{
 if (NumberBlock) NumberBlock->SetText(FText::FromString(DisplayDigits()));
 if (InfoTextBlock)
 {
  InfoTextBlock->SetText(FText::FromString(Status));
  InfoTextBlock->SetColorAndOpacity(bError?TallowPaper::Blood:TallowPaper::Ink);
 }
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

TSharedRef<SWidget> UTallowKeypadWidget::RebuildWidget()
{
 // Keep authored UMG keypads usable; native actors need no Blueprint widget asset.
 if (WidgetTree && WidgetTree->RootWidget) return Super::RebuildWidget();
 const FLinearColor Ivory=TallowPaper::Ink;
 // Each key is the engraved cap the prompts use: dust at rest, lit under the hand, the thread when pressed.
 static const FButtonStyle KeyStyle=[](){
  FButtonStyle S=FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder");
  FSlateBrush B=*TallowPaper::KeyCap();
  B.TintColor=FSlateColor(TallowPaper::Pencil.CopyWithNewOpacity(.8f));S.SetNormal(B);
  B.TintColor=FSlateColor(TallowPaper::Ink);S.SetHovered(B);
  B.TintColor=FSlateColor(TallowPaper::Thread);S.SetPressed(B);return S;}();
 const auto* White=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
 auto Rows=SNew(SVerticalBox);
 auto Key=[this,Ivory,White](int32 Cell,const FString& Label,TFunction<void()> Action)
 {
  // The gamepad cursor is an ink frame round the cap; it shows only while the pad was used last.
  return SNew(SBorder).BorderImage(White).Padding(3)
   .BorderBackgroundColor_Lambda([this,Cell](){return PadCell==Cell&&UTallowInputDeviceSubsystem::GetCurrent(this)==ETallowInputDevice::Gamepad?TallowPaper::Ink:FLinearColor::Transparent;})
   [SNew(SBox).WidthOverride(105).HeightOverride(66)
   [SNew(SButton).ButtonStyle(&KeyStyle).ContentPadding(8)
    .OnClicked_Lambda([Action](){Action();return FReply::Handled();})
    [SNew(STextBlock).Text(FText::FromString(Label)).Font(Label.Len()>1?TallowPaper::FellItalic(TallowPaper::Pt::Body):TallowPaper::Fell(TallowPaper::Pt::Lead+2)).Justification(ETextJustify::Center).ColorAndOpacity(Ivory)]]];
 };
 for (int32 Row=0;Row<4;++Row)
 {
  auto Keys=SNew(SHorizontalBox);
  for (int32 Col=0;Col<3;++Col)
  {
   const int32 Value=Row*3+Col+1;
   if (Value==10) Keys->AddSlot().AutoWidth().Padding(2)[Key(Row*3+Col,TEXT("clear"),[this](){Button_ClearPressed();})];
   else if (Value==12) Keys->AddSlot().AutoWidth().Padding(2)[Key(Row*3+Col,TEXT("enter"),[this](){Button_SubmitPressed();})];
   else { const int32 Digit=Value==11?0:Value;Keys->AddSlot().AutoWidth().Padding(2)[Key(Row*3+Col,FString::FromInt(Digit),[this,Digit](){Button_DigitPressed(Digit);})]; }
  }
  Rows->AddSlot().AutoHeight().HAlign(HAlign_Center)[Keys];
 }
 // The latch's brass plate is the world's; here only its words and keys, on soot, in the book face.
 auto Panel=SNew(SBorder).BorderImage(TallowPaper::Ground()).BorderBackgroundColor(FLinearColor(1,1,1,.97f)).Padding(FMargin(44,36))
  [SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(STextBlock).Text(FText::FromString(TEXT("THE ATTIC RELEASE"))).Font(TallowPaper::Caps(TallowPaper::Pt::Label)).ColorAndOpacity(TallowPaper::Pencil)]
   +SVerticalBox::Slot().AutoHeight().Padding(0,18,0,4).HAlign(HAlign_Center)
   [SNew(STextBlock).Text_Lambda([this](){return FText::FromString(DisplayDigits());}).Justification(ETextJustify::Center).Font(TallowPaper::Fell(TallowPaper::Pt::Heading)).ColorAndOpacity_Lambda([this](){return bError?TallowPaper::Blood:TallowPaper::Ivory;})]
   +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,0,0,20)[SNew(SBox).WidthOverride(200).HeightOverride(8)[SNew(STallowThread).Seed(5)]]
   +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Rows]
   +SVerticalBox::Slot().AutoHeight().Padding(0,20,0,10)
   [SNew(STextBlock).Text_Lambda([this](){return FText::FromString(Status);}).WrapTextAt(360).Justification(ETextJustify::Center).Font(TallowPaper::FellItalic(TallowPaper::Pt::Body)).ColorAndOpacity_Lambda([this](){return bError?TallowPaper::Blood:TallowPaper::Ink;})]
   +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
   [SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("NoBorder")))
    .OnClicked_Lambda([this](){if(LinkedLatch.IsValid())LinkedLatch->CloseKeypad();return FReply::Handled();})
    [SNew(STallowKeys).Text_Lambda([this](){return FString(UTallowInputDeviceSubsystem::GetCurrent(this)==ETallowInputDevice::Gamepad?TEXT("[ circle ]  step away    [ square ]  erase    [ triangle ]  clear    [ R1 ]  try it"):TEXT("[ esc ]  step away"));}).Font(TallowPaper::FellItalic(TallowPaper::Pt::Small)).Colour(TallowPaper::Pencil).KeyColour(TallowPaper::Ink)]]];
 return SNew(SOverlay)
  +SOverlay::Slot()[SNew(SBorder).BorderImage(White).BorderBackgroundColor(TallowPaper::Soot.CopyWithNewOpacity(.6f))]
  +SOverlay::Slot().Padding(24).HAlign(HAlign_Center).VAlign(VAlign_Center)
  [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)[Panel]];
}
