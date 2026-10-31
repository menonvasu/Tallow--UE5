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
	// 30 Oct: it fades in once the chair game is ready (after Mim has gone), rather than appearing at once.
	const float Alpha = Chair->GetOverlayAlpha();
	if (Alpha <= 0.f) return Layer;
	auto Fade = [Alpha](const FLinearColor& Colour) { return Colour.CopyWithNewOpacity(Colour.A * Alpha); };

	const double Now = FPlatformTime::Seconds();
	const int32 Rocks = Chair->GetRockCount();
	const int32 Required = FMath::Max(1, Chair->GetRequiredRocks());
	if (Rocks == 0 && LastRockCount > 0 && !Chair->IsAtRest()) MissFlashUntil = Now + 1.2;
	LastRockCount = Rocks;

	// At the foot of the screen on a pool of soot: the swing as a pencil line, the moment to push as the red thread.
	const float UIScale = FMath::Clamp(float(Geo.GetLocalSize().Y / 1080.f), .5f, 1.3f);
	const FGeometry Canvas = Geo.MakeChild(Geo.GetLocalSize() / UIScale, FSlateLayoutTransform(UIScale));
	const FVector2D Screen = Canvas.GetLocalSize();
	const FVector2D PanelSize(600.f, 200.f);
	const FVector2D PanelPos((Screen.X - PanelSize.X) * .5f, Screen.Y - PanelSize.Y - 46.f);
	TallowPaper::DrawSmudge(Out, ++Layer, Canvas, PanelPos + PanelSize * .5f, PanelSize * FVector2D(.86f, .7f), .92f * Alpha);
	const float CentreX = PanelPos.X + PanelSize.X * .5f;
	TallowPaper::DrawText(Out, ++Layer, Canvas, TEXT("TIED TO THE CHAIR"), TallowPaper::Caps(TallowPaper::Pt::Label), FVector2D(CentreX, PanelPos.Y + 34.f), Fade(TallowPaper::Pencil));
	Layer += 2;

	// The swing, as a pencil line: left is tipped back, right tipped forward. A rock only lands from the back.
	const float TrackWidth = 470.f, TrackY = PanelPos.Y + 82.f;
	const float TrackLeft = CentreX - TrackWidth * .5f;
	TallowPaper::DrawLine(Out, ++Layer, Canvas, FVector2D(TrackLeft, TrackY), FVector2D(TrackLeft + TrackWidth, TrackY), Fade(TallowPaper::Pencil.CopyWithNewOpacity(.6f)), 1.5f, 11);
	const bool bOpen = Chair->IsTimingWindowOpen();
	const float ZoneRight = CentreX - TrackWidth * .5f * Chair->GetWindowThreshold();
	// The back of the swing: dust while it waits, the red thread the moment a rock will land.
	if (bOpen) TallowPaper::DrawThread(Out, ++Layer, Canvas, FVector2D(TrackLeft, TrackY), FVector2D(ZoneRight, TrackY), 20, Alpha);
	else TallowPaper::DrawLine(Out, ++Layer, Canvas, FVector2D(TrackLeft, TrackY), FVector2D(ZoneRight, TrackY), Fade(TallowPaper::Pencil), 3.f, 20);
	Layer += 2;
	TallowPaper::DrawLine(Out, ++Layer, Canvas, FVector2D(CentreX, TrackY - 10.f), FVector2D(CentreX, TrackY + 10.f), Fade(TallowPaper::Pencil), 1.5f, 31);
	TallowPaper::DrawText(Out, ++Layer, Canvas, TEXT("back"), TallowPaper::FellItalic(TallowPaper::Pt::Small - 2), FVector2D(TrackLeft + 22.f, TrackY + 26.f), Fade(TallowPaper::Pencil));
	TallowPaper::DrawText(Out, Layer + 2, Canvas, TEXT("forward"), TallowPaper::FellItalic(TallowPaper::Pt::Small - 2), FVector2D(TrackLeft + TrackWidth - 34.f, TrackY + 26.f), Fade(TallowPaper::Pencil));
	Layer += 4;

	// Where the chair is: one firm ink stroke.
	const float MarkerX = CentreX + Chair->GetNormalizedSwing() * TrackWidth * .5f;
	TallowPaper::DrawLine(Out, ++Layer, Canvas, FVector2D(MarkerX, TrackY - 16.f), FVector2D(MarkerX + 2.f, TrackY + 16.f), Fade(TallowPaper::Ivory), 3.5f, 41);

	// Rocks so far: rings, inked in as they land.
	const float Pip = 9.f, PipGap = 30.f;
	const float PipsLeft = CentreX - (Required - 1) * PipGap * .5f;
	for (int32 I = 0; I < Required; ++I)
	{
		const FVector2D At(PipsLeft + I * PipGap, PanelPos.Y + 128.f);
		TallowPaper::DrawRing(Out, ++Layer, Canvas, At, Pip, 1.f, I < Rocks ? Fade(TallowPaper::Ivory) : Fade(TallowPaper::Pencil), I < Rocks ? 2.5f : 1.2f, 50 + I);
		if (I < Rocks) TallowPaper::DrawRing(Out, ++Layer, Canvas, At, Pip * .45f, 1.f, Fade(TallowPaper::Ivory), 4.f, 60 + I);
	}

	// Only the key, never instructions (25 Oct): "space" before the first rock and again, bigger, while the chair is
	// tipped right for the next push. The creak tells you when you were too soon.
	const FString RockKey = TallowInputPrompts::GetCurrent(this, ETallowPromptAction::RockChair).Label.ToString();
	const bool bKey = Now >= MissFlashUntil && ((Rocks == 0 && Chair->IsAtRest()) || (!Chair->IsAtRest() && bOpen));
	if (Now < MissFlashUntil)
		TallowPaper::DrawText(Out, ++Layer, Canvas, TEXT("it creaked."), TallowPaper::FellItalic(TallowPaper::Pt::Body), FVector2D(CentreX, PanelPos.Y + 166.f), Fade(TallowPaper::Blood));
	else if (bKey)
	{
		// The key alone in its cap, a size up while the chair is tipped for the push.
		const FString Key = FString::Printf(TEXT("[ %s ]"), *RockKey);
		const FSlateFontInfo Font = TallowPaper::Fell(bOpen ? TallowPaper::Pt::Lead : TallowPaper::Pt::Body);
		const FVector2D Size = TallowPaper::MeasureKeys(Key, Font);
		TallowPaper::DrawKeys(Out, ++Layer, Canvas, Key, Font, FVector2D(CentreX - Size.X * .5f, PanelPos.Y + 166.f), Fade(TallowPaper::Ivory), bOpen ? Fade(TallowPaper::Ivory) : Fade(TallowPaper::Ink));
		Layer += 2;
	}
	return Layer + 2;
}
