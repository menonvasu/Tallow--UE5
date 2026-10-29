#include "UI/Presentation/TallowSlidesWidget.h"
#include "Input/TallowInputPrompts.h"

#include "Components/AudioComponent.h"
#include "Engine/Texture2D.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Audio/TallowCorruptedVoice.h"
#include "Styling/CoreStyle.h"
#include "UI/Presentation/TallowPaper.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "UI/Presentation/STallowMark.h"

namespace
{
	constexpr float FadeSeconds = .7f;

	/** Where the words sit on a plate and what they say (Tools/Presentation/UIRedo/story_data.py, in sentence case).
	 *  Side: 'L' or 'R' is the dark third the plate leaves for them; 'T' is a title plate (the wordmark, centred). */
	struct FPlateCaption { const TCHAR* Name; TCHAR Side; const TCHAR* Headline; const TCHAR* Line; };
	const FPlateCaption Captions[] = {
		{TEXT("Intro_01"), 'L', TEXT("London. Winter 1974."), TEXT("The children always disappear one at a time.")},
		{TEXT("Intro_02"), 'R', TEXT("Then they come home."), TEXT("Clean. Fed. Polite. They cannot remember where they have been.")},
		{TEXT("Intro_03"), 'L', TEXT("Every child draws the same thing."), TEXT("Ten arms. Two great eyes. An impossible smile. They call it Mim.")},
		{TEXT("Intro_04"), 'R', TEXT("1894. 1932. 1958. 1973."), TEXT("Different children. The same house. The same careful handwriting.")},
		{TEXT("Intro_05"), 'L', TEXT("Maisie remembered."), TEXT("A staircase. A pale face. A great many hands. Nell believed her.")},
		{TEXT("Intro_06"), 'R', TEXT("Nobody else did."), TEXT("Maisie kept a journal. Then she disappeared for the second time.")},
		{TEXT("Intro_07"), 'L', TEXT("The second time, I did not want to run."), TEXT("Nell went to the house to find the girl she remembered.")},
		{TEXT("Intro_08"), 'L', TEXT("She brought a candle."), TEXT("The entrance closed behind her. Somewhere in the house, something moved.")},
		{TEXT("Intro_09"), 'L', TEXT("Somewhere safe."), TEXT("That was what Mim called it. Nell woke in the chair.")},
		{TEXT("Intro_Title"), 'T', TEXT(""), TEXT("")},
		{TEXT("EndA_01"), 'L', TEXT("She reached the morning."), TEXT("Exhausted. Unharmed. Her clothes were clean.")},
		{TEXT("EndA_02"), 'R', TEXT("Another child found safe."), TEXT("She struggled to remember. Eventually, the questions stopped.")},
		{TEXT("EndA_03"), 'L', TEXT("First a circle. Then two eyes."), TEXT("Then a smile. Then ten arms.")},
		{TEXT("EndA_04"), 'T', TEXT(""), TEXT("Another child found safe.")},
		{TEXT("EndB_01"), 'L', TEXT("Maisie did not want to leave."), TEXT("She said Mim was kind. It never shouted. It never left her alone.")},
		{TEXT("EndB_02"), 'L', TEXT("She called for Mim."), TEXT("Nell hid. Maisie was perfectly calm as the arms gathered her up.")},
		{TEXT("EndB_03"), 'R', TEXT("Maisie Evans. Finished."), TEXT("For a moment, recognition. Perhaps fear. Then she smiled again.")},
		{TEXT("EndB_04"), 'R', TEXT("Eleanor Price. Place prepared."), TEXT("Mim carried Maisie away. Nell remained in the dark.")},
		{TEXT("Confess_01"), 'L', TEXT("I was there. And I did nothing."), TEXT("An unsent letter from the constable who had been warned.")},
		{TEXT("Confess_02"), 'L', TEXT("I told her to be good."), TEXT("The house had been waiting long before Nell was born.")}};

	const FPlateCaption* CaptionFor(FName Name)
	{
		for (const FPlateCaption& Caption : Captions) if (Name == Caption.Name) return &Caption;
		return nullptr;
	}
}

UTexture2D* UTallowSlidesWidget::LoadPlate(FName Name)
{
	const FString Key = Name.ToString();
	if (UTexture2D* Plate = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/FinalPass/UI/Slides/T_%s.T_%s"), *Key, *Key), nullptr, LOAD_NoWarn))
		return Plate;
	return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Presentation/Lore/Slides/T_%s.T_%s"), *Key, *Key), nullptr, LOAD_NoWarn);
}

bool UTallowSlidesWidget::HasNativeCaption() const
{
	// The older stills carry their words in the picture; only the final pass's plates leave them to us.
	return Slides.IsValidIndex(Current) && Slides[Current].Image && CaptionFor(Slides[Current].Name)
		&& Slides[Current].Image->GetPathName().StartsWith(TEXT("/Game/FinalPass/UI/Slides/"));
}

float UTallowSlidesWidget::CaptionAlpha(float Delay) const
{
	if (!Slides.IsValidIndex(Current) || !HasNativeCaption()) return 0.f;
	const float Hold = Slides[Current].Seconds;
	const float In = FMath::Clamp((Elapsed - FadeSeconds * .6f - Delay) / .9f, 0.f, 1.f);
	const float Out = FMath::Clamp((Hold + FadeSeconds - Elapsed) / FadeSeconds, 0.f, 1.f);
	return FMath::Min(In * In * (3.f - 2.f * In), Out);
}

UTallowSlidesWidget::UTallowSlidesWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetIsFocusable(false);
}

void UTallowSlidesWidget::Play(const TArray<FTallowSlide>& InSlides, TFunction<void()> InOnDone)
{
	Slides = InSlides;
	OnDone = MoveTemp(InOnDone);
	if (Slides.IsEmpty()) { if (OnDone) OnDone(); return; }
	bPlaying = true;
	if (!IsInViewport()) AddToViewport(60);
	// A low drone under the stills, so the voice never sits in silence.
	if (USoundBase* Drone = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Tension/T_Drone_Dark_Loop.T_Drone_Dark_Loop")))
		Bed = UGameplayStatics::SpawnSound2D(this, Drone, .17f, 1.f, 0.f, nullptr, false, true);   // 17 dB under the voice
	ShowSlide(0);
}

void UTallowSlidesWidget::ShowSlide(int32 Index)
{
	EndCorrupt(true);
	bDucked = false;
	Current = Index; Elapsed = 0.f; bVoiceStarted = false;
	if (Voice) { Voice->FadeOut(.025f,0.f); Voice = nullptr; }
	Brush = FSlateBrush();
	if (Slides.IsValidIndex(Index) && Slides[Index].Image)
	{
		Brush.SetResourceObject(Slides[Index].Image);
		Brush.ImageSize = FVector2D(Slides[Index].Image->GetSizeX(), Slides[Index].Image->GetSizeY());
	}
}

void UTallowSlidesWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (!bPlaying || !Slides.IsValidIndex(Current)) return;
	Elapsed += DeltaTime;
	const FTallowSlide& Slide = Slides[Current];
	if (!bVoiceStarted && Elapsed >= FadeSeconds * .5f)
	{
		bVoiceStarted = true;
		if (Slide.Voice) Voice = UGameplayStatics::SpawnSound2D(this, Slide.Voice, .85f, 1.f, 0.f, nullptr, false, true);
		if (Slide.Voice && !Slide.CorruptKey.IsNone())
			if (auto* Corrupt = UTallowCorruptedVoiceSubsystem::Get(this)) CorruptSession = Corrupt->Begin(Slide.CorruptKey, Slide.Voice->GetDuration());
	}
	if (CorruptSession)
		if (auto* Corrupt = UTallowCorruptedVoiceSubsystem::Get(this))
		{
			const bool bPaused = UGameplayStatics::IsGamePaused(this);
			Corrupt->Advance(CorruptSession, DeltaTime, FMath::Max(0.f, Elapsed - FadeSeconds * .5f), bPaused ? ETallowCorruptClock::Paused : ETallowCorruptClock::Running);
			// A break ducks the slide's voice by 15 dB while it speaks; slides keep their own timing.
			const bool bDuck = Corrupt->HoldRemaining(CorruptSession) > 0.f;
			if (Voice && bDuck != bDucked) { bDucked = bDuck; Voice->AdjustVolume(.1f, bDuck ? .178f : 1.f); }
		}
	if (Elapsed >= Slide.Seconds + FadeSeconds)
	{
		if (Current + 1 < Slides.Num()) ShowSlide(Current + 1);
		else Finish();
	}
}

void UTallowSlidesWidget::EndCorrupt(bool bFinished)
{
	if (!CorruptSession) return;
	if (auto* Corrupt = UTallowCorruptedVoiceSubsystem::Get(this)) Corrupt->Release(CorruptSession, bFinished);
	CorruptSession = 0;
}

void UTallowSlidesWidget::Finish()
{
	if (!bPlaying) return;
	// Skipping cancels the hidden voice; running out of slides lets lines owed after the last one finish.
	EndCorrupt(Slides.IsValidIndex(Current) && Current + 1 >= Slides.Num() && Elapsed >= Slides[Current].Seconds + FadeSeconds);
	bPlaying = false;
	if (Voice) Voice->FadeOut(.025f,0.f);
	if (Bed) Bed->FadeOut(1.5f, 0.f);
	auto Done = MoveTemp(OnDone); OnDone = nullptr;
	RemoveFromParent();
	if (Done) Done();
}

void UTallowSlidesWidget::NativeDestruct()
{
	EndCorrupt(false);
	if (Voice) Voice->FadeOut(.025f,0.f);
	if (Bed) Bed->FadeOut(.2f,0.f);
	bPlaying=false; OnDone=nullptr;
	Super::NativeDestruct();
}

const FSlateBrush* UTallowSlidesWidget::GetBrush() const
{
	return &Brush;
}

#if WITH_EDITOR
void UTallowSlidesWidget::ConfigureReview(UTexture2D* Image)
{
	FTallowSlide Slide; Slide.Image=Image; Slide.Seconds=30.f;
	Slides={Slide}; ShowSlide(0); Elapsed=1.f;
}
void UTallowSlidesWidget::ConfigureReview(FName Name)
{
	FTallowSlide Slide; Slide.Name=Name; Slide.Image=LoadPlate(Name); Slide.Seconds=30.f;
	// Far enough in that the words have settled.
	Slides={Slide}; ShowSlide(0); Elapsed=4.f;
}
#endif

FLinearColor UTallowSlidesWidget::GetTint() const
{
	if (!Slides.IsValidIndex(Current)) return FLinearColor::Transparent;
	const float Hold = Slides[Current].Seconds;
	const float In = FMath::Clamp(Elapsed / FadeSeconds, 0.f, 1.f);
	const float Out = FMath::Clamp((Hold + FadeSeconds - Elapsed) / FadeSeconds, 0.f, 1.f);
	return FLinearColor(1.f, 1.f, 1.f, FMath::Min(In, Out));
}

TSharedRef<SWidget> UTallowSlidesWidget::MakeCaption()
{
	using namespace TallowPaper;
	auto Find = [this]() { return Slides.IsValidIndex(Current) ? CaptionFor(Slides[Current].Name) : nullptr; };
	auto Side = [Find]() { const FPlateCaption* C = Find(); return C ? C->Side : TCHAR('L'); };
	auto Headline = [Find]() { const FPlateCaption* C = Find(); return FText::FromString(C ? C->Headline : TEXT("")); };
	auto Line = [Find]() { const FPlateCaption* C = Find(); return FText::FromString(C ? C->Line : TEXT("")); };
	auto Fade = [this](float Delay) { return FSlateColor(FLinearColor(1.f, 1.f, 1.f, CaptionAlpha(Delay))); };
	// A told plate: the headline, one red thread under it, the line in italic. Set in the plate's dark third.
	auto Told = [=, this](TCHAR Which) -> TSharedRef<SWidget>
	{
		return SNew(SBox).WidthOverride(660.f).Visibility_Lambda([Side, Which, this]() { return HasNativeCaption() && Side() == Which ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[SNew(STextBlock).Text_Lambda(Headline).Font(Fell(Pt::Display)).ColorAndOpacity(Ivory).WrapTextAt(660.f).LineHeightPercentage(.96f).ColorAndOpacity_Lambda([Fade]() { return Fade(0.f); })]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(FMargin(2.f, 14.f, 0.f, 20.f))
			[SNew(SBox).WidthOverride(150.f).HeightOverride(8.f)[SNew(STallowThread).Seed(17)]]
			+ SVerticalBox::Slot().AutoHeight()
			[SNew(STextBlock).Text_Lambda(Line).Font(FellItalic(Pt::Lead + 2)).WrapTextAt(640.f).LineHeightPercentage(1.14f).ColorAndOpacity_Lambda([Fade]() { const FSlateColor C = Fade(.55f); return FSlateColor(Ink.CopyWithNewOpacity(C.GetSpecifiedColor().A)); })]
		];
	};
	return SNew(SOverlay)
	+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(150.f, 0.f, 0.f, 130.f))[Told('L')]
	+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 130.f, 130.f))[Told('R')]
	// A title plate: the wordmark in the middle, the thread beneath, and a line if the ending has one.
	+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 60.f))
	[
		SNew(SVerticalBox).Visibility_Lambda([Side, this]() { return HasNativeCaption() && Side() == 'T' ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[SNew(SBox).WidthOverride(900.f).HeightOverride(240.f)[SNew(SImage).Image(&Wordmark).ColorAndOpacity_Lambda([Fade]() { return Fade(0.f); })]]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 18.f, 0.f, 26.f))
		[SNew(SBox).WidthOverride(180.f).HeightOverride(8.f)[SNew(STallowThread).Seed(19)]]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[SNew(STextBlock).Text_Lambda(Line).Font(FellItalic(Pt::Lead + 2)).ColorAndOpacity_Lambda([Fade]() { const FSlateColor C = Fade(.7f); return FSlateColor(Ink.CopyWithNewOpacity(C.GetSpecifiedColor().A)); })]
	];
}

TSharedRef<SWidget> UTallowSlidesWidget::RebuildWidget()
{
	if (UTexture2D* Mark = LoadObject<UTexture2D>(nullptr, TEXT("/Game/FinalPass/UI/T_Wordmark.T_Wordmark"), nullptr, LOAD_NoWarn))
	{
		Wordmark = FSlateBrush();
		Wordmark.SetResourceObject(Mark);
		Wordmark.ImageSize = FVector2D(Mark->GetSizeX(), Mark->GetSizeY());
	}
	return SNew(SOverlay)
	+ SOverlay::Slot()
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(TallowPaper::Soot)
	]
	+ SOverlay::Slot()
	[
		SNew(SScaleBox)
		.Stretch(EStretch::ScaleToFit)
		[
			SNew(SBox).WidthOverride(1920).HeightOverride(1080)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SImage)
					.Image_Lambda([this]() { return GetBrush(); })
					.ColorAndOpacity_Lambda([this]() { return FSlateColor(GetTint()); })
				]
				+ SOverlay::Slot()[MakeCaption()]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 56.f, 40.f))
				[
					SNew(STallowKeys).Text_Lambda([this]() { return FString::Printf(TEXT("[ %s ]  skip"), *TallowInputPrompts::GetCurrent(this, ETallowPromptAction::Skip).Label.ToString()); }).Font(TallowPaper::FellItalic(TallowPaper::Pt::Small)).Colour(TallowPaper::Pencil.CopyWithNewOpacity(.7f)).KeyColour(TallowPaper::Pencil)
				]
			]
		]
	];
}
