#include "UI/Presentation/TallowMenuWidget.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTallowMenuNavigationTest, "Tallow.Presentation.Menu.NavigationAndSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTallowMenuNavigationTest::RunTest(const FString& Parameters)
{
	using C = ETallowMenuCommand;
	using A = ETallowMenuAction;
	FTallowMenuState State;
	State.Navigate(-1);
	TestEqual(TEXT("Up wraps to final item"), State.Selection, 3);
	State.Navigate(1);
	TestEqual(TEXT("Down wraps to first item"), State.Selection, 0);
	State.Activate(C::Credits);
	TestEqual(TEXT("Open credits"), State.Page, ETallowMenuPage::Credits);
	TestFalse(TEXT("Stale start callback cannot leave credits"), State.Activate(C::Start).IsSet());
	State.Back();
	TestEqual(TEXT("Back restores originating selection"), State.Selection, 2);
	TestFalse(TEXT("Escape on main never quits"), State.Back().IsSet());
	State.Show(ETallowMenuMode::Pause);
	TestFalse(TEXT("Restart asks before losing run"), State.Activate(C::Restart).IsSet());
	TestEqual(TEXT("Cancel is the safe default"), State.Commands()[State.Selection], C::Cancel);
	State.Activate(C::Cancel);
	TestFalse(TEXT("Cancel clears pending action"), State.PendingConfirmation.IsSet());
	TestEqual(TEXT("Cancel restores restart selection"), State.Selection, 2);
	State.Activate(C::Quit);
	const TOptional<A> Quit = State.Activate(C::Confirm);
	TestTrue(TEXT("Confirmation emits selected action"), Quit.IsSet() && Quit.GetValue() == A::Quit);
	TestFalse(TEXT("Held/repeated activation cannot emit twice"), State.Activate(C::Confirm).IsSet());
	TestFalse(TEXT("Escape cannot race a pending action"), State.Back().IsSet());
	State.Show(ETallowMenuMode::Pause);
	const TOptional<A> Resume = State.Back();
	TestTrue(TEXT("Escape resumes pause"), Resume.IsSet() && Resume.GetValue() == A::Resume);
	State.Show(ETallowMenuMode::Outro);
	const TOptional<A> Replay = State.Activate(C::Restart);
	TestTrue(TEXT("Outro replay starts directly"), Replay.IsSet() && Replay.GetValue() == A::Restart);
	State.Show(ETallowMenuMode::Main);
	TestFalse(TEXT("Fresh menu re-arms requests"), State.bAwaitingAction);
	TestFalse(TEXT("Unavailable action ignored"), State.Activate(C::Resume).IsSet());
	return true;
}
#endif
