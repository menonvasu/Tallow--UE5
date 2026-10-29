#include "UI/TallowChairEscapeWidget.h"
#include "Tasks/TallowChairEscape.h"
#include "UI/Presentation/TallowPaper.h"
#include "Input/TallowInputPrompts.h"

void UTallowChairEscapeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

int32 UTallowChairEscapeWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, Geo, MyCullingRect, Out, LayerId, InWidgetStyle, bParentEnabled);
	if (!Chair) return Layer;

	const double Now = FPlatformTime::Seconds();
	const int32 Rocks = Chair->GetRockCount();
	const int32 Required = FMath::Max(1, Chair->GetRequiredRocks());
	if (Rocks == 0 && LastRockCount > 0 && !Chair->IsAtRest()) MissFlashUntil = Now + 1.2;
	LastRockCount = Rocks;

	const FLinearColor Panel(0.f, 0.f, 0.f, .72f), Track(.18f, .18f, .18f, 1.f), Dim(.45f, .08f, .08f, 1.f), Live(.2f, .95f, .35f, 1.f), Miss(.95f, .15f, .12f, 1.f);
	const FVector2D Screen = Geo.GetLocalSize();
	const FVector2D PanelSize(620.f, 190.f);
	const FVector2D PanelPos((Screen.X - PanelSize.X) * .5f, Screen.Y - PanelSize.Y - 60.f);
	DrawBox(Out, ++Layer, Geo, &Solid, PanelPos, PanelSize, Panel);

	const float CentreX = PanelPos.X + PanelSize.X * .5f;
	DrawLabel(Out, ++Layer, Geo, TEXT("TIED TO THE CHAIR"), 14, FVector2D(CentreX, PanelPos.Y + 22.f), FLinearColor(.7f, .7f, .7f, 1.f));

	// Track: left is tipped back, right is tipped forward. Rocks only land from the back of the swing.
	const float TrackWidth = 520.f, TrackY = PanelPos.Y + 62.f;
	const float TrackLeft = CentreX - TrackWidth * .5f;
	DrawBox(Out, ++Layer, Geo, &Solid, FVector2D(TrackLeft, TrackY), FVector2D(TrackWidth, 18.f), Track);
	const bool bOpen = Chair->IsTimingWindowOpen();
	const float ZoneRight = CentreX - TrackWidth * .5f * Chair->GetWindowThreshold();
	DrawBox(Out, ++Layer, Geo, &Solid, FVector2D(TrackLeft, TrackY), FVector2D(ZoneRight - TrackLeft, 18.f), bOpen ? Live : Dim);
	DrawBox(Out, ++Layer, Geo, &Solid, FVector2D(CentreX - 1.f, TrackY - 6.f), FVector2D(2.f, 30.f), FLinearColor(.6f, .6f, .6f, 1.f));
	DrawLabel(Out, Layer, Geo, TEXT("BACK"), 11, FVector2D(TrackLeft + 24.f, TrackY + 34.f), FLinearColor(.6f, .6f, .6f, 1.f));
	DrawLabel(Out, Layer, Geo, TEXT("FORWARD"), 11, FVector2D(TrackLeft + TrackWidth - 36.f, TrackY + 34.f), FLinearColor(.6f, .6f, .6f, 1.f));

	const float MarkerX = CentreX + Chair->GetNormalizedSwing() * TrackWidth * .5f;
	DrawBox(Out, ++Layer, Geo, &Solid, FVector2D(MarkerX - 6.f, TrackY - 12.f), FVector2D(12.f, 42.f), FLinearColor::White);

	// Progress pips
	const float PipSize = 18.f, PipGap = 12.f;
	const float PipsLeft = CentreX - (Required * PipSize + (Required - 1) * PipGap) * .5f;
	for (int32 I = 0; I < Required; ++I)
	{
		DrawBox(Out, Layer, Geo, &Solid, FVector2D(PipsLeft + I * (PipSize + PipGap), PanelPos.Y + 112.f), FVector2D(PipSize, PipSize), I < Rocks ? Live : Track);
	}

	FString Prompt = TEXT("Wait for the swing back, then press SPACE");
	FLinearColor PromptColour = FLinearColor::White;
	if (Now < MissFlashUntil) { Prompt = TEXT("Too early. The chair creaked."); PromptColour = Miss; }
	else if (Chair->IsAtRest()) Prompt = TEXT("Press SPACE to start rocking");
	else if (bOpen) { Prompt = TEXT("NOW! Press SPACE"); PromptColour = Live; }
	DrawLabel(Out, ++Layer, Geo, Prompt, 18, FVector2D(CentreX, PanelPos.Y + 162.f), PromptColour);
	return Layer;
}
