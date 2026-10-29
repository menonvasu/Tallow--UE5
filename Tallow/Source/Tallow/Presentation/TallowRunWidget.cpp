#include "Presentation/TallowRunWidget.h"
#include "Chapter/TallowChapterSubsystem.h"
#include "Presentation/TallowPlayDirector.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UTallowRunWidget::RebuildWidget()
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return SNew(SOverlay)
	+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(32.f, 28.f)
	[
		SNew(STextBlock).Text_Lambda([this] { return Director ? Director->GetObjective() : FText::GetEmpty(); })
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
		.ColorAndOpacity(FLinearColor(.79f, .74f, .61f, .9f))
		.ShadowOffset(FVector2D(1.f, 2.f)).ShadowColorAndOpacity(FLinearColor(0,0,0,.8f))
	]
	+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
	[
		SNew(STextBlock).Text(FText::FromString(TEXT("·")))
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 16)).ColorAndOpacity(FLinearColor(.9f,.85f,.7f,.7f))
	];
}
