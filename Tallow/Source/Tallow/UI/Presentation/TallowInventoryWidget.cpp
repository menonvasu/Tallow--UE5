#include "UI/Presentation/TallowInventoryWidget.h"
#include "UI/Presentation/TallowPaper.h"
#include "Input/TallowInputPrompts.h"
#include "Input/TallowUIInput.h"
#include "Input/TallowInputDeviceSubsystem.h"
#include "UI/Presentation/STallowMark.h"
#include "Player/TallowCharacter.h"
#include "Player/Map/TallowHouseMapComponent.h"
#include "Presentation/TallowStorySubsystem.h"
#include "Presentation/TallowNarrative.h"
#include "Presentation/TallowNellNotes.h"
#include "UI/Presentation/TallowJournalWidget.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Candle/TallowCandleComponent.h"
#include "GameMode/TallowGameMode.h"
#include "Tasks/TallowLockpickGame.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
UTallowInventoryWidget::UTallowInventoryWidget(const FObjectInitializer& O):Super(O){SetIsFocusable(true);}
void UTallowInventoryWidget::Open(APlayerController* PC)
{
 if(!PC || bOpen)return;Player=Cast<ATallowCharacter>(PC->GetPawn());if(!Player.IsValid())return;
 if(Player->IsTiedToChair() || Player->GetCandleComponent()->IsRelighting())return;
 if(auto* GM=PC->GetWorld()->GetAuthGameMode<ATallowGameMode>();GM && GM->IsCaptureInProgress())return;
 if(auto* Lock=Player->GetLockpickComponent();Lock && Lock->IsComponentTickEnabled())return;
 Player->PrepareForInventory();PC->FlushPressedKeys();InputOwner=PC;Selected=0;BuildEntries();
 bOpen=true;bHadCursor=PC->bShowMouseCursor;AddToViewport(35);Refresh();
 FInputModeUIOnly Input;Input.SetWidgetToFocus(TakeWidget());PC->SetInputMode(Input);PC->bShowMouseCursor=true;PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);SetKeyboardFocus();
}
void UTallowInventoryWidget::BuildEntries()
{
 Entries.Reset(); using M=TallowPaper::EMark;
 auto Add=[this](const FString& N,const FString& D,M Mark){Entries.Add({FText::FromString(N),FText::FromString(TallowInputPrompts::Expand(this,D)),int32(Mark),NAME_None});};
 if(const auto* C=Player->GetCandleComponent();C && C->IsAcquired())
 {
  const TCHAR* Wax=C->GetWaxFraction()>.65f?TEXT("full"):C->GetWaxFraction()>.35f?TEXT("medium"):C->GetWaxFraction()>.15f?TEXT("low"):TEXT("almost gone");
  const FString Description=C->GetWaxFraction()<=0.f?TEXT("Burned down to the holder.\nFind a spare candle."):
   FString::Printf(TEXT("wax: %s\n\n"),Wax)+(C->GetCandleState()==ECandleState::Out?TEXT("The wick is black.\n[ {Relight} ]  strike a match"):TEXT("[ {Cup} ]  shield the flame\n[ {Relight} ]  put it out\n\nShielding it saves wax."));
  Add(TEXT("the candle"),Description,M::Candle);Entries.Last().Amount=C->GetCandleState()==ECandleState::Out?0.f:1.f;
  Add(TEXT("matches"),FString::Printf(TEXT("%d left in the box.\n\nA struck match is loud.\nChoose your moment."),C->GetMatchCount()),M::Match);
 }
 for(const auto Item:{ECarriedItemType::Crowbar,ECarriedItemType::Fuse,ECarriedItemType::FilmReel})
 {
  const int32 Count=Player->GetInventoryCount(Item);if(Count<=0)continue;
  const bool Held=Player->GetCarriedItem()==Item;
  const FString Name=Item==ECarriedItemType::Crowbar?TEXT("crowbar"):Item==ECarriedItemType::FilmReel?TEXT("film reel"):FString::Printf(TEXT("fuses (%d)"),Count);
  const FString Purpose=Item==ECarriedItemType::Crowbar?TEXT("Cold iron for the nailed boards."):Item==ECarriedItemType::FilmReel?TEXT("A little metal wheel of film. The projector will show what the release needs."):TEXT("Old wire behind cloudy glass. For the fuse box.");
  Entries.Add({FText::FromString(Name),FText::FromString(TallowInputPrompts::Expand(this,Purpose+(Held?TEXT("\n\nin your hand\n[ {Confirm} ]  put in your pocket"):TEXT("\n\nin your pocket\n[ {Confirm} ]  take in hand")))),int32(Item==ECarriedItemType::Crowbar?M::Crowbar:Item==ECarriedItemType::FilmReel?M::Reel:M::Fuse),NAME_None,Item});
 }
 if(auto* Map=Player->FindComponentByClass<UTallowHouseMapComponent>();Map&&Map->HasMap())Entries.Add({FText::FromString(TEXT("my map")),FText::FromString(TallowInputPrompts::Expand(this,TEXT("A crooked plan, in someone else's pencil.\n\n[ {Confirm} / {Map} ]  unfold it\n\nCrosses mark the bolts you have found.\nThe little notes are places to try."))),int32(M::Map),TEXT("house-map")});
 if(const auto* Notes=UTallowNellNotesComponent::Find(this);Notes&&Notes->HasNotes())Entries.Add({FText::FromString(TEXT("my notes")),Notes->GetSummary(),int32(M::Book),TEXT("notes")});
 if(Entries.IsEmpty())Add(TEXT("empty hands"),TEXT("Nothing in your pockets yet."),M::Breath);
 if(const auto* Story=UTallowStorySubsystem::GetStory(this))
 for(const FName Key:TallowNarrative::Keys()) if(Story->HasRead(Key))
 {
  const auto Pages=TallowNarrative::Pages(Key);
  Entries.Add({TallowNarrative::Title(Key),Pages.IsEmpty()?FText::GetEmpty():Pages[0],int32(TallowPaper::PaperMark(Key)),Key});
 }
}
void UTallowInventoryWidget::Close()
{
 if(!bOpen)return;bOpen=false;RemoveFromParent();if(auto* PC=InputOwner.Get()){PC->SetInputMode(FInputModeGameOnly());PC->bShowMouseCursor=bHadCursor;PC->SetIgnoreMoveInput(false);PC->SetIgnoreLookInput(false);}
}
FReply UTallowInventoryWidget::NativeOnKeyDown(const FGeometry&,const FKeyEvent& E)
{
 const FKey K=E.GetKey();const ETallowPadIntent Pad=TallowUIInput::ClassifyGamepad(K);
 if(K==EKeys::I||K==EKeys::Tab||K==EKeys::Escape||Pad==ETallowPadIntent::Back||Pad==ETallowPadIntent::Tertiary||Pad==ETallowPadIntent::Menu)Close();
 else if(K==EKeys::Enter||K==EKeys::E||Pad==ETallowPadIntent::Confirm)ReadSelected();
 else if(!Entries.IsEmpty())
 {
  int32 Step=0;if(K==EKeys::Left||K==EKeys::A||Pad==ETallowPadIntent::Left)Step=-1;else if(K==EKeys::Right||K==EKeys::D||Pad==ETallowPadIntent::Right)Step=1;
  else if(K==EKeys::Up||K==EKeys::W||Pad==ETallowPadIntent::Up)Step=-4;else if(K==EKeys::Down||K==EKeys::S||Pad==ETallowPadIntent::Down)Step=4;
  if(Step){Selected=FMath::Clamp(Selected+Step,0,Entries.Num()-1);Refresh();}
 }
 return FReply::Handled();
}
TSharedRef<SWidget> UTallowInventoryWidget::RebuildWidget()
{
 SAssignNew(ItemSlots,SVerticalBox);
 TSharedRef<SWidget> Root=SNew(SOverlay)
 +SOverlay::Slot()[SNew(SBorder).BorderImage(TallowPaper::Ground()).BorderBackgroundColor(FLinearColor(1.f,1.f,1.f,.94f))]
 +SOverlay::Slot().Padding(46).HAlign(HAlign_Center).VAlign(VAlign_Center)
 [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
 [SNew(SBox).WidthOverride(1200).HeightOverride(720)
 [SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[SNew(STextBlock).Text(FText::FromString(TEXT("WHAT YOU CARRY"))).Font(TallowPaper::Caps(TallowPaper::Pt::Label)).ColorAndOpacity(TallowPaper::Pencil)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,40)[SNew(STextBlock).Text(FText::FromString(TEXT("Your things"))).Font(TallowPaper::Fell(TallowPaper::Pt::Heading)).ColorAndOpacity(TallowPaper::Ivory)]
  +SVerticalBox::Slot().FillHeight(1)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().FillWidth(.62f).Padding(0,0,56,0)
   [SAssignNew(ItemScroll,SScrollBox).Style(TallowPaper::ScrollArea()).ScrollBarStyle(TallowPaper::ScrollBar()).ScrollBarThickness(FVector2D(3,3))+SScrollBox::Slot()[ItemSlots.ToSharedRef()]]
   +SHorizontalBox::Slot().FillWidth(.38f)[SAssignNew(Detail,SVerticalBox)]
  ]
  +SVerticalBox::Slot().AutoHeight().Padding(0,30,0,0)[SNew(SBox).HeightOverride(1)[SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(TallowPaper::Pencil.CopyWithNewOpacity(.22f))]]
  +SVerticalBox::Slot().AutoHeight().Padding(0,18,0,0)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().FillWidth(1)[SNew(STallowKeys).Text_Lambda([this](){return FString(UTallowInputDeviceSubsystem::GetCurrent(this)==ETallowInputDevice::Gamepad?TEXT("[ d-pad ]  choose      [ cross ]  use or listen"):TEXT("[ arrows ]  choose      [ enter ]  use or listen"));}).Font(TallowPaper::FellItalic(TallowPaper::Pt::Small)).Colour(TallowPaper::Pencil).KeyColour(TallowPaper::Ink)]
   +SHorizontalBox::Slot().AutoWidth()[SNew(STallowKeys).Text_Lambda([this](){return FString(UTallowInputDeviceSubsystem::GetCurrent(this)==ETallowInputDevice::Gamepad?TEXT("[ triangle ]  put it all away"):TEXT("[ I ]  put it all away"));}).Font(TallowPaper::FellItalic(TallowPaper::Pt::Small)).Colour(TallowPaper::Ink).KeyColour(TallowPaper::Ivory)]]
 ]]];
 Refresh();return Root;
}
void UTallowInventoryWidget::Refresh()
{
 if(ItemSlots.IsValid())
 {
  ItemSlots->ClearChildren();
  for(int32 Row=0;Row<FMath::DivideAndRoundUp(Entries.Num(),4);++Row)
  {
   auto Grid=SNew(SHorizontalBox);
   for(int32 Col=0;Col<4;++Col)
   {
    const int32 I=Row*4+Col;
    if(!Entries.IsValidIndex(I)){Grid->AddSlot().FillWidth(1).Padding(0,0,14,20)[SNew(SBox)];continue;}
    Grid->AddSlot().FillWidth(1).Padding(0,0,14,20)
    [SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).ContentPadding(0)
     .OnClicked_Lambda([this,I](){Selected=I;Refresh();return FReply::Handled();})
     [SNew(SBox).HeightOverride(184)
      [SNew(SVerticalBox)
       // No tile behind the mark: it is cut straight into the ground. The chosen one is in ink, the
       // rest sit back in dust, and the red thread runs between the chosen mark and its name.
       +SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(120).HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(STallowMark).Mark(TallowPaper::EMark(Entries[I].Mark)).Amount(Entries[I].Amount).Size(108).Colour(Selected==I?TallowPaper::Ink:TallowPaper::Pencil.CopyWithNewOpacity(.7f))]]
       +SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(8)
        [Selected==I?StaticCastSharedRef<SWidget>(SNew(STallowThread).Seed(31+I)):StaticCastSharedRef<SWidget>(SNew(SBox).VAlign(VAlign_Center)[SNew(SBox).HeightOverride(1)[SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(TallowPaper::Pencil.CopyWithNewOpacity(.16f))]])]]
       +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[SNew(SBox).HeightOverride(48)[SNew(STextBlock).Text(Entries[I].Name).Font(TallowPaper::Fell(TallowPaper::Pt::Small+1)).ColorAndOpacity(Selected==I?TallowPaper::Ivory:TallowPaper::Pencil).WrapTextAt(150.f).LineHeightPercentage(.95f)]]
      ]
     ]
    ];
   }
   ItemSlots->AddSlot().AutoHeight()[Grid];
   if(ItemScroll.IsValid()&&Selected/4==Row)ItemScroll->ScrollDescendantIntoView(Grid,false);
  }
 }
 if(!Detail.IsValid()||!Entries.IsValidIndex(Selected))return;
 Detail->ClearChildren();const FEntry& E=Entries[Selected];
 Detail->AddSlot().AutoHeight().Padding(0,0,0,18)[SNew(STextBlock).Text(E.Name).Font(TallowPaper::Fell(TallowPaper::Pt::Heading-8)).ColorAndOpacity(TallowPaper::Ivory).AutoWrapText(true)];
 Detail->AddSlot().AutoHeight().Padding(0,0,0,26)[SNew(SBox).HeightOverride(1)[SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(TallowPaper::Pencil.CopyWithNewOpacity(.22f))]];
 // Line by line, so a line with keys in it is set with its key caps.
 auto Lines=SNew(SVerticalBox);
 TArray<FString> Parts;E.Description.ToString().ParseIntoArray(Parts,TEXT("\n"),false);
 for(const FString& Line:Parts)
 {
  if(Line.TrimStartAndEnd().IsEmpty()){Lines->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(16)];continue;}
  if(Line.Contains(TEXT("[")))Lines->AddSlot().AutoHeight().Padding(0,2,0,4)[SNew(STallowKeys).Text(Line.TrimStartAndEnd()).Font(TallowPaper::FellItalic(TallowPaper::Pt::Body)).Colour(TallowPaper::Ink).KeyColour(TallowPaper::Ivory)];
  else Lines->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Line)).AutoWrapText(true).Font(TallowPaper::Fell(TallowPaper::Pt::Body)).ColorAndOpacity(TallowPaper::Ink).LineHeightPercentage(1.18f)];
 }
 Detail->AddSlot().FillHeight(1)[SNew(SScrollBox).Style(TallowPaper::ScrollArea()).ScrollBarStyle(TallowPaper::ScrollBar()).ScrollBarThickness(FVector2D(3,3))+SScrollBox::Slot()[Lines]];
 if(!E.Document.IsNone()||E.Item!=ECarriedItemType::None)Detail->AddSlot().AutoHeight().Padding(0,20,0,0)[SNew(SButton).OnClicked_Lambda([this](){ReadSelected();return FReply::Handled();}).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).HAlign(HAlign_Left).ContentPadding(0)[SNew(STallowKeys).Text(FString(E.Item!=ECarriedItemType::None?TEXT("[ enter ]  take it in hand, or put it back"):E.Document==TEXT("house-map")?TEXT("[ enter ]  unfold it"):E.Document==TEXT("notes")?TEXT("[ enter ]  read it"):TEXT("[ enter ]  play it again"))).Font(TallowPaper::FellItalic(TallowPaper::Pt::Body)).Colour(TallowPaper::Ivory).KeyColour(TallowPaper::Ivory)]];
}

void UTallowInventoryWidget::ReleaseSlateResources(bool B){Super::ReleaseSlateResources(B);Detail.Reset();ItemSlots.Reset();ItemScroll.Reset();}

void UTallowInventoryWidget::ReadSelected()
{
 if(!Entries.IsValidIndex(Selected))return;
 const auto Item=Entries[Selected].Item;
 if(Item!=ECarriedItemType::None && Player.IsValid())
 {
  if(Player->EquipInventoryItem(Player->GetCarriedItem()==Item?ECarriedItemType::None:Item)){BuildEntries();Refresh();}
  return;
 }
 if(Entries[Selected].Document==TEXT("house-map")){Close();if(Player.IsValid())if(auto* Map=Player->FindComponentByClass<UTallowHouseMapComponent>())Map->ToggleMap();return;}
 if(Entries[Selected].Document.IsNone())return;
 const FName Key=Entries[Selected].Document;auto* PC=GetOwningPlayer();if(!PC)return;
 Close();Reader=CreateWidget<UTallowJournalWidget>(PC);
	const auto* Notes=Key==TEXT("notes")?UTallowNellNotesComponent::Find(this):nullptr;
	Reader->Open(PC,Notes?Notes->GetNotebookPages():TallowNarrative::Pages(Key),TallowNarrative::Printed(Key),Key);
}

void UTallowInventoryWidget::NativeDestruct()
{
 Close();Super::NativeDestruct();
}

#if WITH_EDITOR
void UTallowInventoryWidget::ConfigureReview()
{
 using M=TallowPaper::EMark;
 Entries={
  {FText::FromString(TEXT("the candle")),FText::FromString(TEXT("wax: full\n\n[ right mouse ]  shield the flame\n[ R ]  put it out\n\nShielding it saves wax.")),int32(M::Candle),NAME_None},
  {FText::FromString(TEXT("matches")),FText::FromString(TEXT("Six left in the box. A struck match is loud.")),int32(M::Match),NAME_None},
  {FText::FromString(TEXT("crowbar")),FText::FromString(TEXT("Cold iron for the nailed boards.")),int32(M::Crowbar),NAME_None},
  {FText::FromString(TEXT("fuses (1)")),FText::FromString(TEXT("Old wire behind cloudy glass.")),int32(M::Fuse),NAME_None},
  {TallowNarrative::Title(TEXT("journal")),TallowNarrative::Pages(TEXT("journal"))[0],int32(M::Book),TEXT("journal")},
  {FText::FromString(TEXT("my map")),FText::FromString(TEXT("A crooked plan, in someone else's pencil.")),int32(M::Map),TEXT("house-map")}
 };
 Selected=0;
 TakeWidget();Refresh();
}
#endif
