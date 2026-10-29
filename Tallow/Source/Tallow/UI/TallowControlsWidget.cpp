#include "UI/TallowControlsWidget.h"
#include "Player/TallowCharacter.h"
#include "UI/Presentation/TallowPaper.h"
#include "Input/TallowInputPrompts.h"
#include "Input/TallowInputDeviceSubsystem.h"
#include "Candle/TallowCandleComponent.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"

void UTallowControlsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

int32 UTallowControlsWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, Geo, MyCullingRect, Out, LayerId, InWidgetStyle, bParentEnabled);
	if (!Player || Player->IsTiedToChair()) return Layer;
	// Centre dot so you can see what you're aiming at
	const FVector2D Centre = Geo.GetLocalSize() * 0.5f;
	FSlateDrawElement::MakeBox(Out, ++Layer, Geo.ToPaintGeometry(FVector2D(6.f, 6.f), FSlateLayoutTransform(Centre - FVector2D(3.f, 3.f))), &Solid, ESlateDrawEffect::None, FLinearColor(0, 0, 0, 0.6f));
	FSlateDrawElement::MakeBox(Out, ++Layer, Geo.ToPaintGeometry(FVector2D(4.f, 4.f), FSlateLayoutTransform(Centre - FVector2D(2.f, 2.f))), &Solid, ESlateDrawEffect::None, TallowPaper::Ink.CopyWithNewOpacity(.75f));
	const UTallowCandleComponent* Candle = Player->GetCandleComponent();
	const bool bHasCandle = Candle && Candle->IsAcquired();
	const bool bLit = bHasCandle && Candle->GetCandleState() != ECandleState::Out;

	// The keyboard words stay as they were; the pad shows the PlayStation labels.
	const bool bPad = UTallowInputDeviceSubsystem::GetCurrent(this) == ETallowInputDevice::Gamepad;
	auto Word = [bPad](ETallowPromptAction Action, const TCHAR* Keyboard) { return bPad ? TallowInputPrompts::Get(Action, ETallowInputDevice::Gamepad).Label.ToString() : FString(Keyboard); };
	TArray<TPair<FString, FString>> Rows;
	if (!bHasCandle) Rows.Add({Word(ETallowPromptAction::Interact, TEXT("E")), TEXT("Pick up the candle")});
	else
	{
		Rows.Add({Word(ETallowPromptAction::Relight, TEXT("R")), bLit ? TEXT("Blow out candle (hides you)") : TEXT("Strike a match (noisy)")});
		if (bLit) Rows.Add({Word(ETallowPromptAction::Cup, TEXT("RMB")), TEXT("Cup the flame (dimmer)")});
		Rows.Add({Word(ETallowPromptAction::Interact, TEXT("E")), TEXT("Use / interact")});
	}
	Rows.Add({Word(ETallowPromptAction::HoldBreath, TEXT("F")), TEXT("Hold still")});
	Rows.Add({Word(ETallowPromptAction::Run, TEXT("Shift")), bLit ? TEXT("Sprint (snuffs the candle)") : TEXT("Sprint")});
	Rows.Add({Word(ETallowPromptAction::Crouch, TEXT("Ctrl")), TEXT("Crouch")});

	const FSlateFontInfo KeyFont = TallowPaper::Print(15);
	const FSlateFontInfo TextFont = TallowPaper::Print(16);
	const float RowH = 30.f, Pad = 14.f, KeyW = bPad ? 96.f : 60.f, Width = 410.f;
	const FVector2D Size(Width, Pad * 2 + Rows.Num() * RowH);
	const FVector2D Origin(24.f, Geo.GetLocalSize().Y - Size.Y - 24.f);
	FSlateDrawElement::MakeBox(Out, ++Layer, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(Origin)), &Solid, ESlateDrawEffect::None, FLinearColor(.008f,.007f,.006f,.9f));
	++Layer;
	for (int32 I = 0; I < Rows.Num(); ++I)
	{
		const FVector2D Row = Origin + FVector2D(Pad, Pad + I * RowH);
		// key cap
		FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(FVector2D(KeyW, RowH - 5.f), FSlateLayoutTransform(Row)), &Solid, ESlateDrawEffect::None, FLinearColor(.04f,.035f,.028f,.9f));
		const FVector2D KeySize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Rows[I].Key, KeyFont);
		FSlateDrawElement::MakeText(Out, Layer + 1, Geo.ToPaintGeometry(KeySize, FSlateLayoutTransform(Row + FVector2D((KeyW - KeySize.X) * 0.5f, (RowH - 5.f - KeySize.Y) * 0.5f))), Rows[I].Key, KeyFont, ESlateDrawEffect::None, TallowPaper::Ink);
		FSlateDrawElement::MakeText(Out, Layer + 1, Geo.ToPaintGeometry(FVector2D(Width - KeyW - Pad * 3, RowH), FSlateLayoutTransform(Row + FVector2D(KeyW + 10.f, 2.f))), Rows[I].Value, TextFont, ESlateDrawEffect::None, TallowPaper::Pencil);
	}
	return Layer + 2;
}
