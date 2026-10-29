#include "UI/Presentation/TallowHouseMapWidget.h"
#include "Player/Map/TallowHouseMapComponent.h"
#include "House/TallowRoomSearchSubsystem.h"
#include "Input/TallowInputPrompts.h"
#include "Environment/TallowDoor.h"
#include "Environment/TallowHouseRunLayout.h"
#include "EngineUtils.h"
#include "UI/Presentation/TallowPaper.h"
#include "UI/Presentation/STallowMark.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h"
#include "Widgets/Layout/SScrollBox.h"

namespace { const FLinearColor MapPencil=TallowPaper::Ink; const FLinearColor MapRed=TallowPaper::Blood; }

class SHouseScribble : public SLeafWidget
{
public:
 SLATE_BEGIN_ARGS(SHouseScribble):_Review(false){} SLATE_ARGUMENT(TWeakObjectPtr<UTallowHouseMapComponent>,Map) SLATE_ATTRIBUTE(int32,Floor) SLATE_ARGUMENT(bool,Review) SLATE_END_ARGS()
 void Construct(const FArguments&A){Map=A._Map;Floor=A._Floor;bReview=A._Review;}
 virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(660,530);}
 virtual bool ComputeVolatility()const override{return true;}
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry&G,const FSlateRect&,FSlateWindowElementList&O,int32 L,const FWidgetStyle&,bool)const override
 {
  if(!Map.IsValid()&&!bReview)return L;
  const auto Point=[](const FVector2D& P){return FVector2D(45.f+(P.X+100.f)*.31f,30.f+(200.f-P.Y)*.31f);};
  auto Line=[&](FVector2D A,FVector2D B,FLinearColor Ink,float Width=1.2f,bool Dashed=false)
  {
   const int32 N=FMath::Max(2,FMath::CeilToInt((B-A).Size()/13.f));
   for(int32 I=0;I<N-1;++I)
   {
    if(Dashed && I%2)continue;
    const auto P=[&](int32 J){const float T=float(J)/float(N-1);return FMath::Lerp(A,B,T)+FVector2D(FMath::Sin(J*2.6f+A.Y)*.85f,FMath::Cos(J*2.1f+A.X)*.85f);};
    TArray<FVector2D>V{P(I),P(I+1)};FSlateDrawElement::MakeLines(O,L,G.ToPaintGeometry(),V,ESlateDrawEffect::None,Ink,true,Width);
   }
  };
  auto Text=[&](const FString& S,FVector2D At,int32 Size,FLinearColor Ink,bool Center=false)
  {
   const auto Font=TallowPaper::Child(Size+6);if(Center)At-=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S,Font)*.5f;
   FSlateDrawElement::MakeText(O,L+2,G.ToPaintGeometry(FVector2f(G.GetLocalSize()),FSlateLayoutTransform(FVector2f(At))),S,Font,ESlateDrawEffect::None,Ink);
  };
  const TCHAR* PlanNames[]={TEXT("PlanCellar"),TEXT("PlanGround"),TEXT("PlanUpper")};
  const FSlateBrush* Plan=TallowPaper::Art(PlanNames[FMath::Clamp(Floor.Get(),0,2)]);
  FSlateDrawElement::MakeBox(O,L,G.ToPaintGeometry(FVector2D(660,530),FSlateLayoutTransform()),Plan,ESlateDrawEffect::None,FLinearColor(1,1,1,.13f));
  for(const auto& Room:UTallowHouseMapComponent::Rooms())
  {
   if(Room.Floor!=Floor.Get()||(!bReview&&!Map->KnowsRoom(Room.Id)))continue;
   const FVector2D A=Point({Room.Min.X,Room.Max.Y})-FVector2D(12,12),B=Point({Room.Max.X,Room.Min.Y})+FVector2D(12,12);
   O.PushClip(FSlateClippingZone(G.MakeChild(B-A,FSlateLayoutTransform(A))));
   FSlateDrawElement::MakeBox(O,L+1,G.ToPaintGeometry(FVector2D(660,530),FSlateLayoutTransform()),Plan,ESlateDrawEffect::None,FLinearColor::White);
   O.PopClip();
  }
  for(const auto& R:UTallowHouseMapComponent::Rooms())
  {
   if(R.Floor!=Floor.Get())continue;
   const bool Known=bReview||Map->KnowsRoom(R.Id);const FVector2D A=Point({R.Min.X,R.Max.Y})+FVector2D(4,4),B=Point({R.Max.X,R.Min.Y})-FVector2D(4,4);
   FLinearColor Ink=TallowPaper::Ink;Ink.A=Known?.95f:.32f;
   
   Text(Known?R.Label:TEXT("?"),(A+B)*.5f,Known?16:24,Ink,true);
   if(Map.IsValid())if(const auto* Search=UTallowRoomSearchSubsystem::Get(Map.Get());Search&&Search->IsRunning()&&Search->GetRoomState(R.Id).bSearched)
    for(int32 I=0;I<3;++I)Line(FVector2D(B.X-24+I*6,A.Y+16),FVector2D(B.X-18+I*6,A.Y+6),MapPencil,1.2f);
   if(Map.IsValid()&&R.Id==Map->GetCurrentRoom())
   {
    const FVector2D Here((A.X+B.X)*.5f,B.Y-24);TArray<FVector2D>Ring;
    for(int32 I=0;I<=16;++I){const float T=I*2.f*PI/16.f;Ring.Add(Here+FVector2D(FMath::Cos(T)*7,FMath::Sin(T)*6));}
    FSlateDrawElement::MakeLines(O,L+3,G.ToPaintGeometry(),Ring,ESlateDrawEffect::None,MapRed,true,2.f);
    Text(TEXT("me"),Here+FVector2D(12,-8),13,MapRed);
   }
  }
  TMap<const ATallowDoor*,FTallowRunDoorState> DoorStates;
  if(Map.IsValid())for(TActorIterator<ATallowHouseRunLayout> It(Map->GetWorld());It;++It)for(const auto& State:It->GetDoorStates())if(State.Door)DoorStates.Add(State.Door,State);
  if(Map.IsValid())for(const auto& Ref:Map->GetKnownDoors())if(const auto* D=Ref.Get())
  {
   const auto* State=DoorStates.Find(D);
   const FVector P=State?State->DoorwayPosition:D->GetActorLocation();const int32 F=P.Z<-100.f?0:P.Z<300.f?1:2;
   if(F!=Floor.Get())continue;const FVector2D At=Point({P.X,P.Y});
   if(State&&State->bBarricaded){for(int32 I=0;I<3;++I)Line(At+FVector2D(-9+I*6,-6),At+FVector2D(-3+I*6,6),MapRed,1.8f);}
   else if(State?State->bLocked:D->bIsLocked){Line(At-FVector2D(6,6),At+FVector2D(6,6),MapRed,2.f);Line(At+FVector2D(-6,6),At+FVector2D(6,-6),MapRed,2.f);}
   else {Line(At-FVector2D(5,2),At+FVector2D(0,4),TallowPaper::Ink,1.7f);Line(At+FVector2D(0,4),At+FVector2D(8,-6),TallowPaper::Ink,1.7f);}
  }
  Text(TEXT("not to scale. i drew it from memory."),{25,500},16,TallowPaper::Ink);
  return L+4;
 }
private:TWeakObjectPtr<UTallowHouseMapComponent>Map;TAttribute<int32>Floor;bool bReview=false;
};

UTallowHouseMapWidget::UTallowHouseMapWidget(const FObjectInitializer&O):Super(O){SetIsFocusable(true);}
void UTallowHouseMapWidget::Open(UTallowHouseMapComponent* InMap,APlayerController* PC)
{
 if(bOpen||!InMap||!PC)return;Map=InMap;InputOwner=PC;Floor=InMap->GetCurrentFloor();Age=0.f;CloseAge=-1.f;bOpen=true;bHadCursor=PC->bShowMouseCursor;
 AddToViewport(36);SetRenderOpacity(0.f);PC->FlushPressedKeys();
 FInputModeUIOnly Input;Input.SetWidgetToFocus(TakeWidget());PC->SetInputMode(Input);PC->bShowMouseCursor=true;PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);SetKeyboardFocus();
}
void UTallowHouseMapWidget::ReleaseInput()
{
 if(!bOpen)return;bOpen=false;
 if(auto* PC=InputOwner.Get()){PC->SetInputMode(FInputModeGameOnly());PC->bShowMouseCursor=bHadCursor;PC->SetIgnoreMoveInput(false);PC->SetIgnoreLookInput(false);}
}
void UTallowHouseMapWidget::Close(bool bImmediate)
{
 if(!bOpen)return;
 if(!bImmediate)
 {
  if(CloseAge<0.f)
  {
   CloseAge=0.f;if(Map.IsValid())Map->PlayGesture(true);
   if(GetWorld())GetWorld()->GetTimerManager().SetTimer(FoldTimer,FTimerDelegate::CreateWeakLambda(this,[this](){Close(true);}),.25f,false);
  }
  return;
 }
 if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(FoldTimer);
 ReleaseInput();RemoveFromParent();
}
void UTallowHouseMapWidget::NativeTick(const FGeometry&G,float Dt)
{
 Super::NativeTick(G,Dt);if(!bOpen)return;Age+=Dt;
 if(CloseAge>=0.f){CloseAge+=Dt;if(CloseAge>=.25f){Close(true);return;}}
 const float Open=FMath::SmoothStep(.12f,.55f,Age);const float T=CloseAge>=0.f?1.f-FMath::SmoothStep(0.f,.25f,CloseAge):Open;
 SetRenderOpacity(T);if(Paper.IsValid())Paper->SetRenderTransform(FSlateRenderTransform(FVector2D(0.f,180.f*(1.f-T))));
}
bool UTallowHouseMapWidget::ApplyPadIntent(ETallowPadIntent Intent,int32& ViewedFloor,bool& bClose)
{
 bClose=false;
 if(Intent==ETallowPadIntent::PrevTab||Intent==ETallowPadIntent::Left)ViewedFloor=FMath::Clamp(ViewedFloor-1,0,2);
 else if(Intent==ETallowPadIntent::NextTab||Intent==ETallowPadIntent::Right)ViewedFloor=FMath::Clamp(ViewedFloor+1,0,2);
 else if(Intent==ETallowPadIntent::Back||Intent==ETallowPadIntent::View)bClose=true;
 else return false;
 return true;
}
FReply UTallowHouseMapWidget::NativeOnKeyDown(const FGeometry&,const FKeyEvent&E)
{
 const FKey K=E.GetKey();bool bClose=false;
 if(ApplyPadIntent(TallowUIInput::ClassifyGamepad(K),Floor,bClose)){if(bClose)Close();return FReply::Handled();}
 if(K==EKeys::M||K==EKeys::Escape||K==EKeys::Tab||K==EKeys::I)Close();
 else if(K==EKeys::One)Floor=0;else if(K==EKeys::Two)Floor=1;else if(K==EKeys::Three)Floor=2;
 return FReply::Handled();
}
TSharedRef<SWidget> UTallowHouseMapWidget::RebuildWidget()
{
 using namespace TallowPaper;
 // Laid on the soot like every other page: the plan in the child's pencil, the page's own words in the book face.
 auto Tabs=SNew(SHorizontalBox);
 const TCHAR*Names[]={TEXT("cellar"),TEXT("ground floor"),TEXT("upstairs")};
 for(int32 I=0;I<3;++I)Tabs->AddSlot().AutoWidth().Padding(0,0,34,0)
 [SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).ContentPadding(0)
 .OnClicked_Lambda([this,I](){Floor=I;return FReply::Handled();})
 [SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight()[SNew(STallowKeys).Text(FString::Printf(TEXT("[ %d ]  %s"),I+1,Names[I])).Font(Fell(Pt::Body)).Colour_Lambda([this,I](){return Floor==I?Ivory:Pencil;}).KeyColour_Lambda([this,I](){return Floor==I?Ivory:Pencil;})]
  +SVerticalBox::Slot().AutoHeight().Padding(40,2,0,0)[SNew(SBox).HeightOverride(8)[SNew(STallowThread).Seed(40+I).Shown_Lambda([this,I](){return Floor==I;})]]]];
 auto Page=SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)[SNew(STextBlock).Text(FText::FromString(TEXT("A PLAN OF THE HOUSE"))).Font(Caps(Pt::Label)).ColorAndOpacity(Pencil)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,22)[SNew(STextBlock).Text(FText::FromString(TEXT("my way out"))).Font(Child(Pt::Heading+8)).ColorAndOpacity(Ivory)]
 +SVerticalBox::Slot().AutoHeight()[Tabs]
 +SVerticalBox::Slot().FillHeight(1).Padding(0,18)
 [SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(.7f)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SHouseScribble).Map(Map).Review(bReview).Floor_Lambda([this](){return Floor;})]]
 +SHorizontalBox::Slot().FillWidth(.3f).Padding(36,28,0,0)[SNew(STextBlock).Text_Lambda([this](){return Map.IsValid()?FText::FromString(Map->GetClue()):FText::GetEmpty();}).Font(Child(Pt::Lead+2)).AutoWrapText(true).ColorAndOpacity(Ink)]]
 +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)[SNew(STextBlock).Text(FText::FromString(TEXT("x  locked        ///  boarded        tick  open        ?  not seen yet"))).Font(FellItalic(Pt::Small)).ColorAndOpacity(Pencil)]
 +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1)[SNew(STallowKeys).Text_Lambda([this](){return FString::Printf(TEXT("[ %s ]  [ %s ]  another floor"),*TallowInputPrompts::GetCurrent(this,ETallowPromptAction::FloorPrev).Label.ToString(),*TallowInputPrompts::GetCurrent(this,ETallowPromptAction::FloorNext).Label.ToString());}).Font(FellItalic(Pt::Small)).Colour(Pencil).KeyColour(Ink)]
  +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).OnClicked_Lambda([this](){Close();return FReply::Handled();}).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).ContentPadding(0)
   [SNew(STallowKeys).Text_Lambda([this](){return FString::Printf(TEXT("[ %s ]  fold it away"),*TallowInputPrompts::GetCurrent(this,ETallowPromptAction::Back).Label.ToString());}).Font(FellItalic(Pt::Small)).Colour(Ink).KeyColour(Ivory)]]];
 Paper=Page;
 return SNew(SOverlay)+SOverlay::Slot()[SNew(SBorder).BorderImage(Ground()).BorderBackgroundColor(FLinearColor(1,1,1,.95f))]
 +SOverlay::Slot().Padding(48).HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)[SNew(SBox).WidthOverride(1560).HeightOverride(860)[Page]]];
}
void UTallowHouseMapWidget::NativeDestruct(){if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(FoldTimer);ReleaseInput();Super::NativeDestruct();}
void UTallowHouseMapWidget::ReleaseSlateResources(bool B){Super::ReleaseSlateResources(B);Paper.Reset();}
