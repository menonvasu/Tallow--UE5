#include "Player/Stealth/TallowStealthFeedbackWidget.h"
#include "Player/Stealth/TallowStealthCharacter.h"
#include "UI/Presentation/TallowPaper.h"
#include "Widgets/Layout/SBox.h"
TSharedRef<SWidget> UTallowStealthFeedbackWidget::RebuildWidget()
{ SetVisibility(ESlateVisibility::HitTestInvisible); SetIsFocusable(false); return SNew(SBox); }
int32 UTallowStealthFeedbackWidget::NativePaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& R,FSlateWindowElementList& O,int32 L,const FWidgetStyle& W,bool B) const
{
 L=Super::NativePaint(A,G,R,O,L,W,B); if(!Character.IsValid())return L;
 using namespace TallowPaper;
 const FVector2D Size=G.GetLocalSize();const float S=FMath::Clamp(float(Size.Y/900),.6f,1.4f);
 const bool Hold=Character->IsHoldingBreath(); const float Air=Character->GetBreathFraction();
 if(Hold || Air<.7f)
 {
  const FVector2D At(Size.X*.5f,Size.Y-95*S);const FLinearColor Colour=Air<.24f?Blood:Ink;
  DrawMark(O,++L,G,EMark::Breath,At,32*S,Colour);
  DrawRing(O,++L,G,At,32*S,Air,Colour,1.6f,19);
  DrawText(O,++L,G,Hold?TEXT("hold it in"):TEXT("breathe..."),Hand(FMath::RoundToInt(14*S)),At+FVector2D(0,48*S),Colour);L+=2;
 }
 if(Character->GetStealthTraversalMode()!=ETallowPlayerTraversalMode::None)
 {
  DrawText(O,++L,G,Character->GetStealthTraversalMode()==ETallowPlayerTraversalMode::Squeezing?TEXT("a little further..."):TEXT("pull yourself up"),Hand(FMath::RoundToInt(15*S)),FVector2D(Size.X*.5,Size.Y*.72),Pencil);L+=2;
 }
 return L+2;
}
