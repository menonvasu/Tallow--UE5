#include "UI/Presentation/TallowMenuWidget.h"
#include "Audio/TallowCorruptedVoice.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Save/TallowSaveSubsystem.h"
#include "Input/TallowUIInput.h"
#include "Input/TallowInputPrompts.h"
#include "Input/TallowInputDeviceSubsystem.h"
#include "Input/TallowGamepadContext.h"
#include "Input/TallowHaptics.h"
#include "Widgets/Input/SSlider.h"
#include "UI/Presentation/STallowMark.h"
#include "UI/Presentation/TallowPaper.h"
#include "Presentation/TallowPlaytestSubsystem.h"
#include "Engine/Texture2D.h"
#include "Widgets/Images/SImage.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SLeafWidget.h"

#define LOCTEXT_NAMESPACE "TallowMenu"

void FTallowMenuState::Show(ETallowMenuMode InMode)
{
	Mode = InMode;
	Page = ETallowMenuPage::Home;
	Selection = ReturnSelection = Mode == ETallowMenuMode::Main && !bContinueAvailable ? 1 : 0;
	bAwaitingAction = false;
	PendingConfirmation.Reset();
}

TArray<ETallowMenuCommand> FTallowMenuState::Commands() const
{
	using C = ETallowMenuCommand;
	if (Page == ETallowMenuPage::Confirm) return {C::Cancel, C::Confirm};
	if (Page == ETallowMenuPage::Settings) return {C::Volume, C::Brightness, C::Vibration, C::InvertLook, C::InvertMove, C::HiddenVoice, C::Back};
	if (Page != ETallowMenuPage::Home) return {C::Back};
	if (Mode == ETallowMenuMode::Pause) return {C::Resume, C::Settings, C::Controls, C::Restart, C::MainMenu, C::Quit};
	if (Mode == ETallowMenuMode::Caught) return {C::Restart, C::MainMenu};
	if (Mode == ETallowMenuMode::Outro) return {C::Restart, C::MainMenu, C::Credits};
	return {C::Continue, C::Start, C::Settings, C::Controls, C::Credits, C::Quit};
}

bool FTallowMenuState::IsEnabled(ETallowMenuCommand Command) const
{
	return Command != ETallowMenuCommand::Continue || bContinueAvailable;
}

void FTallowMenuState::Navigate(int32 Direction)
{
	if (bAwaitingAction || Direction == 0) return;
	const int32 Count = Commands().Num();
	for (int32 I = 0; I < Count; ++I)
	{
		Selection = (Selection + (Direction > 0 ? 1 : -1) + Count) % Count;
		if (IsEnabled(Commands()[Selection])) break;
	}
}

TOptional<ETallowMenuAction> FTallowMenuState::Request(ETallowMenuAction Action)
{
	bAwaitingAction = true; // Set before delegates run: repeated keys cannot load twice.
	return Action;
}

void FTallowMenuState::ReturnHome()
{
	Page = ETallowMenuPage::Home;
	PendingConfirmation.Reset();
	Selection = FMath::Clamp(ReturnSelection, 0, Commands().Num() - 1);
}

TOptional<ETallowMenuAction> FTallowMenuState::Activate(ETallowMenuCommand Command)
{
	using C = ETallowMenuCommand;
	using A = ETallowMenuAction;
	if (bAwaitingAction || !Commands().Contains(Command) || !IsEnabled(Command)) return {};
	Selection = Commands().IndexOfByKey(Command);
	if (Command == C::BabyMode || Command == C::Vibration || Command == C::InvertLook || Command == C::InvertMove || Command == C::HiddenVoice || Command == C::Volume || Command == C::Brightness) return {};
	if (Command == C::Back || Command == C::Cancel) { ReturnHome(); return {}; }
	if (Command == C::Confirm)
	{
		if (!PendingConfirmation.IsSet()) return {};
		const A Action = PendingConfirmation.GetValue();
		PendingConfirmation.Reset();
		return Request(Action);
	}
	ReturnSelection = Selection;
	if (Command == C::Controls || Command == C::Credits || Command == C::Settings)
	{
		Page = Command == C::Controls ? ETallowMenuPage::Controls : Command == C::Settings ? ETallowMenuPage::Settings : ETallowMenuPage::Credits;
		Selection = 0;
		return {};
	}
	A Action;
	switch (Command)
	{
	case C::Continue: Action = A::Continue; break;
	case C::Start: Action = A::Start; break;
	case C::Resume: Action = A::Resume; break;
	case C::Restart: Action = A::Restart; break;
	case C::MainMenu: Action = A::MainMenu; break;
	case C::Quit: Action = A::Quit; break;
	default: return {};
	}
	if ((Mode == ETallowMenuMode::Pause && Action != A::Resume)
		|| (Action == A::Start && bContinueAvailable))
	{
		PendingConfirmation = Action;
		Page = ETallowMenuPage::Confirm;
		Selection = 0; // Safe default: cancel, including keyboard confirmation.
		return {};
	}
	return Request(Action);
}

TOptional<ETallowMenuAction> FTallowMenuState::Back()
{
	if (bAwaitingAction) return {};
	if (Page != ETallowMenuPage::Home) { ReturnHome(); return {}; }
	if (Mode == ETallowMenuMode::Pause) return Request(ETallowMenuAction::Resume);
	return {};
}

namespace
{
	/** A setting's slider: a dust hairline, the filled part in ivory, a short ivory tick at the value. Click or drag sets it. */
	class STallowSlider : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(STallowSlider) : _Chosen(false) {}
			SLATE_ATTRIBUTE(float, Value)
			SLATE_ATTRIBUTE(bool, Chosen)
			SLATE_EVENT(FOnFloatValueChanged, OnValueChanged)
		SLATE_END_ARGS()
		void Construct(const FArguments& A) { Value = A._Value; Chosen = A._Chosen; Changed = A._OnValueChanged; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(260.f, 30.f); }
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& O, int32 L, const FWidgetStyle&, bool) const override
		{
			const FVector2D Size = G.GetLocalSize();
			const float Y = Size.Y * .5f, Left = 4.f, Right = Size.X - 4.f;
			const float X = FMath::Lerp(Left, Right, FMath::Clamp(Value.Get(), 0.f, 1.f));
			const FLinearColor Lit = Chosen.Get() ? TallowPaper::Ivory : TallowPaper::Ink;
			TallowPaper::DrawLine(O, L, G, {Left, Y}, {Right, Y}, TallowPaper::Pencil.CopyWithNewOpacity(.45f), 1.f, 77);
			TallowPaper::DrawLine(O, L + 1, G, {Left, Y}, {X, Y}, Lit, 2.f, 78);
			TallowPaper::DrawLine(O, L + 2, G, {X, Y - 9.f}, {X, Y + 9.f}, Lit, 2.f, 79);
			return L + 3;
		}
		virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override
		{
			if (E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
			Set(G, E);
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
		virtual FReply OnMouseMove(const FGeometry& G, const FPointerEvent& E) override
		{
			if (!HasMouseCapture()) return FReply::Unhandled();
			Set(G, E);
			return FReply::Handled();
		}
		virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override { return HasMouseCapture() ? FReply::Handled().ReleaseMouseCapture() : FReply::Unhandled(); }
	private:
		void Set(const FGeometry& G, const FPointerEvent& E)
		{
			const float X = G.AbsoluteToLocal(E.GetScreenSpacePosition()).X;
			Changed.ExecuteIfBound(FMath::Clamp((X - 4.f) / FMath::Max(1.f, float(G.GetLocalSize().X) - 8.f), 0.f, 1.f));
		}
		TAttribute<float> Value;
		TAttribute<bool> Chosen;
		FOnFloatValueChanged Changed;
	};

	float VolumeValue() { return FApp::GetVolumeMultiplier(); }
	void SetVolume(float V) { FApp::SetVolumeMultiplier(FMath::Clamp(V, 0.f, 1.f)); }
	float BrightnessValue() { return GEngine ? FMath::Clamp((GEngine->DisplayGamma - 1.35f) / .8f, 0.f, 1.f) : .5f; }
	void SetBrightness(float V) { if (GEngine) GEngine->DisplayGamma = 1.35f + FMath::Clamp(V, 0.f, 1.f) * .8f; }
}

UTallowMenuWidget::UTallowMenuWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
	ControlsText = LOCTEXT("ControlsCopy", "WASD   move\nmouse   look\nE   use, pick up, read\nspace   jump, rock the chair\nhold right mouse   cup the flame\nR   strike a match, or put the candle out\nhold F   hold your breath\nhold left mouse   tension, when picking a lock\nshift   run\nctrl   crouch\nQ   put down what you carry\nI / Tab   look through your things\nM   unfold your map, once found\nesc   pause\n\ndraughts can snuff the candle. cup it to keep it.\nstriking a match is loud. running costs you your light.");
	// Blocks: a heading line, then its lines. Set as heads in ochre capitals and names in Fell.
	CreditsText = LOCTEXT("CreditsCopy", "a game by\nTyron Oldroyd and Vasudev Menon\n\nmade for\nCGRA252, Team 9\nTe Herenga Waka, Victoria University of Wellington\n\nsound\nour own recordings, with CC0 sources from Freesound, BigSoundBank and Kenney\n\nfurniture\nPoly Haven, CC0\n\ntypography\nIM Fell English, Igino Marini\nShadows Into Light Two and Gloria Hallelujah, Kimberly Geswein\nSIL Open Font License");
}

void UTallowMenuWidget::ShowMenu(ETallowMenuMode Mode)
{
	const auto* Save = UTallowSaveSubsystem::Get(this);
	State.bContinueAvailable = Save && Save->HasContinue();
	State.Show(Mode);
	SetVisibility(ESlateVisibility::Visible);
	Refresh();
	if (GetCachedWidget().IsValid() && GetOwningLocalPlayer()) SetFocus();
}

void UTallowMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (GetOwningLocalPlayer()) SetFocus();
}

void UTallowMenuWidget::Navigate(int32 Direction) { NoteInput(); State.Navigate(Direction); }
void UTallowMenuWidget::ActivateSelected()
{
	const TArray<ETallowMenuCommand> Commands = State.Commands();
	if (Commands.IsValidIndex(State.Selection)) ActivateCommand(Commands[State.Selection]);
}
void UTallowMenuWidget::ActivateCommand(ETallowMenuCommand Command)
{
	NoteInput();
	if (Command == ETallowMenuCommand::BabyMode && !State.bAwaitingAction && State.Commands().Contains(Command))
		UTallowPlaytestSubsystem::SetBabyModeEnabled(!UTallowPlaytestSubsystem::IsBabyModeEnabled());
	if (Command == ETallowMenuCommand::Vibration && !State.bAwaitingAction && State.Commands().Contains(Command))
	{
		const bool bEnabled = !TallowHaptics::IsEnabled();
		TallowHaptics::SetEnabled(bEnabled);
		if (GConfig)
		{
			GConfig->SetBool(TEXT("Tallow.Settings"), TEXT("Vibration"), bEnabled, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}
	if (Command == ETallowMenuCommand::InvertLook && !State.bAwaitingAction && State.Commands().Contains(Command))
		UTallowGamepadContext::SetInvertLook(!UTallowGamepadContext::InvertLook());
	if (Command == ETallowMenuCommand::InvertMove && !State.bAwaitingAction && State.Commands().Contains(Command))
		UTallowGamepadContext::SetInvertMove(!UTallowGamepadContext::InvertMove());
	if (Command == ETallowMenuCommand::HiddenVoice && !State.bAwaitingAction && State.Commands().Contains(Command))
		TallowCorrupted::SetShowHiddenVoice(!TallowCorrupted::ShowHiddenVoice());
	Dispatch(State.Activate(Command));
}
void UTallowMenuWidget::GoBack() { NoteInput(); Dispatch(State.Back()); }
void UTallowMenuWidget::RearmActions()
{
	// A failed action returns to the home menu; a consumed confirmation is never reused.
	const ETallowMenuMode Mode = State.Mode;
	ShowMenu(Mode);
}
void UTallowMenuWidget::Dispatch(TOptional<ETallowMenuAction> Action)
{
	Refresh();
	if (Action.IsSet()) OnActionRequested.Broadcast(Action.GetValue());
}

bool UTallowMenuWidget::Adjust(ETallowMenuCommand Command, float Delta)
{
	if (State.bAwaitingAction) return false;
	if (Command == ETallowMenuCommand::Volume) { SetVolume(VolumeValue() + Delta); return true; }
	if (Command == ETallowMenuCommand::Brightness) { SetBrightness(BrightnessValue() + Delta); return true; }
	return false;
}

FText UTallowMenuWidget::Heading() const
{
	switch (State.Page)
	{
	case ETallowMenuPage::Settings: return LOCTEXT("Settings", "Settings");
	case ETallowMenuPage::Controls: return LOCTEXT("Controls", "Controls");
	case ETallowMenuPage::Credits: return LOCTEXT("Credits", "Credits");
	case ETallowMenuPage::Confirm: return State.PendingConfirmation == ETallowMenuAction::Start
		? LOCTEXT("NewRun", "Start again?") : LOCTEXT("Leave", "Leave this run?");
	default: break;
	}
	if (State.Mode == ETallowMenuMode::Caught) return LOCTEXT("Caught", "Caught");
	if (State.Mode == ETallowMenuMode::Pause) return LOCTEXT("Pause", "Paused");
	if (State.Mode == ETallowMenuMode::Outro) return LOCTEXT("Ending", "The flame is out.");
	return FText::GetEmpty();
}

FText UTallowMenuWidget::Body() const
{
	if (State.Page == ETallowMenuPage::Confirm)
	{
		if (State.PendingConfirmation == ETallowMenuAction::Start) return LOCTEXT("ReplaceSave", "A new game will replace your saved chapter.");
		if (State.PendingConfirmation == ETallowMenuAction::Restart && State.bContinueAvailable) return LOCTEXT("RestartChapter", "Return to the start of your saved chapter?");
		return State.bContinueAvailable ? LOCTEXT("LoseChapterProgress", "Your progress since the start of this chapter will be lost.") : LOCTEXT("LoseProgress", "Your progress in this run will be lost.");
	}
	if (State.Page == ETallowMenuPage::Home && State.Mode == ETallowMenuMode::Caught) return LOCTEXT("CaughtLine", "It carried you back to the chair.");
	return FText::GetEmpty();
}

FText UTallowMenuWidget::Label(ETallowMenuCommand Command) const
{
	using C = ETallowMenuCommand;
	switch (Command)
	{
	case C::Continue: return LOCTEXT("Continue", "Continue");
	case C::Start: return LOCTEXT("Start", "New game");
	case C::Settings: return LOCTEXT("SettingsButton", "Settings");
	case C::Volume: return LOCTEXT("Volume", "Sound");
	case C::Brightness: return LOCTEXT("Gamma", "Brightness");
	case C::BabyMode: return LOCTEXT("BabyMode", "Ty is a baby");
	case C::Vibration: return LOCTEXT("Vibration", "Vibration");
	case C::InvertLook: return LOCTEXT("InvertLook", "Invert look up/down");
	case C::InvertMove: return LOCTEXT("InvertMove", "Invert move up/down");
	case C::HiddenVoice: return LOCTEXT("HiddenVoice", "Show hidden voice");
	case C::Resume: return LOCTEXT("Resume", "Resume");
	case C::Restart: if (State.Mode == ETallowMenuMode::Caught) return LOCTEXT("Retry", "Try again"); return State.Mode == ETallowMenuMode::Outro ? LOCTEXT("Replay", "Play again") : LOCTEXT("Restart", "Restart");
	case C::MainMenu: return LOCTEXT("MainMenu", "Main menu");
	case C::Quit: return LOCTEXT("Quit", "Quit");
	case C::Controls: return LOCTEXT("ControlsButton", "Controls");
	case C::Credits: return LOCTEXT("CreditsButton", "Credits");
	case C::Back: return LOCTEXT("Back", "Back");
	case C::Cancel: return State.Mode == ETallowMenuMode::Main ? LOCTEXT("CancelNew", "Cancel") : LOCTEXT("Cancel", "Keep playing");
	case C::Confirm:
		if (State.PendingConfirmation == ETallowMenuAction::Start) return LOCTEXT("ConfirmStart", "New game");
		if (State.PendingConfirmation == ETallowMenuAction::Restart) return State.bContinueAvailable ? LOCTEXT("ConfirmRestartChapter", "Restart chapter") : LOCTEXT("ConfirmRestart", "Restart this run");
		if (State.PendingConfirmation == ETallowMenuAction::Quit) return LOCTEXT("ConfirmQuit", "Quit game");
		return LOCTEXT("ConfirmMenu", "Return to main menu");
	default: return FText::GetEmpty();
	}
}

FText UTallowMenuWidget::Value(ETallowMenuCommand Command) const
{
	using C = ETallowMenuCommand;
	auto OnOff = [](bool bOn) { return bOn ? LOCTEXT("On", "on") : LOCTEXT("Off", "off"); };
	switch (Command)
	{
	case C::BabyMode: return OnOff(UTallowPlaytestSubsystem::IsBabyModeEnabled());
	case C::Vibration: return OnOff(TallowHaptics::IsEnabled());
	case C::InvertLook: return OnOff(UTallowGamepadContext::InvertLook());
	case C::InvertMove: return OnOff(UTallowGamepadContext::InvertMove());
	case C::HiddenVoice: return OnOff(TallowCorrupted::ShowHiddenVoice());
	default: return FText::GetEmpty();
	}
}

FText UTallowMenuWidget::SettingNote(ETallowMenuCommand Command) const
{
	using C = ETallowMenuCommand;
	switch (Command)
	{
	case C::Volume: return LOCTEXT("VolumeHelp", "Everything you hear. Left and right to change it.");
	case C::Brightness: return LOCTEXT("GammaHelp", "With the candle out you should barely see.");
	case C::BabyMode: return LOCTEXT("BabyModeHelp", "Playtest: lights on, with a note about Mim.");
	case C::Vibration: return LOCTEXT("VibrationHelp", "The pad trembles when something is near.");
	case C::InvertLook: return LOCTEXT("InvertLookHelp", "Controller only. Pushing the right stick up looks down.");
	case C::InvertMove: return LOCTEXT("InvertMoveHelp", "Controller only. Pushing the left stick up walks backward.");
	case C::HiddenVoice: return LOCTEXT("HiddenVoiceHelp", "Write down the other voice in the recordings.");
	default: return FText::GetEmpty();
	}
}

FString UTallowMenuWidget::FooterKeys() const
{
	const bool bPad = UTallowInputDeviceSubsystem::GetCurrent(this) == ETallowInputDevice::Gamepad;
	const bool bSlider = State.Page == ETallowMenuPage::Settings;
	if (bPad) return bSlider ? TEXT("[ d-pad ]  choose and change      [ cross ]  open      [ circle ]  back") : TEXT("[ d-pad ]  choose      [ cross ]  open      [ circle ]  back");
	return bSlider ? TEXT("[ arrows ]  choose and change      [ enter ]  open      [ esc ]  back") : TEXT("[ arrows ]  choose      [ enter ]  open      [ esc ]  back");
}

TSharedRef<SWidget> UTallowMenuWidget::MakeItem(int32 Index, ETallowMenuCommand Command, int32 FontSize)
{
	using C = ETallowMenuCommand;
	const bool bSlider = Command == C::Volume || Command == C::Brightness;
	const bool bValue = !Value(Command).IsEmpty();
	auto Chosen = [this, Index]() { return State.Selection == Index; };
	// Chosen: ivory, with the red thread run under it and on toward the picture. The rest wait in dust.
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
	+ SHorizontalBox::Slot().AutoWidth()
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Text(Label(Command)).Font(TallowPaper::Fell(FontSize))
			.ColorAndOpacity_Lambda([this, Index, Command]() { return FSlateColor(!State.IsEnabled(Command) ? TallowPaper::Pencil.CopyWithNewOpacity(.35f) : State.Selection == Index ? TallowPaper::Ivory : TallowPaper::Pencil); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(-4.f, -2.f, -46.f, 0.f))
		[
			SNew(SBox).HeightOverride(8.f)[SNew(STallowThread).Seed(60 + Index).Shown_Lambda(Chosen)]
		]
	];
	if (bSlider || bValue)
	{
		Row->AddSlot().FillWidth(1.f)[SNew(SBox)];
		if (bSlider)
			Row->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(FMargin(30.f, FontSize * .28f, 0.f, 0.f))
			[
				SNew(SBox).WidthOverride(260.f).HeightOverride(30.f)
				[
					SNew(STallowSlider).Chosen_Lambda(Chosen)
					.Value_Lambda([Command]() { return Command == ETallowMenuCommand::Volume ? VolumeValue() : BrightnessValue(); })
					.OnValueChanged_Lambda([this, Command, Index](float V) { NoteInput(); State.Selection = Index; Command == ETallowMenuCommand::Volume ? SetVolume(V) : SetBrightness(V); })
				]
			];
		else
			Row->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(FMargin(30.f, FontSize * .12f, 0.f, 0.f))
			[
				SNew(STextBlock).Text_Lambda([this, Command]() { return Value(Command); }).Font(TallowPaper::FellItalic(FontSize - 4))
				.ColorAndOpacity_Lambda([Chosen]() { return FSlateColor(Chosen() ? TallowPaper::Ivory : TallowPaper::Pencil); })
			];
	}
	return SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
		.IsFocusable(false).HAlign(HAlign_Fill).ContentPadding(FMargin(0.f, 4.f))
		.Visibility(State.IsEnabled(Command) ? EVisibility::Visible : EVisibility::Collapsed)
		.IsEnabled_Lambda([this, Command]() { return !State.bAwaitingAction && State.IsEnabled(Command); })
		.OnHovered_Lambda([this, Index, Command]() { if (!State.bAwaitingAction && State.IsEnabled(Command) && State.Selection != Index) { State.Selection = Index; NoteInput(); } })
		.OnClicked_Lambda([this, Command]() { ActivateCommand(Command); return FReply::Handled(); })
		[Row];
}

TSharedRef<SWidget> UTallowMenuWidget::MakeControls() const
{
	using A = ETallowPromptAction;
	const ETallowInputDevice Device = UTallowInputDeviceSubsystem::GetCurrent(this);
	const bool bPad = Device == ETallowInputDevice::Gamepad;
	auto K = [Device](A Action) { return TallowInputPrompts::Get(Action, Device).Label.ToString(); };
	struct FRow { FString Keys; FString Action; };
	struct FGroup { FString Head; TArray<FRow> Rows; };
	const TArray<FGroup> Groups = {
		{TEXT("MOVE"), {{bPad ? TEXT("left stick") : TEXT("W A S D"), TEXT("walk")}, {bPad ? TEXT("right stick") : TEXT("mouse"), TEXT("look")},
			{K(A::Run), TEXT("run")}, {K(A::Crouch), TEXT("crouch")}, {K(A::RockChair), TEXT("jump, rock the chair")}}},
		{TEXT("CANDLE"), {{K(A::Cup), TEXT("hold to cup the flame")}, {K(A::Relight), TEXT("strike a match, or put it out")}}},
		{TEXT("SURVIVE"), {{K(A::HoldBreath), TEXT("hold your breath")}, {K(A::Tension), TEXT("hold for tension on a lock")}}},
		{TEXT("CARRY"), {{K(A::Interact), TEXT("use, pick up, listen")}, {K(A::Drop), TEXT("put down what you carry")},
			{K(A::Inventory), TEXT("look through your things")}, {K(A::Map), TEXT("unfold your map, once found")}, {K(A::Pause), TEXT("pause")}}}};
	auto Column = [](const TArray<FGroup>& In) -> TSharedRef<SVerticalBox>
	{
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		for (const FGroup& Group : In)
		{
			Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f)
			[SNew(STextBlock).Text(FText::FromString(Group.Head)).Font(TallowPaper::Caps(TallowPaper::Pt::Label)).ColorAndOpacity(TallowPaper::Ochre)];
			for (const FRow& Row : Group.Rows)
				Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 9.f)
				[SNew(STallowKeys).Text(FString::Printf(TEXT("[ %s ]  %s"), *Row.Keys, *Row.Action)).Font(TallowPaper::Fell(TallowPaper::Pt::Body)).Colour(TallowPaper::Ink).KeyColour(TallowPaper::Ivory)];
			Box->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(30.f)];
		}
		return Box;
	};
	return SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 90.f, 0.f)[Column({Groups[0], Groups[1]})]
		+ SHorizontalBox::Slot().AutoWidth()[Column({Groups[2], Groups[3]})]
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
	[
		SNew(STextBlock).Text(LOCTEXT("ControlsNote", "Draughts can snuff the candle; cup it to keep it.\nA struck match is loud. Running costs you your light."))
		.Font(TallowPaper::FellItalic(TallowPaper::Pt::Body)).ColorAndOpacity(TallowPaper::Pencil).LineHeightPercentage(1.15f)
	];
}

TSharedRef<SWidget> UTallowMenuWidget::MakeCredits() const
{
	TArray<FString> Blocks;
	CreditsText.ToString().ParseIntoArray(Blocks, TEXT("\n\n"));
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	for (const FString& Block : Blocks)
	{
		FString Head, Rest;
		if (!Block.Split(TEXT("\n"), &Head, &Rest)) { Head = Block; }
		Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
		[SNew(STextBlock).Text(FText::FromString(Head.ToUpper())).Font(TallowPaper::Caps(TallowPaper::Pt::Label)).ColorAndOpacity(TallowPaper::Ochre)];
		if (!Rest.IsEmpty())
			Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 30.f)
			[SNew(STextBlock).Text(FText::FromString(Rest)).Font(TallowPaper::Fell(TallowPaper::Pt::Body)).ColorAndOpacity(TallowPaper::Ink).AutoWrapText(true).LineHeightPercentage(1.12f)];
	}
	return Box;
}

TSharedRef<SWidget> UTallowMenuWidget::RebuildWidget()
{
	if (!TitleArtwork) TitleArtwork = LoadObject<UTexture2D>(nullptr, TEXT("/Game/FinalPass/UI/T_Wordmark.T_Wordmark"), nullptr, LOAD_NoWarn);
	if (TitleArtwork)
	{
		TitleBrush = FSlateBrush();
		TitleBrush.SetResourceObject(TitleArtwork);
		TitleBrush.ImageSize = FVector2D(TitleArtwork->GetSizeX(), TitleArtwork->GetSizeY());
		TitleBrush.TintColor = FSlateColor(FLinearColor::White);
	}
	// Home over the picture: only soot falling from the left, so the close-up stays whole.
	// Every other page, and pause or caught, lays the soot ground over the game as well.
	auto Veiled = [this]() { return State.Mode != ETallowMenuMode::Main || State.Page != ETallowMenuPage::Home; };
	TSharedRef<SWidget> Root = SNew(SOverlay)
	+ SOverlay::Slot()
	[
		SNew(SBorder).BorderImage(TallowPaper::Ground())
		.Visibility(EVisibility::HitTestInvisible)
		.BorderBackgroundColor_Lambda([Veiled]() { return FSlateColor(FLinearColor(1.f, 1.f, 1.f, Veiled() ? .86f : 0.f)); })
	]
	+ SOverlay::Slot()
	[
		SNew(SHorizontalBox).Visibility(EVisibility::HitTestInvisible)
		+ SHorizontalBox::Slot().FillWidth(.62f)[SNew(STallowScrim).FromLeft(true).Alpha_Lambda([Veiled]() { return Veiled() ? .7f : .93f; })]
		+ SHorizontalBox::Slot().FillWidth(.38f)[SNew(SBox)]
	]
	+ SOverlay::Slot().Padding(FMargin(150.f, 96.f, 60.f, 64.f))
	[
		SAssignNew(Content, SVerticalBox)
	];
	Refresh();
	return Root;
}

void UTallowMenuWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Content.Reset();
	ReadingScroll.Reset();
}

void UTallowMenuWidget::Refresh()
{
	if (!Content.IsValid()) return;
	Content->ClearChildren();
	ReadingScroll.Reset();
	using namespace TallowPaper;
	const bool bHome = State.Page == ETallowMenuPage::Home;
	const bool bTitle = bHome && (State.Mode == ETallowMenuMode::Main || State.Mode == ETallowMenuMode::Outro);
	const bool bWide = State.Page == ETallowMenuPage::Controls || State.Page == ETallowMenuPage::Credits;
	TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
	Content->AddSlot().FillHeight(1.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(bWide ? 1120.f : 640.f)[Column]]
	];

	if (bTitle)
	{
		// The wordmark, then where and when, quietly.
		Column->AddSlot().AutoHeight().Padding(0.f, 40.f, 0.f, 0.f)
		[
			TitleArtwork
			? StaticCastSharedRef<SWidget>(SNew(SBox).WidthOverride(560.f).HeightOverride(150.f)[SNew(SImage).Image(&TitleBrush)])
			: StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(LOCTEXT("Title", "TALLOW")).Font(Caps(96, 260)).ColorAndOpacity(Ivory))
		];
		Column->AddSlot().AutoHeight().Padding(10.f, 14.f, 0.f, 0.f)
		[SNew(STextBlock).Text(LOCTEXT("Winter", "London, winter 1974")).Font(FellItalic(Pt::Body)).ColorAndOpacity(Pencil)];
		Column->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(State.Mode == ETallowMenuMode::Outro ? 70.f : 120.f)];
	}
	else
	{
		// Pages and states sit a little above the middle, under their heading.
		Column->AddSlot().FillHeight(bWide ? .08f : .62f)[SNew(SBox)];
	}

	if (!Heading().IsEmpty())
	{
		const bool bQuiet = bTitle; // the outro's line sits under the wordmark, in italic
		Column->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, Body().IsEmpty() ? 40.f : 14.f)
		[SNew(STextBlock).Text(Heading()).Font(bQuiet ? FellItalic(Pt::Item) : Fell(Pt::Heading)).ColorAndOpacity(Ivory)];
	}
	if (!Body().IsEmpty())
		Column->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 40.f)
		[SNew(STextBlock).Text(Body()).Font(FellItalic(Pt::Body)).ColorAndOpacity(Ink).AutoWrapText(true)];

	if (State.Page == ETallowMenuPage::Controls) Column->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 36.f)[MakeControls()];
	if (State.Page == ETallowMenuPage::Credits) Column->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 26.f)[MakeCredits()];

	const TArray<ETallowMenuCommand> Commands = State.Commands();
	const int32 ItemSize = State.Page == ETallowMenuPage::Settings ? Pt::Lead + 2 : Pt::Item;
	for (int32 I = 0; I < Commands.Num(); ++I)
	{
		// In settings, Back stands apart from the rows it leaves.
		const float Gap = State.Page == ETallowMenuPage::Settings && Commands[I] == ETallowMenuCommand::Back ? 26.f : 0.f;
		Column->AddSlot().AutoHeight().Padding(0.f, Gap, 0.f, 10.f)[MakeItem(I, Commands[I], ItemSize)];
	}
	if (State.Page == ETallowMenuPage::Settings)
		Column->AddSlot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)
		[
			SNew(STextBlock).AutoWrapText(true).Font(FellItalic(Pt::Small)).ColorAndOpacity(Pencil)
			.Text_Lambda([this]() { const auto C = State.Commands(); return C.IsValidIndex(State.Selection) ? SettingNote(C[State.Selection]) : FText::GetEmpty(); })
		];
	Column->AddSlot().FillHeight(1.f)[SNew(SBox)];
	Column->AddSlot().AutoHeight()
	[SNew(STallowKeys).Text_Lambda([this]() { return FooterKeys(); }).Font(FellItalic(Pt::Small + 2)).Colour(Pencil).KeyColour(Ink)];
}

FReply UTallowMenuWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	NoteInput();
	const ETallowPadIntent Pad = TallowUIInput::ClassifyGamepad(Key);
	if (ReadingScroll.IsValid() && (Key == EKeys::PageUp || Key == EKeys::PageDown))
	{
		const float Delta = Key == EKeys::PageUp ? -240.f : 240.f;
		ReadingScroll->SetScrollOffset(FMath::Max(0.f, ReadingScroll->GetScrollOffset() + Delta));
		return FReply::Handled();
	}
	if (Key == EKeys::Left || Key == EKeys::Right || Pad == ETallowPadIntent::Left || Pad == ETallowPadIntent::Right)
	{
		const TArray<ETallowMenuCommand> Commands = State.Commands();
		const float Step = (Key == EKeys::Left || Pad == ETallowPadIntent::Left) ? -.05f : .05f;
		if (Commands.IsValidIndex(State.Selection) && Adjust(Commands[State.Selection], Step)) return FReply::Handled();
	}
	if (Key == EKeys::Up || Pad == ETallowPadIntent::Up) { Navigate(-1); return FReply::Handled(); }
	if (Key == EKeys::Down || Pad == ETallowPadIntent::Down) { Navigate(1); return FReply::Handled(); }
	if (Key == EKeys::Tab) { Navigate(Event.IsShiftDown() ? -1 : 1); return FReply::Handled(); }
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Pad == ETallowPadIntent::Confirm)
	{
		if (!Event.IsRepeat()) ActivateSelected();
		return FReply::Handled();
	}
	if (Key == EKeys::Escape || Pad == ETallowPadIntent::Back || Pad == ETallowPadIntent::Menu)
	{
		if (!Event.IsRepeat()) GoBack();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}

#undef LOCTEXT_NAMESPACE
