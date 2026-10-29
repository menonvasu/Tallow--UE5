#include "UI/Presentation/TallowNellNoteWidget.h"

#include "Kismet/GameplayStatics.h"
#include "Input/TallowInputDeviceSubsystem.h"
#include "Input/TallowInputPrompts.h"
#include "UI/Presentation/TallowPaper.h"
#include "Widgets/Layout/SBox.h"

UTallowNellNoteWidget::UTallowNellNoteWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetIsFocusable(false);
}

TSharedRef<SWidget> UTallowNellNoteWidget::RebuildWidget()
{
	return SNew(SBox);
}

void UTallowNellNoteWidget::ShowNote(const FString& InText, const FString& InHint, float InSeconds)
{
	Text = InText; Hint = InHint; Seconds = InSeconds; Age = 0.f;
}

void UTallowNellNoteWidget::HideNote()
{
	// Let it go in a short fade rather than a cut.
	if (IsShowing()) Age = FMath::Max(Age, Seconds - .35f);
}

void UTallowNellNoteWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (!bSuppressed && !UGameplayStatics::IsGamePaused(this)) Age += DeltaTime;
}

int32 UTallowNellNoteWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	Layer = Super::NativePaint(Args, Geometry, Culling, Out, Layer, Style, bParentEnabled);
	if (!IsShowing() || bSuppressed || Text.IsEmpty() || UGameplayStatics::IsGamePaused(this)) return Layer;
	using namespace TallowPaper;
	// Written in, not faded like a caption: a quick rise, a long hold, a slow fade.
	const float Alpha = FMath::Min(FMath::Clamp(Age / .45f, 0.f, 1.f), FMath::Clamp((Seconds - Age) / 1.1f, 0.f, 1.f));
	const FVector2D Screen = Geometry.GetLocalSize();
	const float S = FMath::Clamp(float(Screen.Y / 1080.f), .5f, 1.4f);
	TArray<FString> Lines; Text.ParseIntoArray(Lines, TEXT("\n"));
	// The hints are written with the keyboard's keys; on the pad each names its button instead.
	FString ShownHint = Hint;
	if (UTallowInputDeviceSubsystem::GetCurrent(this) == ETallowInputDevice::Gamepad)
	{
		using A = ETallowPromptAction;
		const TPair<const TCHAR*, A> Keys[] = {{TEXT("[ hold right mouse ]"), A::Cup}, {TEXT("[ right mouse ]"), A::Cup}, {TEXT("[ R ]"), A::Relight}, {TEXT("[ F ]"), A::HoldBreath}, {TEXT("[ M ]"), A::Map}};
		for (const auto& Key : Keys)
			ShownHint.ReplaceInline(Key.Key, *FString::Printf(TEXT("[ %s ]"), *TallowInputPrompts::Get(Key.Value, ETallowInputDevice::Gamepad).Label.ToString()));
	}
	// Her own words in her pencil; the keys she would press in the book face, in their caps.
	const FSlateFontInfo Font = Child(FMath::RoundToInt((Pt::Lead + 8) * S)), Keys = FellItalic(FMath::RoundToInt(Pt::Small * S));
	FVector2D Size(0.f, 0.f);
	TArray<float> Heights;
	for (const FString& Line : Lines)
	{
		const FVector2D Measured = Measure(Line, Font);
		Size.X = FMath::Max(Size.X, Measured.X); Size.Y += Measured.Y * .92f; Heights.Add(Measured.Y * .92f);
	}
	const FVector2D HintSize = ShownHint.IsEmpty() ? FVector2D::ZeroVector : MeasureKeys(ShownHint, Keys);
	Size.X = FMath::Max(Size.X, HintSize.X);
	if (!ShownHint.IsEmpty()) Size.Y += HintSize.Y + 14.f * S;
	// Low in the middle, under the interaction prompt and clear of the candle and match marks in the corners.
	const FVector2D Centre(Screen.X * .5f, Screen.Y * .815f);
	DrawSmudge(Out, ++Layer, Geometry, Centre, Size, .82f * Alpha);
	float Y = Centre.Y - Size.Y * .5f;
	for (int32 I = 0; I < Lines.Num(); ++I)
	{
		DrawText(Out, ++Layer, Geometry, Lines[I], Font, FVector2D(Centre.X, Y + Heights[I] * .5f), Ivory.CopyWithNewOpacity(Alpha));
		++Layer;
		Y += Heights[I];
	}
	if (!ShownHint.IsEmpty())
	{
		DrawKeys(Out, ++Layer, Geometry, ShownHint, Keys, FVector2D(Centre.X - HintSize.X * .5f, Y + 14.f * S + HintSize.Y * .5f), Pencil.CopyWithNewOpacity(Alpha), Ink.CopyWithNewOpacity(Alpha));
		Layer += 3;
	}
	return Layer + 2;
}
