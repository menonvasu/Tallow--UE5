#include "TallowLockpickWidget.h"

#include "Components/Border.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Layout/SBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Tasks/TallowLockpickGame.h"
#include "Blueprint/WidgetTree.h"
#include "UI/Presentation/TallowPaper.h"
#include "Input/TallowInputPrompts.h"
#include "Input/TallowInputDeviceSubsystem.h"

void UTallowLockpickWidget::InitializeLockpickWidget(UTallowLockpickGame* InGameComponent)
{
	if (!InGameComponent) { return; }
	LockpickGame = InGameComponent;
	
	PinWidgets.Reset();
	PinWidgets.Add(Pin0);
	PinWidgets.Add(Pin1);
	PinWidgets.Add(Pin2);
	PinWidgets.Add(Pin3);
	
	LockpickGame->OnFalseSet.AddDynamic(this, &UTallowLockpickWidget::HandleOnFalseSet);
	LockpickGame->OnOverset.AddDynamic(this, &UTallowLockpickWidget::HandleOnOverSet);
	LockpickGame->OnSnap.AddDynamic(this, &UTallowLockpickWidget::HandleOnTensionReleasedOrOnSnap);
	LockpickGame->OnTensionReleased.AddDynamic(this, &UTallowLockpickWidget::HandleOnTensionReleasedOrOnSnap);
	LockpickGame->OnPinSet.AddDynamic(this, &UTallowLockpickWidget::HandleOnPinSet);
	LockpickGame->OnLockpickSet.AddDynamic(this, &UTallowLockpickWidget::HandleOnLockpickSet);
}

void UTallowLockpickWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!WidgetTree) return;
	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		if (UTextBlock* Words = Cast<UTextBlock>(Widget))
		{
			Words->SetFont(TallowPaper::Hand(FMath::Max(18, Words->GetFont().Size + 2)));
			Words->SetColorAndOpacity(FSlateColor(TallowPaper::Ink));
			Words->SetShadowOffset(FVector2D(1.5f, 1.5f));
			Words->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, .55f));
		}
		else if (UBorder* Panel = Cast<UBorder>(Widget))
		{
			// Pins and the pick keep their solid shape (the game recolours them every tick); every other panel is card.
			if (Panel == PickImage) Panel->SetBrushColor(TallowPaper::Ink);
			else if (Panel != Pin0 && Panel != Pin1 && Panel != Pin2 && Panel != Pin3)
			{
				Panel->SetBrush(*TallowPaper::Scrap());
				Panel->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, .92f));
			}
		}
		else if (UProgressBar* Bar = Cast<UProgressBar>(Widget))
		{
			FProgressBarStyle Style = Bar->GetWidgetStyle();
			Style.BackgroundImage = *TallowPaper::Scrap();
			Style.BackgroundImage.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, .9f));
			Bar->SetWidgetStyle(Style);
		}
	});
}

void UTallowLockpickWidget::NativeTick(const FGeometry& Geometry, float InDeltaTime)
{
	Super::NativeTick(Geometry, InDeltaTime);
	if (!LockpickGame.IsValid() || !PickImage) { return; }

	bool bIsAnyPinOverset = false;
	bool bIsAnyFalseSet = false;
	
	ShakeTime += InDeltaTime;
	const float CurrentTorque = LockpickGame->GetCurrentTorque();
	const int32 BindingPinIndex = LockpickGame->GetCurrentBindingPinIndex();
	const bool bPickOnBinding = BindingPinIndex == FMath::RoundToInt(LockpickGame->GetPickDepth());
	const bool bIsPickMovingUp = (LockpickGame->GetTargetPickLift() - LockpickGame->GetPickLift()) > 0.01f;
	const bool bShouldShake = bPickOnBinding && CurrentTorque > LockpickGame->GetMinTorque() &&bIsPickMovingUp;
	
	ShakeAlpha = FMath::FInterpTo(ShakeAlpha, bShouldShake ? 1.0f : 0.0f, InDeltaTime, ShakeFadeSpeed);
	const float Amp = CurrentTorque * MaxShakePixels * ShakeAlpha;
	const FVector2D Shake(FMath::Sin(ShakeTime * 33.0f) * Amp, FMath::Sin(ShakeTime * 44.0f + 1.3f) * Amp * 0.5f);
	
	// Update pick position
	PickImage->SetRenderTranslation(FVector2D(LockpickGame->GetPickDepth() * PinSpacing + Shake.X,-LockpickGame->GetPickLift() * MaxLiftPixels + Shake.Y));
	
	// Offset pin by change and update color
	// Pencil for a free pin, ink as it binds, bright ink once set, ochre for a false set, blood for overset.
	const FLinearColor Gray = TallowPaper::Pencil;
	const TArray<FPinChamber>& PinChambers = LockpickGame->GetPinChambers();
	for (int i = 0; i < PinWidgets.Num(); i++)
	{
		if (!PinWidgets[i]) { continue; }
		// The overlay draws four pins; a lock with fewer hides the spare ones
		if (!PinChambers.IsValidIndex(i))
		{
			if (PinWidgets[i]->GetVisibility() != ESlateVisibility::Collapsed) { PinWidgets[i]->SetVisibility(ESlateVisibility::Collapsed); }
			continue;
		}
		if (PinWidgets[i]->GetVisibility() == ESlateVisibility::Collapsed) { PinWidgets[i]->SetVisibility(ESlateVisibility::Visible); }

		FVector2D PinOffset(0.0f, -(PinChambers[i].CurrentHeight * MaxLiftPixels));
		PinWidgets[i]->SetRenderTranslation(PinOffset);
		
		switch (PinChambers[i].PinState)
		{
		case EPinState::Free:
			PinWidgets[i]->SetBrushColor(Gray);
			break;
		case EPinState::Binding:
			{
				FLinearColor Color = Gray;
				if (i == BindingPinIndex)
				{
					// fades from gray to white as dwell fills
					Color = FMath::Lerp(Gray, TallowPaper::Ink, LockpickGame->GetSetDwellProgress());
				}
				PinWidgets[i]->SetBrushColor(Color);
				break;
			}
		case EPinState::Set:
			PinWidgets[i]->SetBrushColor(TallowPaper::Ink);
			break;
		case EPinState::Overset:
			PinWidgets[i]->SetBrushColor(TallowPaper::Blood);
			bIsAnyPinOverset = true;
			break;
		case EPinState::FalseSet:
			PinWidgets[i]->SetBrushColor(TallowPaper::Ochre);
			bIsAnyFalseSet = true;
			break;
		}
	}
	
	if (TensionBar)
	{
		TensionBar->SetPercent(CurrentTorque);
		
		FLinearColor BarColor;
		if (CurrentTorque >= 0.75f && bIsAnyPinOverset)
		{
			BarColor = TallowPaper::Blood;
		}
		else if (bIsAnyFalseSet)
		{
			BarColor = TallowPaper::Ochre;
		}
		else if (CurrentTorque < LockpickGame->GetMinTorque())
		{
			BarColor = Gray;                                
		}
		else
		{
			BarColor = TallowPaper::Ink;
		}
		
		TensionBar->SetFillColorAndOpacity(BarColor);
		
		const bool bBarDanger = bIsAnyPinOverset && CurrentTorque >= 0.75f;
		BarShakeAlpha = FMath::FInterpTo(BarShakeAlpha, bBarDanger ? 1.0f : 0.0f, InDeltaTime, ShakeFadeSpeed);
		const float BarAmp = MaxBarShakePixels * BarShakeAlpha;
		TensionBar->SetRenderTranslation(FVector2D(FMath::Sin(ShakeTime * 40.0f) * BarAmp, 0.0f));
	}
	
	// Status check
	StatusTimeRemaining = FMath::Max(0.0f, StatusTimeRemaining - InDeltaTime);
	if (EventFeedbackText)
	{
		EventFeedbackText->SetOpacity(FMath::Clamp(StatusTimeRemaining / 0.5f, 0.0f, 1.0f));
	}
	
	if (!HintFeedbackText) { return; }
	switch (LockpickGame->GetHint())
	{
	case ELockpickHint::None:
		HintFeedbackText->SetText(FText::FromString(""));
		break;
	case ELockpickHint::ApplyTension:
		HintFeedbackText->SetText(FText::FromString("hold left mouse"));
		break;
	case ELockpickHint::EaseTension:
		HintFeedbackText->SetText(FText::FromString("ease off"));
		break;
	case ELockpickHint::FindBindingPin:
		HintFeedbackText->SetText(FText::FromString("find the stiff pin"));
		break;
	case ELockpickHint::LowerPick:
		HintFeedbackText->SetText(FText::FromString("lower the pick"));
	}
}
	
void UTallowLockpickWidget::NativeDestruct()
{
	if (LockpickGame.IsValid())
	{
		LockpickGame->OnFalseSet.RemoveDynamic(this, &UTallowLockpickWidget::HandleOnFalseSet);
		LockpickGame->OnOverset.RemoveDynamic(this, &UTallowLockpickWidget::HandleOnOverSet);
		LockpickGame->OnSnap.RemoveDynamic(this, &UTallowLockpickWidget::HandleOnTensionReleasedOrOnSnap);
		LockpickGame->OnTensionReleased.RemoveDynamic(this, &UTallowLockpickWidget::HandleOnTensionReleasedOrOnSnap);
		LockpickGame->OnPinSet.RemoveDynamic(this, &UTallowLockpickWidget::HandleOnPinSet);
		LockpickGame->OnLockpickSet.RemoveDynamic(this, &UTallowLockpickWidget::HandleOnLockpickSet);
	}
	
	Super::NativeDestruct();
}

void UTallowLockpickWidget::ShowStatus(const FText& Message)
{
	if (EventFeedbackText)
	{
		EventFeedbackText->SetText(Message);
		EventFeedbackText->SetOpacity(1.0f);
		StatusTimeRemaining = 2.0f;
	}
}

//---------------------------------------------------------------------------
//All status handlers

void UTallowLockpickWidget::HandleOnFalseSet()
{
	ShowStatus(FText::FromString("something feels off"));
}
void UTallowLockpickWidget::HandleOnTensionReleasedOrOnSnap()
{
	ShowStatus(FText::FromString("it slipped. start again"));
}

void UTallowLockpickWidget::HandleOnOverSet()
{
	ShowStatus(FText::FromString("the pick is straining"));
}

void UTallowLockpickWidget::HandleOnPinSet()
{
	ShowStatus(FText::FromString("yes"));
}

void UTallowLockpickWidget::HandleOnLockpickSet()
{
	ShowStatus(FText::FromString("open"));
}

TSharedRef<SWidget> UTallowLockpickWidget::RebuildWidget()
{
	Super::RebuildWidget();
	return SNew(SBox);
}
int32 UTallowLockpickWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	Layer = Super::NativePaint(Args, Geo, Cull, Out, Layer, Style, bParentEnabled);
	if (!LockpickGame.IsValid()) { return Layer; }
	using namespace TallowPaper;

	// The lock in section on a soot card: housing and springs above, pins through the plug, the pick coming in from the left.
	// Design units (760 x 510) are scaled to the screen; the card is centred.
	constexpr float W = 760.f, H = 510.f;
	const FVector2D Screen = Geo.GetLocalSize();
	const float S = FMath::Clamp(float(Screen.Y) / 900.f, .6f, 1.25f);
	const FVector2D Origin((Screen.X - W * S) * .5f, (Screen.Y - H * S) * .5f);
	auto At = [&](float X, float Y) { return Origin + FVector2D(X, Y) * S; };
	auto Line = [&](FVector2D A, FVector2D B, const FLinearColor& C, float Thick, int32 Seed)
	{
		DrawLine(Out, ++Layer, Geo, At(A.X, A.Y), At(B.X, B.Y), C, Thick * S, Seed);
	};
	auto Words = [&](const FString& Text, int32 Size, float X, float Y, const FLinearColor& C)
	{
		DrawText(Out, ++Layer, Geo, Text, Hand(FMath::RoundToInt(Size * S)), At(X, Y), C);
	};

	// The lock drawn on a pool of soot over the door, not on a card.
	DrawSmudge(Out, ++Layer, Geo, Origin + FVector2D(W, H) * S * .5f, FVector2D(W, H) * S * .72f, .95f);

	// Geometry of the section.
	constexpr float HousingTop = 74.f, PlugTop = 232.f, PlugBottom = 332.f, PinBottomRest = 300.f, PinLength = 70.f, Travel = 100.f;
	constexpr float Left = 96.f, Right = 690.f, FirstPin = 190.f, PinGap = 130.f, PinHalf = 15.f;
	const float Torque = LockpickGame->GetCurrentTorque();

	// Housing, plug and keyway.
	Line(FVector2D(Left, HousingTop), FVector2D(Right, HousingTop), Ink, 2.2f, 1);
	Line(FVector2D(Right, HousingTop), FVector2D(Right, PlugBottom), Ink, 2.2f, 2);
	Line(FVector2D(Left, PlugTop), FVector2D(Right, PlugTop), Pencil, 1.6f, 3);
	Line(FVector2D(Left, PlugBottom), FVector2D(Right, PlugBottom), Ink, 2.2f, 4);
	Line(FVector2D(Left, PlugBottom - 40.f), FVector2D(Right - 40.f, PlugBottom - 40.f), Pencil, 1.2f, 5);
	Line(FVector2D(Left, HousingTop), FVector2D(Left, PlugTop), Ink, 2.2f, 6);

	const TArray<FPinChamber>& Pins = LockpickGame->GetPinChambers();
	const int32 PickPin = FMath::RoundToInt(LockpickGame->GetPickDepth());
	const float PickX = FirstPin + LockpickGame->GetPickDepth() * PinGap;
	const float PickLift = LockpickGame->GetPickLift();
	const float PickTipY = PinBottomRest - PickLift * Travel;
	const bool bDone = LockpickGame->GetIsCompleting();

	for (int32 I = 0; I < Pins.Num(); ++I)
	{
		const FPinChamber& Pin = Pins[I];
		const float X = FirstPin + I * PinGap;
		FLinearColor C = Pencil;
		switch (Pin.PinState)
		{
		case EPinState::Set: C = Ink; break;
		case EPinState::Overset: C = Blood; break;
		case EPinState::FalseSet: C = Ochre; break;
		default: break;
		}
		const float Rise = Pin.CurrentHeight * Travel;
		const float Bottom = PinBottomRest - Rise;
		const float Top = Bottom - PinLength;

		// the chamber walls, faint, through the housing and the plug
		Line(FVector2D(X - PinHalf - 3.f, HousingTop), FVector2D(X - PinHalf - 3.f, PlugTop), Pencil.CopyWithNewOpacity(.55f), 1.f, 10 + I);
		Line(FVector2D(X + PinHalf + 3.f, HousingTop), FVector2D(X + PinHalf + 3.f, PlugTop), Pencil.CopyWithNewOpacity(.55f), 1.f, 20 + I);

		// the spring, squeezed between the housing and the pin as it rises
		const int32 Coils = 7;
		const float SpringTop = HousingTop + 4.f, SpringBottom = Top - 2.f;
		for (int32 K = 0; K < Coils; ++K)
		{
			const float Y0 = SpringTop + (SpringBottom - SpringTop) * K / Coils;
			const float Y1 = SpringTop + (SpringBottom - SpringTop) * (K + 1) / Coils;
			Line(FVector2D(X - 11.f + (K % 2) * 22.f, Y0), FVector2D(X + 11.f - (K % 2) * 22.f, Y1), Pencil, 1.3f, 30 + I * 8 + K);
		}

		// the pin
		Line(FVector2D(X - PinHalf, Top), FVector2D(X - PinHalf, Bottom), C, 2.2f, 40 + I);
		Line(FVector2D(X + PinHalf, Top), FVector2D(X + PinHalf, Bottom), C, 2.2f, 50 + I);
		Line(FVector2D(X - PinHalf, Top), FVector2D(X + PinHalf, Top), C, 2.2f, 60 + I);
		Line(FVector2D(X - PinHalf, Bottom), FVector2D(X, Bottom + 7.f), C, 2.2f, 70 + I);
		Line(FVector2D(X + PinHalf, Bottom), FVector2D(X, Bottom + 7.f), C, 2.2f, 80 + I);

		if (Pin.PinState == EPinState::Set)
		{
			// held: the break at the shear line, struck through and dotted down the pin
			Line(FVector2D(X - PinHalf - 9.f, Top + PinLength * .5f), FVector2D(X + PinHalf + 9.f, Top + PinLength * .5f), Ink, 2.f, 90 + I);
			Line(FVector2D(X - 5.f, Top + 12.f), FVector2D(X + 5.f, Top + 12.f), Ink, 2.f, 100 + I);
			Line(FVector2D(X - 5.f, Top + 24.f), FVector2D(X + 5.f, Top + 24.f), Ink, 2.f, 110 + I);
			Words(TEXT("set"), 16, X, PlugBottom + 22.f, Ink);
		}
	}

	// The pick: handle outside the keyway, shaft along it, the tip hooked up under the pin it is on.
	Line(FVector2D(Left - 70.f, PlugBottom - 14.f), FVector2D(Left - 8.f, PlugBottom - 14.f), Ink, 5.f, 120);
	Line(FVector2D(Left - 8.f, PlugBottom - 14.f), FVector2D(PickX - 18.f, PlugBottom - 14.f), Ink, 2.4f, 121);
	Line(FVector2D(PickX - 18.f, PlugBottom - 14.f), FVector2D(PickX - 4.f, PickTipY + 6.f), Ink, 2.4f, 122);
	Line(FVector2D(PickX - 4.f, PickTipY + 6.f), FVector2D(PickX + 3.f, PickTipY - 1.f), Ink, 2.4f, 123);

	// the one under the pick, as a red thread
	if (!bDone && Pins.IsValidIndex(PickPin) && FMath::Abs(LockpickGame->GetPickDepth() - PickPin) < .3f)
	{
		const float X = FirstPin + PickPin * PinGap;
		DrawThread(Out, ++Layer, Geo, At(X - 38.f, PlugBottom + 8.f), At(X + 38.f, PlugBottom + 8.f), 7 + PickPin, 1.f);
	}

	// The tension wrench turns with the force on it.
	const float Angle = FMath::DegreesToRadians(Torque * 16.f);
	const FVector2D Pivot(Left + 8.f, PlugBottom - 20.f);
	auto Turn = [&](float X, float Y)
	{
		const FVector2D D = FVector2D(X, Y) - Pivot;
		return Pivot + FVector2D(D.X * FMath::Cos(Angle) - D.Y * FMath::Sin(Angle), D.X * FMath::Sin(Angle) + D.Y * FMath::Cos(Angle));
	};
	bool bStrain = false;
	for (const FPinChamber& Pin : Pins) { bStrain |= Pin.PinState == EPinState::Overset; }
	const FLinearColor Wrench = bStrain && Torque > .75f ? Blood : Ink;
	Line(Turn(Left + 8.f, PlugBottom - 20.f), Turn(Left + 8.f, PlugBottom + 54.f), Wrench, 3.4f, 130);
	Line(Turn(Left + 8.f, PlugBottom + 54.f), Turn(Left - 56.f, PlugBottom + 54.f), Wrench, 3.4f, 131);

	// Tension: a row of ticks, the notch where the pins begin to bind, red only when the pick is straining.
	const float GaugeY = 404.f, GaugeLeft = 190.f, GaugeWidth = 500.f;
	const int32 Ticks = 26;
	Words(TEXT("tension"), 17, 118.f, GaugeY, Pencil);
	Line(FVector2D(GaugeLeft, GaugeY + 13.f), FVector2D(GaugeLeft + GaugeWidth, GaugeY + 13.f), Pencil, 1.3f, 140);
	for (int32 I = 0; I < Ticks; ++I)
	{
		const float T = float(I) / Ticks;
		const float X = GaugeLeft + T * GaugeWidth + 4.f;
		const bool bOn = T < Torque;
		const FLinearColor C = !bOn ? Pencil.CopyWithNewOpacity(.5f) : (bStrain && Torque > .75f) ? Blood : Ink;
		Line(FVector2D(X, GaugeY + (bOn ? -10.f : -4.f)), FVector2D(X, GaugeY + 10.f), C, bOn ? 2.4f : 1.2f, 150 + I);
	}
	const float MinX = GaugeLeft + LockpickGame->GetMinTorque() * GaugeWidth;
	Line(FVector2D(MinX, GaugeY + 14.f), FVector2D(MinX, GaugeY + 26.f), Pencil, 1.4f, 190);

	// Words: what just happened, then what to do next, then the keys. All lower case.
	const bool bPad = UTallowInputDeviceSubsystem::GetCurrent(this) == ETallowInputDevice::Gamepad;
	auto Label = [bPad](ETallowPromptAction Action)
	{
		return TallowInputPrompts::Get(Action, bPad ? ETallowInputDevice::Gamepad : ETallowInputDevice::KeyboardMouse).Label.ToString().ToLower();
	};
	FString Hint;
	switch (LockpickGame->GetHint())
	{
	case ELockpickHint::ApplyTension: Hint = bPad ? FString(TEXT("hold ")) + Label(ETallowPromptAction::Tension) : FString(TEXT("hold the left button")); break;
	case ELockpickHint::EaseTension: Hint = TEXT("ease off"); break;
	case ELockpickHint::FindBindingPin: Hint = TEXT("find the stiff pin"); break;
	case ELockpickHint::LowerPick: Hint = TEXT("lower the pick"); break;
	default: break;
	}
	if (EventFeedbackText && StatusTimeRemaining > 0.f)
	{
		const FString Status = EventFeedbackText->GetText().ToString();
		const bool bGood = Status == TEXT("yes") || Status == TEXT("open");
		Words(Status, 22, W * .5f, 40.f, bGood ? Ink : Blood);
	}
	Words(Hint, 19, W * .5f, 452.f, Ink);
	const FString Keys = bPad ? FString::Printf(TEXT("[ %s ]  pick      [ %s ]  tension      [ %s ]  stop"), *Label(ETallowPromptAction::PickMove), *Label(ETallowPromptAction::Tension), *Label(ETallowPromptAction::StopLockpick))
		: FString(TEXT("[ mouse ]  pick      [ left button ]  tension      [ E ]  stop"));
	const FSlateFontInfo KeyFont = FellItalic(FMath::RoundToInt(Pt::Small * S));
	const FVector2D KeySize = MeasureKeys(Keys, KeyFont);
	DrawKeys(Out, ++Layer, Geo, Keys, KeyFont, At(W * .5f, 488.f) - FVector2D(KeySize.X * .5f, 0.f), Pencil, Ink);
	Layer += 3;
	return Layer + 3;
}
