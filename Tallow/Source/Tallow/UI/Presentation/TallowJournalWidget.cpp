#include "UI/Presentation/TallowJournalWidget.h"
#include "Sound/SoundWave.h"
#include "Audio/TallowSoundscapeComponent.h"
#include "Presentation/TallowPlayDirector.h"

#include "UI/Presentation/TallowPaper.h"
#include "Input/TallowInputPrompts.h"
#include "Input/TallowUIInput.h"
#include "Input/TallowInputDeviceSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/Presentation/STallowMark.h"
#include "Presentation/TallowNarrative.h"
#include "Audio/TallowKeeperVoiceComponent.h"
#include "AI/TallowCreature.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Components/AudioComponent.h"
#include "GameMode/TallowGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Player/TallowCharacter.h"
#include "Presentation/TallowStorySubsystem.h"
#include "Audio/TallowCorruptedVoice.h"
#include "HAL/IConsoleManager.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "TallowJournal"

/** The tape's progress: a dust hairline, the part already played run in the red thread, an ivory tick where it is. */
class SReadingLine : public SLeafWidget
{
public:
 SLATE_BEGIN_ARGS(SReadingLine):_Fraction(0.f),_Available(false),_OnDark(false){}
  SLATE_ATTRIBUTE(float,Fraction) SLATE_ATTRIBUTE(bool,Available) SLATE_ATTRIBUTE(bool,OnDark)
 SLATE_END_ARGS()
 void Construct(const FArguments& A){Fraction=A._Fraction;Available=A._Available;OnDark=A._OnDark;}
 FVector2D ComputeDesiredSize(float)const override{return FVector2D(180,20);}
 int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool)const override
 {
  const float Left=3.f,Right=FMath::Max(Left+1.f,float(G.GetLocalSize().X)-3.f),Y=float(G.GetLocalSize().Y)*.5f;
  const auto Track=(OnDark.Get()?TallowPaper::Pencil:TallowPaper::PaperInk).CopyWithNewOpacity(.4f);
  TallowPaper::DrawLine(Out,Layer,G,{Left,Y},{Right,Y},Track,1.f,91);
  if(Available.Get())
  {
   const float X=FMath::Lerp(Left,Right,FMath::Clamp(Fraction.Get(),0.f,1.f));
   if(X>Left+1.f)TallowPaper::DrawThread(Out,Layer+1,G,{Left,Y},{X,Y},103);
   TallowPaper::DrawLine(Out,Layer+3,G,{X,Y-6.f},{X,Y+6.f},OnDark.Get()?TallowPaper::Ivory:TallowPaper::PaperInk,1.6f,104);
  }
  return Layer+4;
 }
private:TAttribute<float> Fraction;TAttribute<bool> Available;TAttribute<bool> OnDark;
};

UTallowJournalWidget::UTallowJournalWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UTallowJournalWidget::Open(APlayerController* InController, const TArray<FText>& InPages, bool bInPrinted, FName InDocumentKey)
{
 if(!InController||InPages.IsEmpty())return;
 const FName Key=InDocumentKey.IsNone()?TallowJournalVoices::Identify(InPages):InDocumentKey;
 TArray<UUserWidget*> Readers;
 UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Readers,UTallowJournalWidget::StaticClass(),false);
 for(auto* Reader:Readers)if(auto* Other=Cast<UTallowJournalWidget>(Reader))
 {
  if(Other->bOpen&&Other->DocumentKey==Key&&!Key.IsNone())
  {Other->bCompact=false;Other->TakeReadingInput();Other->InvalidateLayoutAndVolatility();return;}
  if(Other!=this)Other->SkipReading();
 }
 if(bOpen)SkipReading();
 ConfigureDocument(InPages,bInPrinted,Key);bOpen=true;bCompact=false;Controller=InController;
 if(const auto* Story=UTallowStorySubsystem::GetStory(this))bNarrationMuted=Story->IsDocumentNarrationMuted();
 if(const auto* Player=Cast<ATallowCharacter>(InController->GetPawn()))SeenAtOpen=Player->SeenCallAt;
 if(auto* GM=GetWorld()->GetAuthGameMode<ATallowGameMode>())
 {
  if(GM->IsCaptureInProgress()){bOpen=false;return;}
  GM->OnCaught.AddUniqueDynamic(this,&UTallowJournalWidget::SkipReading);
 }
 if(!IsInViewport())AddToViewport(40);
 TakeReadingInput();
 if(auto* Noise=UTallowNoiseSubsystem::Get(this))Noise->OnNoiseEmitted.AddUniqueDynamic(this,&UTallowJournalWidget::OnImportantNoise);
 PlayNarration();
 if(auto* Director=UGameplayStatics::GetActorOfClass(this,ATallowPlayDirector::StaticClass()))AudioFocus=Director->FindComponentByClass<UTallowSoundscapeComponent>();
}

void UTallowJournalWidget::TakeReadingInput()
{
 auto* PC=Controller.Get();if(!PC||bHoldingInput)return;
 bHoldingInput=true;bHadCursor=PC->bShowMouseCursor;PC->bShowMouseCursor=true;SetIsFocusable(true);
 FInputModeUIOnly Mode;Mode.SetWidgetToFocus(TakeWidget());PC->SetInputMode(Mode);
 PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);
}

void UTallowJournalWidget::ReleaseReadingInput()
{
 if(!bHoldingInput)return;bHoldingInput=false;
 if(auto* PC=Controller.Get())
 {
  PC->SetInputMode(FInputModeGameOnly());PC->bShowMouseCursor=bHadCursor;
  PC->SetIgnoreMoveInput(false);PC->SetIgnoreLookInput(false);
 }
}

void UTallowJournalWidget::Close()
{
 if(!bOpen)return;
 if(HasReading()&&(!Playback.bFinished||Pages.IsValidIndex(Page+1))&&(!bNarrationMuted||Playback.Position>0.f)){MinimizeReading();return;}
 SkipReading();
}

void UTallowJournalWidget::MinimizeReading()
{
 if(!bOpen)return;
 bCompact=true;ReleaseReadingInput();SetIsFocusable(false);InvalidateLayoutAndVolatility();
 if(Playback.bFinished)ReadingFinished();
}

void UTallowJournalWidget::SkipReading()
{
 if(!bOpen)return;bOpen=false;bCompact=false;
 StopNarration();ReleaseReadingInput();
 if(auto* Noise=UTallowNoiseSubsystem::Get(this))Noise->OnNoiseEmitted.RemoveDynamic(this,&UTallowJournalWidget::OnImportantNoise);
 if(GetWorld())if(auto* GM=GetWorld()->GetAuthGameMode<ATallowGameMode>())GM->OnCaught.RemoveDynamic(this,&UTallowJournalWidget::SkipReading);
 RemoveFromParent();
}

void UTallowJournalWidget::ConfigureDocument(const TArray<FText>& InPages, bool bInPrinted, FName InDocumentKey)
{ StopNarration();Pages=InPages;Page=0;bPrinted=bInPrinted;DocumentKey=InDocumentKey.IsNone()?TallowJournalVoices::Identify(Pages):InDocumentKey;SpeakerKey=TallowJournalVoices::Speaker(DocumentKey); }

void UTallowJournalWidget::Turn(int32 Delta)
{
	if (Page + Delta >= Pages.Num()) { Close(); return; }
	const int32 NextPage = FMath::Clamp(Page + Delta, 0, Pages.Num() - 1);
	if (NextPage != Page) { Page = NextPage; PlayNarration(); }
}

static TAutoConsoleVariable<int32> CVarCorruptSimClock(TEXT("tallow.CorruptSimClock"),0,TEXT("Advance reading playback by frame time even without an audio device (headless checks)."));

void UTallowJournalWidget::StopNarration()
{
	// A recording that ran out still owes its "after" lines; anything else cancels them.
	if(CorruptSession){if(auto* Corrupt=UTallowCorruptedVoiceSubsystem::Get(this))Corrupt->Release(CorruptSession,Playback.bFinished);CorruptSession=0;}
	if (Narration) { Narration->OnAudioFinished.RemoveDynamic(this,&UTallowJournalWidget::ReadingFinished);Narration->OnAudioPlaybackPercent.RemoveDynamic(this,&UTallowJournalWidget::ReadingPercent);Narration->Stop(); Narration->DestroyComponent(); Narration = nullptr; }
	bHaveAudioPercent=false;
	Playback=FTallowReadingPlayback();
	bNarrationDucked = false;
}

void UTallowJournalWidget::PlayNarration()
{
	const double HoldUntil=Playback.HoldUntil;
	StopNarration();
	if (!bOpen || DocumentKey.IsNone() || !Pages.IsValidIndex(Page)) return;
	SpeakerKey = TallowJournalVoices::Speaker(DocumentKey, Page);
	const FString Name = FString::Printf(TEXT("VO_Read_%s_%02d"), *DocumentKey.ToString(), Page + 1);
	const FString Package = TEXT("/Game/Audio/VO/") + Name;
	const FString ObjectPath = Package + TEXT(".") + Name;
	USoundBase* Sound = FindObject<USoundBase>(nullptr, *ObjectPath);
	// Unrecorded pages remain readable without a missing-asset warning every frame.
	if (!Sound && FPackageName::DoesPackageExist(Package)) Sound = LoadObject<USoundBase>(nullptr, *ObjectPath);
	Playback.Prepare(Sound!=nullptr,Sound?Sound->GetDuration():0.f,!bNarrationMuted);
	Playback.HoldUntil=HoldUntil;Playback.bPriorityPaused=FPlatformTime::Seconds()<HoldUntil;
	if (!Sound) return;
	if(auto* Corrupt=UTallowCorruptedVoiceSubsystem::Get(this))CorruptSession=Corrupt->Begin(*FString::Printf(TEXT("read:%s:%d"),*DocumentKey.ToString(),Page+1),Playback.Duration);
	Narration = UGameplayStatics::CreateSound2D(this, Sound, .72f, 1.f, 0.f, nullptr, false, false);
	if(Narration){Narration->bIsUISound=false;Narration->OnAudioFinished.AddUniqueDynamic(this,&UTallowJournalWidget::ReadingFinished);Narration->OnAudioPlaybackPercent.AddUniqueDynamic(this,&UTallowJournalWidget::ReadingPercent);Narration->Play();Narration->SetPaused(true);}
}

void UTallowJournalWidget::SetNarrationMuted(bool bMuted)
{
	bNarrationMuted = bMuted;
	Playback.bUserPaused=bMuted;
	if (auto* Story = UTallowStorySubsystem::GetStory(this)) Story->SetDocumentNarrationMuted(bMuted);
	if (!Narration) return;
	Narration->SetPaused(!Playback.IsRunning());
}

void UTallowJournalWidget::ReplayNarration()
{
	if (!bOpen) return;
	bNarrationMuted=false;if(auto* Story=UTallowStorySubsystem::GetStory(this))Story->SetDocumentNarrationMuted(false);
	PlayNarration();
}

void UTallowJournalWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (!bOpen) return;
	const auto* PC = Controller.Get();
	const auto* Player = PC ? Cast<ATallowCharacter>(PC->GetPawn()) : nullptr;
	const auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ATallowGameMode>() : nullptr;
	// Being spotted or caught puts the paper away immediately, returning movement
	// and the audio foreground to the encounter. It never resumes over a capture.
	if ((GM && GM->IsCaptureInProgress()) || (Player && Player->SeenCallAt > SeenAtOpen + .01f)) { SkipReading(); return; }
	bool bPriority=Player&&Player->KeeperSight>.08f;
	if(!Keeper.IsValid())Keeper=Cast<ATallowCreature>(UGameplayStatics::GetActorOfClass(this,ATallowCreature::StaticClass()));
	if(Keeper.IsValid()&&Player){const float Distance=FVector::Dist(Keeper->GetActorLocation(),Player->GetActorLocation());bPriority|=Distance<1800.f&&Keeper->CurrentState==ETallowCreatureState::Chasing;if(const auto* Voice=Keeper->FindComponentByClass<UTallowKeeperVoiceComponent>())bPriority|=Distance<2000.f&&Voice->IsSpeaking();}
	if(Keeper.IsValid()&&Player)
	{
		const auto State=Keeper->CurrentState;
		const bool bSearching=State==ETallowCreatureState::SearchingCorner||State==ETallowCreatureState::SearchHigh||State==ETallowCreatureState::SearchLow;
		bPriority|=bSearching&&FVector::Dist(Keeper->GetActorLocation(),Player->GetActorLocation())<1400.f;
	}
	if(AudioFocus.IsValid())
	{
		const auto State=AudioFocus->GetTension();
		bPriority|=State==ETallowTension::Hunted||State==ETallowTension::Chase||State==ETallowTension::Caught;
	}
	if(bPriority)PauseReadingForImportantSound(1.25f);
	if(CorruptSession)
	{
		if(auto* Corrupt=UTallowCorruptedVoiceSubsystem::Get(this))
		{
			const bool bWorldPaused=UGameplayStatics::IsGamePaused(this);
			const auto Clock=Playback.bFinished?ETallowCorruptClock::Finished:(Playback.bUserPaused||bWorldPaused)?ETallowCorruptClock::Paused:Playback.bPriorityPaused?ETallowCorruptClock::Held:ETallowCorruptClock::Running;
			Corrupt->Advance(CorruptSession,DeltaTime,Playback.Position,Clock);
			// A break holds the original for its whole chain; the hold is renewed every frame so a pause cannot make them overlap.
			const float Hold=Corrupt->HoldRemaining(CorruptSession);
			if(Hold>0.f&&!bWorldPaused&&!Playback.bUserPaused)PauseReadingForImportantSound(Hold);
		}
	}
	Playback.Advance((CVarCorruptSimClock.GetValueOnGameThread()&&Playback.bAvailable)||(!bHaveAudioPercent&&Narration&&Narration->IsPlaying())?DeltaTime:0.f,FPlatformTime::Seconds(),UGameplayStatics::IsGamePaused(this));
	if(bCompact&&Playback.bFinished){ReadingFinished();return;}
	if(Narration)Narration->SetPaused(!Playback.IsRunning());
}

void UTallowJournalWidget::PauseReadingForImportantSound(float Seconds)
{Playback.Interrupt(FPlatformTime::Seconds(),Seconds);if(Narration)Narration->SetPaused(true);}
void UTallowJournalWidget::OnImportantNoise(const FTallowNoiseEvent& Noise)
{
 const APawn* Listener=Controller.IsValid()?Controller->GetPawn():nullptr;auto* Bus=UTallowNoiseSubsystem::Get(this);if(!Listener||!Bus)return;
 const bool Step=Noise.Type==ETallowNoiseType::Footstep||Noise.Type==ETallowNoiseType::CrouchStep||Noise.Type==ETallowNoiseType::RunStep;
 if(Step&&Noise.Instigator==Listener)return;
 if(Bus->GetLoudnessAt(Noise,Listener->GetActorLocation())<.025f)return;
 PauseReadingForImportantSound(Step?1.3f:2.5f);
}
void UTallowJournalWidget::GoToPage(int32 InPage)
{
	if(!bOpen||!Pages.IsValidIndex(InPage)||InPage==Page)return;
	Page=InPage;PlayNarration();
}
void UTallowJournalWidget::ToggleReading(){if(!HasReading())return;if(Playback.bFinished){ReplayNarration();return;}SetNarrationMuted(!bNarrationMuted);}
void UTallowJournalWidget::ReplayReading(){if(HasReading())ReplayNarration();}
void UTallowJournalWidget::ReadingFinished()
{
 Playback.bFinished=true;Playback.Position=Playback.Duration;
 if(bCompact){if(Pages.IsValidIndex(Page+1)){++Page;PlayNarration();}else SkipReading();}
}

void UTallowJournalWidget::ReadingPercent(const USoundWave* Wave,float Percent)
{
	if(!Narration||Narration->Sound!=Wave||!Playback.IsRunning())return;
	// Mixer sources without a ready frame count can initially report 100%.
	// A replay must see a real cursor before an end percentage can finish it.
	if(!FMath::IsFinite(Percent)||(!bHaveAudioPercent&&(Percent<=.0001f||Percent>=.99f)))return;
	bHaveAudioPercent=true;
	Playback.Position=FMath::Clamp(Percent,0.f,1.f)*Playback.Duration;
}
FText UTallowJournalWidget::ReadingStatus() const
{
 if(!HasReading())return FText::GetEmpty();
 if(Playback.bPriorityPaused)return LOCTEXT("ReadingHold","listen...");
 if(Playback.bUserPaused)return LOCTEXT("ReadingPaused","paused");
 if(Playback.bFinished)return LOCTEXT("ReadingEnd","the end of this page");
 return LOCTEXT("ReadingNow","playing");
}

FText UTallowJournalWidget::ReadingAction() const
{
 const bool bPad=UTallowInputDeviceSubsystem::GetCurrent(this)==ETallowInputDevice::Gamepad;
 if(bPad)return Playback.bFinished?LOCTEXT("PadReadAgain","[ d-pad left ]  again"):Playback.bUserPaused?LOCTEXT("PadReadPlay","[ d-pad left ]  play"):LOCTEXT("PadReadPause","[ d-pad left ]  pause");
 if(Playback.bFinished)return LOCTEXT("ReadAgain","[ V ]  again");
 // A danger hold is independent of the player's pause. The key continues
 // to offer a manual pause while the soundscape has temporarily held it.
 return Playback.bUserPaused?LOCTEXT("ReadPlay","[ V ]  play"):LOCTEXT("ReadPause","[ V ]  pause");
}

FText UTallowJournalWidget::ReadingTime() const
{
 if(!HasReading())return FText::GetEmpty();
 const int32 P=int32(Playback.Position),D=int32(Playback.Duration);
 return FText::FromString(FString::Printf(TEXT("%02d:%02d / %02d:%02d"),P/60,P%60,D/60,D%60));
}

FName UTallowJournalWidget::PageIllustration() const
{
 if(DocumentKey==TEXT("ruth"))return TEXT("PageSewing");
 if(DocumentKey==TEXT("nan"))return TEXT("PageWarning");
 if(DocumentKey==TEXT("register")||DocumentKey==TEXT("labels")||DocumentKey==TEXT("chart"))return TEXT("PageRecords");
 if(DocumentKey==TEXT("notebook")||DocumentKey==TEXT("letter")||DocumentKey==TEXT("bill")||DocumentKey==TEXT("newspaper")||DocumentKey==TEXT("portrait"))return TEXT("PageEvidence");
 return TEXT("JournalDrawing");
}

#if WITH_EDITOR
void UTallowJournalWidget::ConfigureReadingReview(FName Document,bool bMinimized,bool bPaused,bool bHeld,FName SpeakerOverride)
{
 ConfigureDocument(TallowNarrative::Pages(Document),TallowNarrative::Printed(Document),Document);
 bCompact=bMinimized;
 if(!SpeakerOverride.IsNone())SpeakerKey=SpeakerOverride;
 Playback.Prepare(true,94.f,!bPaused);Playback.Position=37.f;Playback.bPriorityPaused=bHeld;
 // bOpen stays false, so no ticking, input changes or sound creation occurs.
}
#endif

void UTallowJournalWidget::NativeDestruct()
{
	SkipReading(); StopNarration(); Super::NativeDestruct();
}

FReply UTallowJournalWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	const ETallowPadIntent Pad = TallowUIInput::ClassifyGamepad(Key);
	if (Key == EKeys::Escape || Key == EKeys::Tab || Pad == ETallowPadIntent::Back || Pad == ETallowPadIntent::Menu) Close();
	else if (Key == EKeys::V || Pad == ETallowPadIntent::Secondary) ToggleReading();
	else if (Key == EKeys::R || Pad == ETallowPadIntent::Tertiary) ReplayReading();
	else if (Key == EKeys::BackSpace) SkipReading();
	else if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Q || Pad == ETallowPadIntent::Left || Pad == ETallowPadIntent::PrevTab) Turn(-1);
	else if (Key == EKeys::E || Key == EKeys::SpaceBar || Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Enter || Pad == ETallowPadIntent::Right || Pad == ETallowPadIntent::NextTab || Pad == ETallowPadIntent::Confirm) Turn(1);
	return FReply::Handled();
}

FReply UTallowJournalWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::RightMouseButton) { Turn(-1); return FReply::Handled(); }
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

FText UTallowJournalWidget::GetPageText() const
{
	return Pages.IsValidIndex(Page) ? Pages[Page] : FText::GetEmpty();
}

FText UTallowJournalWidget::GetFooterText() const
{
	return FText::Format(LOCTEXT("PageOf", "{0} of {1}"), Page + 1, Pages.Num());
}

FString UTallowJournalWidget::KeyHint(int32 Which) const
{
	// One place for every key the reader names, keyboard or pad, set in key caps by TallowPaper::DrawKeys.
	const bool bPad = UTallowInputDeviceSubsystem::GetCurrent(this) == ETallowInputDevice::Gamepad;
	const bool bLast = Page + 1 >= Pages.Num();
	switch (Which)
	{
	case 0: return Page > 0 ? (bPad ? TEXT("[ L1 ]  back a page") : TEXT("[ A ]  back a page")) : FString();
	case 1: return bPad ? TEXT("[ circle ]  put it away") : TEXT("[ esc ]  put it away");
	case 2: return bLast ? (bPad ? TEXT("[ R1 ]  done") : TEXT("[ E ]  done")) : (bPad ? TEXT("[ R1 ]  next page") : TEXT("[ E ]  next page"));
	case 3: return ReadingAction().ToString();
	case 4: return bPad ? TEXT("[ d-pad right ]  stop the tape") : TEXT("[ backspace ]  stop the tape");
	default: return FString();
	}
}

TallowPaper::EMark UTallowJournalWidget::SpeakerMark() const
{
	// A recording is filed under the reel; a paper with no voice under its own kind.
	return HasReading() ? TallowPaper::EMark::Reel : TallowPaper::PaperMark(DocumentKey);
}

TSharedRef<SWidget> UTallowJournalWidget::RebuildWidget()
{
	using namespace TallowPaper;
	auto Available = [this]() { return HasReading() ? EVisibility::Visible : EVisibility::Collapsed; };
	auto Fraction = [this]() { return Playback.Duration > 0 ? Playback.Position / Playback.Duration : 0.f; };
	auto Rule = [](FLinearColor Colour)
	{ return SNew(SBox).HeightOverride(1)[SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Colour)]; };
	auto KeyButton = [this](int32 Which, TFunction<void()> Act)
	{
		return SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).ContentPadding(FMargin(0, 6))
			.OnClicked_Lambda([Act]() { Act(); return FReply::Handled(); })
			[SNew(STallowKeys).Text_Lambda([this, Which]() { return KeyHint(Which); }).Font(FellItalic(Pt::Small)).Colour(Ink).KeyColour(Ivory)];
	};
	auto StatusColour = [this]() { return Playback.bPriorityPaused ? Blood : Pencil; };
	// A child's own pages are shown in her hand; everything else is set in the book face.
	auto PageFont = [this]() { return DocumentKey == TEXT("notes") ? Child(Pt::Lead + 6) : Fell(Pt::Lead); };

	const TSharedRef<SWidget> Paper = SNew(SOverlay)
	+ SOverlay::Slot()[SNew(SBorder).BorderImage(Ground()).BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, .95f))]
	+ SOverlay::Slot().Padding(48).HAlign(HAlign_Center).VAlign(VAlign_Center)
	[SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
	 [SNew(SBox).WidthOverride(1560).HeightOverride(860)
	  [SNew(SVerticalBox)
	   +SVerticalBox::Slot().FillHeight(1)
	   [SNew(SHorizontalBox)
	    // What it is and whose voice, then the tape running as the red thread.
	    +SHorizontalBox::Slot().FillWidth(.32f).Padding(0, 18, 64, 10)
	    [SNew(SVerticalBox)
	     +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this]() { return DocumentKey == TEXT("notes") ? LOCTEXT("OwnHand", "IN MY OWN HAND") : HasReading() ? LOCTEXT("Recording", "A RECORDING FROM THE HOUSE") : LOCTEXT("Evidence", "A PAPER FROM THE HOUSE"); }).Font(Caps(Pt::Label)).ColorAndOpacity(Pencil)]
	     +SVerticalBox::Slot().AutoHeight().Padding(0, 20, 0, 28)[SNew(STextBlock).Text_Lambda([this]() { return DocumentKey.IsNone() ? LOCTEXT("UntitledPaper", "Pages left behind") : TallowNarrative::Title(DocumentKey); }).Font(Fell(Pt::Heading)).ColorAndOpacity(Ivory).AutoWrapText(true).LineHeightPercentage(.95f)]
	     +SVerticalBox::Slot().AutoHeight()
	     [SNew(SHorizontalBox)
	      +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 14, 0)
	      [SNew(SBox).WidthOverride(64).HeightOverride(64)[SNew(STallowMark).Size(62).Colour(Ink).Mark_Lambda([this]() { return SpeakerMark(); })]]
	      +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([this]() { return TallowJournalVoices::Name(SpeakerKey); }).Font(FellItalic(Pt::Body + 2)).ColorAndOpacity(Ink).AutoWrapText(true)]]
	     +SVerticalBox::Slot().FillHeight(1)[SNew(SBox)]
	     +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox).Visibility_Lambda(Available)
	      +SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text_Lambda([this]() { return ReadingStatus(); }).Font(FellItalic(Pt::Small)).ColorAndOpacity_Lambda(StatusColour)]
	      +SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text_Lambda([this]() { return ReadingTime(); }).Font(Fell(Pt::Small - 2)).ColorAndOpacity(Pencil)]]
	     +SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 14)[SNew(SBox).HeightOverride(20).Visibility_Lambda(Available)[SNew(SReadingLine).OnDark(true).Available_Lambda([this]() { return HasReading(); }).Fraction_Lambda(Fraction)]]
	     +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda(Available)[KeyButton(3, [this]() { ToggleReading(); })]]
	     +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda(Available)[KeyButton(4, [this]() { SkipReading(); })]]
	     +SVerticalBox::Slot().AutoHeight().Padding(0, 26, 0, 0)[SNew(STextBlock).Visibility_Lambda(Available).Text(LOCTEXT("VoiceContinue", "Put it away and the tape keeps playing.\nThe house will not wait.")).Font(FellItalic(Pt::Small)).LineHeightPercentage(1.12f).ColorAndOpacity(Pencil)]
	    ]
	    // The words themselves, on the lighter sheet.
	    +SHorizontalBox::Slot().FillWidth(.68f)
	    [SNew(SBorder).BorderImage(Ground(true)).BorderBackgroundColor(FLinearColor::White).Padding(FMargin(72, 52, 64, 40))
	     [SNew(SVerticalBox)
	      +SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)[SNew(SHorizontalBox)
	       +SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text_Lambda([this]() { return DocumentKey == TEXT("notes") ? LOCTEXT("Notebook", "my notebook") : HasReading() ? LOCTEXT("Transcript", "what the tape says") : bPrinted ? LOCTEXT("Printed", "set in type") : LOCTEXT("Transcription", "written by hand"); }).Font(FellItalic(Pt::Small)).ColorAndOpacity(Pencil)]
	       +SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text_Lambda([this]() { return GetFooterText(); }).Font(FellItalic(Pt::Small)).ColorAndOpacity(Pencil)]]
	      +SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 34)[Rule(Pencil.CopyWithNewOpacity(.22f))]
	      +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox).Style(ScrollArea()).ScrollBarStyle(ScrollBar()).ScrollBarThickness(FVector2D(3, 3)).AllowOverscroll(EAllowOverscroll::No)
	       +SScrollBox::Slot()[SNew(STextBlock).Text_Lambda([this]() { return GetPageText(); }).Font_Lambda(PageFont).ColorAndOpacity(Ink).AutoWrapText(true).LineHeightPercentage(1.24f)]]
	     ]]
	   ]
	   // Turning, and putting it away.
	   +SVerticalBox::Slot().AutoHeight().Padding(0, 26, 0, 0)[SNew(SHorizontalBox)
	    +SHorizontalBox::Slot().FillWidth(.32f)[KeyButton(0, [this]() { Turn(-1); })]
	    +SHorizontalBox::Slot().FillWidth(.36f).HAlign(HAlign_Center)[KeyButton(1, [this]() { Close(); })]
	    +SHorizontalBox::Slot().FillWidth(.32f).HAlign(HAlign_Right)[KeyButton(2, [this]() { Turn(1); })]]
	  ]
	 ]
	];

	// Listening while she moves: no card, only soot under the words, low on the left.
	const TSharedRef<SWidget> Strip = SNew(SBox).WidthOverride(500)
	[SNew(SOverlay)
	 +SOverlay::Slot()[SNew(STallowSmudge).Alpha(.94f)]
	 +SOverlay::Slot().Padding(FMargin(8, 14, 8, 10))
	 [SNew(SVerticalBox)
	  +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
	   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 14, 0)
	   [SNew(SBox).WidthOverride(52).HeightOverride(52)[SNew(STallowMark).Size(50).Colour(Ink).Mark_Lambda([this]() { return SpeakerMark(); })]]
	   +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
	   [SNew(SVerticalBox)
	    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this]() { return TallowJournalVoices::Name(SpeakerKey); }).Font(Fell(Pt::Body + 2)).ColorAndOpacity(Ivory)]
	    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this]() { return TallowNarrative::Title(DocumentKey); }).Font(FellItalic(Pt::Small)).ColorAndOpacity(Pencil)]]
	   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(12, 4, 0, 0)[SNew(STextBlock).Text_Lambda([this]() { return ReadingTime(); }).Font(Fell(Pt::Small - 3)).ColorAndOpacity(Pencil)]]
	  +SVerticalBox::Slot().AutoHeight().Padding(66, 6, 0, 4)[SNew(SBox).HeightOverride(20)[SNew(SReadingLine).OnDark(true).Available_Lambda([this]() { return HasReading(); }).Fraction_Lambda(Fraction)]]
	  +SVerticalBox::Slot().AutoHeight().Padding(66, 0, 0, 0)[SNew(SHorizontalBox)
	   +SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 22, 0)[SNew(STallowKeys).Text_Lambda([this]() { return KeyHint(3); }).Font(FellItalic(Pt::Small - 2)).Colour(Ink).KeyColour(Ivory)]
	   +SHorizontalBox::Slot().AutoWidth()[SNew(STallowKeys).Text_Lambda([this]() { return KeyHint(4); }).Font(FellItalic(Pt::Small - 2)).Colour(Pencil).KeyColour(Ink)]
	   +SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Right).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([this]() { return ReadingStatus(); }).Font(FellItalic(Pt::Small - 2)).ColorAndOpacity_Lambda(StatusColour)]]
	 ]
	];

	return SNew(SOverlay)
	+SOverlay::Slot()[SNew(SBox).Visibility_Lambda([this]() { return bCompact ? EVisibility::Collapsed : EVisibility::Visible; })[Paper]]
	+SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(56, 0, 0, 46)
	[SNew(SBox).Visibility_Lambda([this]() { return bCompact ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })[Strip]];
}

#undef LOCTEXT_NAMESPACE
