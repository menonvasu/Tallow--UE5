#include "TallowInteractWidget.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/Presentation/TallowPaper.h"
#include "Input/TallowInputPrompts.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Layout/SBox.h"

void UTallowInteractWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	SetVisibility(ESlateVisibility::Collapsed);
}

void UTallowInteractWidget::SetHoldProgress(float InPercent)
{
	Progress = FMath::Clamp(InPercent, 0.0f, 1.0f);
}

void UTallowInteractWidget::ShowPrompt(const FText& ActionText, const bool bIsHold, const bool bCanInteract)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	Label = ActionText;
	bHold = bIsHold;
	bCanAct = bCanInteract;
}

void UTallowInteractWidget::HidePrompt()
{
	// Collapse UI and reset progress
	SetVisibility(ESlateVisibility::Collapsed);
	Progress = 0.f;
}

TSharedRef<SWidget> UTallowInteractWidget::RebuildWidget()
{
	// Keep the designer tree built (its widgets are still bound) but paint the prompt ourselves.
	Super::RebuildWidget();
	return SNew(SBox);
}

int32 UTallowInteractWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, Geo, MyCullingRect, Out, LayerId, InWidgetStyle, bParentEnabled);
	if (Label.IsEmpty()) return Layer;
	using namespace TallowPaper;
	// Lower case, like the journal. No card: the words sit on a pool of soot so they hold over a lit wall.
	const FString Text = Label.ToString().ToLower();
	const FVector2D Screen = Geo.GetLocalSize();
	const FVector2D Centre(Screen.X * .5f, Screen.Y * .62f);
	if (!bCanAct)
	{
		// Not yet: no key to press. The words in dust, a short red stitch before them.
		const FSlateFontInfo Font = FellItalic(Pt::Body);
		const FVector2D Words = Measure(Text, Font);
		const float Stitch = 26.f;
		const FVector2D Box(Words.X + Stitch + 12.f, Words.Y);
		DrawSmudge(Out, ++Layer, Geo, Centre, Box, .9f);
		const float Left = Centre.X - Box.X * .5f;
		DrawThread(Out, ++Layer, Geo, FVector2D(Left, Centre.Y + 1.f), FVector2D(Left + Stitch, Centre.Y), 13);
		Layer += 2;
		DrawText(Out, ++Layer, Geo, Text, Font, FVector2D(Left + Stitch + 12.f + Words.X * .5f, Centre.Y), Ink);
		return Layer + 2;
	}
	const FString KeyWord = TallowInputPrompts::GetCurrent(this, ETallowPromptAction::Interact).Label.ToString();
	const FString Line = FString::Printf(TEXT("[ %s ]  %s"), *KeyWord, *Text);
	const FSlateFontInfo Font = FellItalic(Pt::Lead);
	const FVector2D Box = MeasureKeys(Line, Font);
	DrawSmudge(Out, ++Layer, Geo, Centre, Box, .9f);
	const float Left = Centre.X - Box.X * .5f;
	DrawKeys(Out, ++Layer, Geo, Line, Font, FVector2D(Left, Centre.Y), Ivory, Ivory);
	Layer += 3;
	if (bHold && Progress > 0.f)
	{
		// A hold fills a ring round the key, in the red thread.
		const float Cap = FMath::RoundToFloat(Font.Size * 1.45f);
		const float KeyWidth = FMath::Max(Cap, Measure(KeyWord, Fell(FMath::RoundToInt(Font.Size * .78f))).X + Font.Size * .9f);
		DrawRing(Out, ++Layer, Geo, FVector2D(Left + Font.Size * .1f + KeyWidth * .5f, Centre.Y), FMath::Max(KeyWidth, Cap) * .5f + 7.f, Progress, Thread, 2.5f, 7);
	}
	return Layer + 2;
}
