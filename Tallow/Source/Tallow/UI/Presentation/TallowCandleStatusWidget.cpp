#include "UI/Presentation/TallowCandleStatusWidget.h"
#include "Candle/TallowCandleComponent.h"
#include "UI/Presentation/TallowPaper.h"
#include "UI/Presentation/STallowMark.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Rendering/DrawElements.h"

// The candle as it stands: one engraved plate per wax stage, the flame or the smoke drawn in.
class SCandleWax : public SLeafWidget
{
public:
 SLATE_BEGIN_ARGS(SCandleWax){} SLATE_ATTRIBUTE(FName,Plate) SLATE_END_ARGS()
 void Construct(const FArguments& A){Plate=A._Plate;}
 virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(40,65);}
 virtual bool ComputeVolatility()const override{return true;}
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& O,int32 L,const FWidgetStyle&,bool)const override
 {
  const auto* Brush=TallowPaper::Art(Plate.Get());
  FSlateDrawElement::MakeBox(O,L,G.ToPaintGeometry(G.GetLocalSize(),FSlateLayoutTransform()),Brush,ESlateDrawEffect::None,TallowPaper::Ink);
  return L+1;
 }
private:TAttribute<FName>Plate;
};

UTallowCandleStatusWidget::UTallowCandleStatusWidget(const FObjectInitializer& O):Super(O)
{SetVisibility(ESlateVisibility::HitTestInvisible);SetIsFocusable(false);}

#if WITH_EDITOR
void UTallowCandleStatusWidget::ConfigureReview(ETallowCandleFuelStage InStage, bool bLit)
{bReview=true;ReviewStage=InStage;bReviewLit=bLit;}
#endif

// The wax plate only means something while the candle burns down (off since 31 Oct), so it stays hidden.
bool UTallowCandleStatusWidget::IsShown() const {return bReview||(Candle.IsValid()&&Candle->bBurnsDown&&Candle->IsAcquired()&&!Candle->IsGivenUp());}
bool UTallowCandleStatusWidget::IsLit() const {return bReview?bReviewLit:Candle.IsValid()&&Candle->GetCandleState()!=ECandleState::Out;}
ETallowCandleFuelStage UTallowCandleStatusWidget::Stage() const {return bReview?ReviewStage:Candle.IsValid()?Candle->GetFuelStage():ETallowCandleFuelStage::Out;}
TSharedRef<SWidget> UTallowCandleStatusWidget::RebuildWidget()
{
 auto Plate=[this]()->FName
 {
  if(!IsLit())return TEXT("WaxOut");
  switch(Stage()){case ETallowCandleFuelStage::Full:return TEXT("WaxFull");case ETallowCandleFuelStage::Medium:return TEXT("WaxMedium");case ETallowCandleFuelStage::Low:return TEXT("WaxLow");case ETallowCandleFuelStage::Danger:return TEXT("WaxGutter");default:return TEXT("WaxOut");}
 };
 // Bottom left on a pool of soot, no card: the wax plate, and two quiet words.
 return SNew(SOverlay)+SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(40,0,0,30)
 [SNew(SOverlay).Visibility_Lambda([this](){return IsShown()?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
 +SOverlay::Slot()[SNew(STallowSmudge).Alpha(.85f)]
 +SOverlay::Slot().Padding(FMargin(10,6,18,6))
 [SNew(SHorizontalBox)
 +SHorizontalBox::Slot().AutoWidth()[SNew(SCandleWax).Plate_Lambda(Plate)]
 +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10,0)
 [SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("CANDLE"))).Font(TallowPaper::Caps(TallowPaper::Pt::Label-2,300)).ColorAndOpacity(TallowPaper::Pencil)]
  +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(TallowPaper::FellItalic(TallowPaper::Pt::Small)).ColorAndOpacity_Lambda([this](){return IsLit()&&Stage()==ETallowCandleFuelStage::Danger?TallowPaper::Blood:TallowPaper::Ink;})
 .Text_Lambda([this](){
  const TCHAR* Text=TEXT("out");
  if(IsLit())switch(Stage()){
   case ETallowCandleFuelStage::Full:Text=TEXT("full");break;case ETallowCandleFuelStage::Medium:Text=TEXT("half gone");break;
   case ETallowCandleFuelStage::Low:Text=TEXT("low");break;case ETallowCandleFuelStage::Danger:Text=TEXT("guttering");break;default:break;}
  return FText::FromString(Text);
 })]]
 ]];
}
