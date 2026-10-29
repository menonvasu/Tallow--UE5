#include "UI/Presentation/TallowLessonWidget.h"
#include "Styling/CoreStyle.h"
#include "UI/Presentation/TallowPaper.h"
#include "UI/Presentation/STallowMark.h"
#include "Widgets/SBoxPanel.h"
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
		return FText::Format(LOCTEXT("Chair", "[ {0} ]  to rock. Push as it swings back."), RockKey);
	case ETallowLesson::CandlePickup:
		return FText::Format(LOCTEXT("Pickup", "Look at the candle.  [ {0} ]  to take it."), InteractKey);
	case ETallowLesson::Draft:
		return LOCTEXT("Draft", "A draught is catching the flame. Watch it bend.");
	case ETallowLesson::Cup:
		return FText::Format(LOCTEXT("Cup", "Hold  [ {0} ]  to shield the flame from the draught."), CupKey);
	case ETallowLesson::Relight:
		return FText::Format(LOCTEXT("Relight", "[ {0} ]  strikes a match. It is loud."), RelightKey);
	case ETallowLesson::Stillness:
		return FText::Format(LOCTEXT("Stillness", "Hold  [ {0} ]  to hold your breath. It still sees your light."), StillnessKey);
	default:
		return FText::GetEmpty();
	}
}

TSharedRef<SWidget> UTallowLessonWidget::RebuildWidget()
{
	// One italic line, low in the middle, a small thread tick before it, on soot rather than a panel.
	return SNew(SOverlay)
	+ SOverlay::Slot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Bottom)
	.Padding(FMargin(24.f, 0.f, 24.f, 92.f))
	[
		SNew(SOverlay)
		.Visibility_Lambda([this]()
		{
			return ActiveLesson == ETallowLesson::None ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
		})
		+ SOverlay::Slot()[SNew(STallowSmudge).Alpha(.9f)]
		+ SOverlay::Slot().Padding(FMargin(10.f, 6.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f, 0.f)
			[SNew(SBox).WidthOverride(24.f).HeightOverride(8.f)[SNew(STallowThread).Seed(11)]]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STallowKeys)
				.Text_Lambda([this]() { return GetLessonPrompt().ToString(); })
				.Font(TallowPaper::FellItalic(TallowPaper::Pt::Lead))
				.Colour(TallowPaper::Ink).KeyColour(TallowPaper::Ivory)
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
