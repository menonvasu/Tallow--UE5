#include "UI/Presentation/TallowMenuWidget.h"
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
	if (Page != ETallowMenuPage::Home) return {C::Back};
	if (Mode == ETallowMenuMode::Pause) return {C::Resume, C::Controls, C::Restart, C::MainMenu, C::Quit};
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
	if (Command == C::Controls || Command == C::Credits)
	{
		Page = Command == C::Controls ? ETallowMenuPage::Controls : ETallowMenuPage::Credits;
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

UTallowMenuWidget::UTallowMenuWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
	static ConstructorHelpers::FObjectFinder<UFont> TitleFace(TEXT("/Game/Presentation/UI/Fonts/LiberationSerif_Font.LiberationSerif_Font"));
	TitleFont = TitleFace.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> Wordmark(TEXT("/Game/Presentation/UI/Textures/T_TallowWordmark.T_TallowWordmark"));
	TitleArtwork = Wordmark.Object;
	ControlsText = LOCTEXT("ControlsCopy", "WASD   Move\nMouse   Look\nE   Interact\nSpace   Jump / rock the chair\nHold RMB   Cup the flame\nR   Relight / put out the candle\nHold F   Stay still\nShift   Sprint\nCtrl   Crouch\nQ   Put down carried item\nEsc   Pause\n\nDrafts can snuff the candle. Cup it to protect it.\nRelighting makes noise. Running costs you your light.");
	CreditsText = LOCTEXT("CreditsCopy", "A game by\nTyron Oldroyd & Vasudev Menon\n\nCGRA252 · Team 9\nTe Herenga Waka—Victoria University of Wellington\n\nFurniture\nPoly Haven · CC0\n\nTypography\nLiberation Serif · SIL Open Font License\n© 2010 Google / © 2012 Red Hat");
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

void UTallowMenuWidget::Navigate(int32 Direction) { State.Navigate(Direction); }
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

FText UTallowMenuWidget::Heading() const
{
	switch (State.Page)
	{
	case ETallowMenuPage::Controls: return LOCTEXT("Controls", "CONTROLS");
	case ETallowMenuPage::Credits: return LOCTEXT("Credits", "CREDITS");
	case ETallowMenuPage::Confirm: return LOCTEXT("Leave", "LEAVE THIS RUN?");
	default: break;
	}
	if (State.Mode == ETallowMenuMode::Pause) return LOCTEXT("Pause", "TAKE A BREATH");
	if (State.Mode == ETallowMenuMode::Outro) return LOCTEXT("Ending", "THE FLAME IS OUT.");
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
	case C::Start: return LOCTEXT("Start", "Start");
	case C::Resume: return LOCTEXT("Resume", "Resume");
	case C::Restart: return State.Mode == ETallowMenuMode::Outro ? LOCTEXT("Replay", "Play again") : LOCTEXT("Restart", "Restart");
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

TSharedRef<SWidget> UTallowMenuWidget::RebuildWidget()
{
	TSharedRef<SWidget> Root = SNew(SOverlay)
	+ SOverlay::Slot()
	[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.Visibility(EVisibility::HitTestInvisible)
		.BorderBackgroundColor_Lambda([this]()
		{
			const bool bVeil = State.Mode != ETallowMenuMode::Main || State.Page != ETallowMenuPage::Home;
			return FSlateColor(FLinearColor(.009f, .008f, .007f, bVeil ? .88f : 0.f));
		})
	]
	+ SOverlay::Slot().Padding(FMargin(54.f, 38.f, 24.f, 38.f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(.40f)
		[
			SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
			.HAlign(HAlign_Left).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(460.f).HeightOverride_Lambda([this]()
				{
					return FOptionalSize(State.Mode == ETallowMenuMode::Main && State.Page == ETallowMenuPage::Home ? 560.f : 700.f);
				})
				[
					SAssignNew(Content, SVerticalBox)
				]
			]
		]
		+ SHorizontalBox::Slot().FillWidth(.60f)[SNew(SBox)]
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
	const FLinearColor Ivory(.87f, .82f, .69f, 1.f);
	if (TitleArtwork)
	{
		TitleBrush.SetResourceObject(TitleArtwork);
		TitleBrush.SetImageSize(FVector2D(TitleArtwork->GetSizeX(), TitleArtwork->GetSizeY()));
		TitleBrush.DrawAs = ESlateBrushDrawType::Image;
		Content->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 0.f, 0.f, 48.f)
		[
			SNew(SBox).WidthOverride(460.f).HeightOverride(180.f)
			[
				SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
				[
					SNew(SImage).Image(&TitleBrush)
				]
			]
		];
	}
	else
	{
		Content->AddSlot().AutoHeight().Padding(8.f, 25.f, 0.f, 70.f)
		[
			SNew(STextBlock).Text(LOCTEXT("Title", "T A L L O W"))
			.Font(TitleFont ? FSlateFontInfo(TitleFont, 48) : FCoreStyle::GetDefaultFontStyle("Light", 48)).ColorAndOpacity(Ivory)
		];
	}
	if (!Heading().IsEmpty())
	{
		Content->AddSlot().AutoHeight().Padding(10.f, 0.f, 0.f, 26.f)
		[
			SNew(STextBlock).Text(Heading())
			.Font(TitleFont ? FSlateFontInfo(TitleFont, 21) : FCoreStyle::GetDefaultFontStyle("Regular", 21))
			.ColorAndOpacity(Ivory)
		];
	}
	const bool bReading = State.Page == ETallowMenuPage::Controls || State.Page == ETallowMenuPage::Credits;
	if (!Body().IsEmpty())
	{
		auto BodySlot = Content->AddSlot();
		BodySlot.Padding(0.f, 0.f, 0.f, 24.f);
		if (bReading) BodySlot.FillHeight(1.f);
		else BodySlot.AutoHeight();
		BodySlot
		[
			SAssignNew(ReadingScroll, SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(STextBlock).Text(Body()).AutoWrapText(true).LineHeightPercentage(1.25f)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 17)).ColorAndOpacity(Ivory)
			]
		];
	}
	const TArray<ETallowMenuCommand> Commands = State.Commands();
	for (int32 I = 0; I < Commands.Num(); ++I)
	{
		const ETallowMenuCommand Command = Commands[I];
		const float Indents[] = {12.f, 18.f, 9.f, 15.f, 11.f};
		Content->AddSlot().AutoHeight().Padding(Indents[I % 5], 0.f, 0.f, I % 2 == 0 ? 12.f : 15.f)
		[
			SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
			.IsFocusable(false).HAlign(HAlign_Left).ContentPadding(FMargin(0.f, 10.f))
			.IsEnabled_Lambda([this]() { return !State.bAwaitingAction; })
			.OnHovered_Lambda([this, I]() { if (!State.bAwaitingAction) State.Selection = I; })
			.OnClicked_Lambda([this, Command]() { ActivateCommand(Command); return FReply::Handled(); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 16.f, 0.f)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("•")))
					.Font(TitleFont ? FSlateFontInfo(TitleFont, 27) : FCoreStyle::GetDefaultFontStyle("Regular", 27))
					.ColorAndOpacity_Lambda([this, I]() { return State.Selection == I ? FSlateColor(FLinearColor(.56f, .065f, .045f, 1.f)) : FSlateColor(FLinearColor::Transparent); })
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(STextBlock).Text(Label(Command)).Font(TitleFont ? FSlateFontInfo(TitleFont, 29) : FCoreStyle::GetDefaultFontStyle("Regular", 29))
					.ColorAndOpacity_Lambda([this, I, Ivory]() { return FSlateColor(State.Selection == I ? Ivory : FLinearColor(.68f, .61f, .49f, 1.f)); })
				]
			]
		];
	}
	if (!bReading) Content->AddSlot().FillHeight(1.f)[SNew(SBox)];
	Content->AddSlot().AutoHeight().Padding(0.f, 22.f, 0.f, 0.f)
	[
		SNew(STextBlock).Text(bReading
			? LOCTEXT("ReadingNavigation", "PgUp / PgDn  Scroll     Enter / Esc  Back")
			: LOCTEXT("Navigation", "↑ ↓  Choose     Enter  Select     Esc  Back"))
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 15)).ColorAndOpacity(FLinearColor(.60f, .55f, .45f, 1.f))
	];
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
	if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up) { Navigate(-1); return FReply::Handled(); }
	if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down) { Navigate(1); return FReply::Handled(); }
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
