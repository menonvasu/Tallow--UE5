#include "UI/Presentation/TallowLessonWidget.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TallowLessons"

UTallowLessonWidget::UTallowLessonWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetIsFocusable(false);
}

bool UTallowLessonWidget::ShowLesson(ETallowLesson Lesson)
{
	if (Lesson <= ETallowLesson::None || Lesson > ETallowLesson::Stillness || CompletedLessons.Contains(Lesson))
	{
		return false;
	}
	SeenLessons.Add(Lesson);
	ActiveLesson = Lesson;
	return true;
}

void UTallowLessonWidget::CompleteLesson()
{
	if (ActiveLesson == ETallowLesson::None) return;
	const ETallowLesson Finished = ActiveLesson;
	CompletedLessons.Add(Finished);
	ActiveLesson = ETallowLesson::None;
	// Clear first so a listener can safely show the next lesson.
	OnLessonCompleted.Broadcast(Finished);
}

void UTallowLessonWidget::DismissLesson()
{
	ActiveLesson = ETallowLesson::None;
}

bool UTallowLessonWidget::CompleteLessonIfActive(ETallowLesson ExpectedLesson)
{
	if (ExpectedLesson == ETallowLesson::None || ActiveLesson != ExpectedLesson) return false;
	CompleteLesson();
	return true;
}

void UTallowLessonWidget::ResetLessons()
{
	ActiveLesson = ETallowLesson::None;
	SeenLessons.Reset();
	CompletedLessons.Reset();
}

FText UTallowLessonWidget::GetLessonPrompt() const
{
	switch (ActiveLesson)
	{
	case ETallowLesson::Chair:
		return FText::Format(LOCTEXT("Chair", "Press {0} to rock. Time each push with the backward swing."), RockKey);
	case ETallowLesson::CandlePickup:
		return FText::Format(LOCTEXT("Pickup", "Look at the candle and press {0} to pick it up."), InteractKey);
	case ETallowLesson::Draft:
		return LOCTEXT("Draft", "A draft is catching the flame. Watch it bend and gutter.");
	case ETallowLesson::Cup:
		return FText::Format(LOCTEXT("Cup", "Hold {0} to shield the flame from the draft."), CupKey);
	case ETallowLesson::Relight:
		return FText::Format(LOCTEXT("Relight", "With the flame out, press {0} to strike a match. Relighting makes noise."), RelightKey);
	case ETallowLesson::Stillness:
		return FText::Format(LOCTEXT("Stillness", "Hold {0} to keep still and avoid being seen. You cannot move or look around."), StillnessKey);
	default:
		return FText::GetEmpty();
	}
}

TSharedRef<SWidget> UTallowLessonWidget::RebuildWidget()
{
	return SNew(SOverlay)
	+ SOverlay::Slot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Bottom)
	.Padding(FMargin(24.f, 0.f, 24.f, 48.f))
	[
		SNew(SBox)
		.MaxDesiredWidth(640.f)
		.Visibility_Lambda([this]()
		{
			return ActiveLesson == ETallowLesson::None ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
		})
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(.025f, .021f, .016f, .88f))
			.Padding(FMargin(24.f, 14.f))
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return GetLessonPrompt(); })
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 20))
				.ColorAndOpacity(FLinearColor(.9f, .84f, .67f, 1.f))
				.Justification(ETextJustify::Center)
				.AutoWrapText(true)
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
