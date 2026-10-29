#include "UI/Presentation/TallowMatchCountWidget.h"

#include "UI/Presentation/TallowPaper.h"
#include "UI/Presentation/STallowMark.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TallowMatches"

UTallowMatchCountWidget::UTallowMatchCountWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetIsFocusable(false);
}

void UTallowMatchCountWidget::ShowCount(int32 Count, bool bTriedWithNone)
{
	Text = bTriedWithNone ? LOCTEXT("None", "no matches") : FText::Format(LOCTEXT("Count", "matches  {0}"), Count);
	ShownAt = FPlatformTime::Seconds();
}

TSharedRef<SWidget> UTallowMatchCountWidget::RebuildWidget()
{
	// In the book face's italic on a little soot, bottom left; it holds for two seconds, then fades over one.
	auto Alpha = [this]() { return FMath::Clamp(3.f - float(FPlatformTime::Seconds() - ShownAt), 0.f, 1.f); };
	return SNew(SOverlay)
	+ SOverlay::Slot()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Bottom)
	.Padding(FMargin(46.f, 0.f, 0.f, 128.f))
	[
		SNew(SOverlay)
		+ SOverlay::Slot()[SNew(STallowSmudge).Alpha_Lambda([Alpha]() { return .85f * Alpha(); })]
		+ SOverlay::Slot().Padding(FMargin(16.f, 6.f, 20.f, 6.f))
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return Text; })
			.Font(TallowPaper::FellItalic(TallowPaper::Pt::Small + 1))
			.ColorAndOpacity_Lambda([this, Alpha]()
			{
				const FLinearColor Ink = Text.ToString().StartsWith(TEXT("no ")) ? TallowPaper::Blood : TallowPaper::Ink;
				return FSlateColor(FLinearColor(Ink.R, Ink.G, Ink.B, Alpha()));
			})
		]
	];
}

#undef LOCTEXT_NAMESPACE
