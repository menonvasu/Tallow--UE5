#include "UI/Presentation/TallowLessonWidget.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTallowLessonLifecycleTest,
	"Tallow.Presentation.Lessons.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTallowLessonLifecycleTest::RunTest(const FString& Parameters)
{
	UTallowLessonWidget* Widget = NewObject<UTallowLessonWidget>();
	TestFalse(TEXT("None cannot be shown"), Widget->ShowLesson(ETallowLesson::None));
	TestFalse(TEXT("Unknown enum cannot be shown"), Widget->ShowLesson(static_cast<ETallowLesson>(255)));
	TestTrue(TEXT("Show draft"), Widget->ShowLesson(ETallowLesson::Draft));
	TestTrue(TEXT("Draft has been seen"), Widget->HasSeenLesson(ETallowLesson::Draft));
	TestFalse(TEXT("Showing is not completing"), Widget->HasCompletedLesson(ETallowLesson::Draft));
	Widget->DismissLesson();
	TestEqual(TEXT("Dismiss clears prompt"), Widget->GetActiveLesson(), ETallowLesson::None);
	TestTrue(TEXT("Dismiss retains seen history"), Widget->HasSeenLesson(ETallowLesson::Draft));
	TestTrue(TEXT("Dismissed lesson can recur"), Widget->ShowLesson(ETallowLesson::Draft));
	TestTrue(TEXT("Cup replaces draft"), Widget->ShowLesson(ETallowLesson::Cup));
	TestFalse(TEXT("Late draft success cannot complete cup"), Widget->CompleteLessonIfActive(ETallowLesson::Draft));
	TestEqual(TEXT("Stale callback retains active prompt"), Widget->GetActiveLesson(), ETallowLesson::Cup);
	TestFalse(TEXT("Stale callback does not award completion"), Widget->HasCompletedLesson(ETallowLesson::Cup));
	TestTrue(TEXT("Expected cup success completes cup"), Widget->CompleteLessonIfActive(ETallowLesson::Cup));
	TestTrue(TEXT("Only active lesson completes"), Widget->HasCompletedLesson(ETallowLesson::Cup));
	TestFalse(TEXT("Replaced draft remains incomplete"), Widget->HasCompletedLesson(ETallowLesson::Draft));
	Widget->CompleteLesson();
	TestEqual(TEXT("Repeated completion is harmless"), Widget->GetActiveLesson(), ETallowLesson::None);
	TestTrue(TEXT("Draft can still be shown"), Widget->ShowLesson(ETallowLesson::Draft));
	TestFalse(TEXT("Completed cup is suppressed"), Widget->ShowLesson(ETallowLesson::Cup));
	TestEqual(TEXT("Suppression does not hide current prompt"), Widget->GetActiveLesson(), ETallowLesson::Draft);
	Widget->DismissLesson(); // Capture integration may dismiss, but must not reset.
	TestTrue(TEXT("Capture-style dismissal retains completion"), Widget->HasCompletedLesson(ETallowLesson::Cup));
	Widget->ResetLessons();
	TestFalse(TEXT("Reset clears seen history"), Widget->HasSeenLesson(ETallowLesson::Draft));
	TestFalse(TEXT("Reset clears completion"), Widget->HasCompletedLesson(ETallowLesson::Cup));
	TestTrue(TEXT("Fresh run can teach cup"), Widget->ShowLesson(ETallowLesson::Cup));
	Widget->CupKey = FText::FromString(TEXT("Test remapped key"));
	TestTrue(TEXT("Copy uses caller key label"), Widget->GetLessonPrompt().ToString().Contains(TEXT("Test remapped key")));
	return true;
}
#endif
