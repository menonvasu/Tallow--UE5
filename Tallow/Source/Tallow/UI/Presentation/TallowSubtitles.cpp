#include "UI/Presentation/TallowSubtitles.h"
#include "UI/Presentation/TallowPaper.h"
#include "UI/Presentation/STallowMark.h"
#include "Audio/TallowCorruptedVoice.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const TCHAR* const Section = TEXT("Tallow.Settings");

	const TMap<FName, FString>& Lines()
	{
		static const TMap<FName, FString> All = []
		{
			struct FRow { const TCHAR* Asset; const TCHAR* Words; };
			static const FRow Rows[] = {
#include "UI/Presentation/TallowSubtitleLines.inl"
			};
			TMap<FName, FString> Out;
			for (const FRow& R : Rows) Out.Add(R.Asset, R.Words);
			return Out;
		}();
		return All;
	}

	/** Breaks a run of words in two near its middle: at a comma or colon if one sits close, else at a space. */
	void HalveInto(const FString& Words, TArray<FString>& Out)
	{
		if (Words.Len() <= TallowSubtitles::MaxCaptionCharacters) { Out.Add(Words); return; }
		const int32 Middle = Words.Len() / 2;
		int32 Best = INDEX_NONE, BestScore = MAX_int32;
		for (int32 I = 1; I < Words.Len() - 1; ++I)
		{
			const TCHAR C = Words[I];
			if (C != ' ') continue;
			const TCHAR Before = Words[I - 1];
			const bool bPause = Before == ',' || Before == ';' || Before == ':';
			const int32 Score = FMath::Abs(I - Middle) - (bPause ? Words.Len() / 5 : 0);
			if (Score < BestScore) { BestScore = Score; Best = I; }
		}
		if (Best == INDEX_NONE) { Out.Add(Words); return; }
		HalveInto(Words.Left(Best), Out);
		HalveInto(Words.Mid(Best + 1), Out);
	}
}

bool TallowSubtitles::Enabled()
{
	bool bOn = true;
	if (GConfig) GConfig->GetBool(Section, TEXT("Subtitles"), bOn, GGameUserSettingsIni);
	return bOn;
}

void TallowSubtitles::SetEnabled(bool bEnabled)
{
	if (!GConfig) return;
	GConfig->SetBool(Section, TEXT("Subtitles"), bEnabled, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

const FString* TallowSubtitles::TextFor(FName Asset)
{
	return Lines().Find(Asset);
}

FName TallowSubtitles::ReadingAsset(FName Document, int32 Page)
{
	return FName(*FString::Printf(TEXT("VO_Read_%s_%02d"), *Document.ToString(), Page + 1));
}

TArray<FTallowSubtitleCue> TallowSubtitles::Split(const FString& Text, float Duration)
{
	// Whole sentences first, then packed together while they fit a caption, then any sentence still too long is halved.
	TArray<FString> Sentences;
	FString Run;
	for (int32 I = 0; I < Text.Len(); ++I)
	{
		Run.AppendChar(Text[I]);
		const bool bEnd = Text[I] == '.' || Text[I] == '!' || Text[I] == '?';
		const bool bBreak = bEnd && (I + 1 >= Text.Len() || Text[I + 1] == ' ');
		// "R. P." and the like: a full stop after a single capital is not the end of a sentence.
		const bool bInitial = Text[I] == '.' && Run.Len() >= 2 && FChar::IsUpper(Run[Run.Len() - 2]) && (Run.Len() == 2 || Run[Run.Len() - 3] == ' ');
		if (bBreak && !bInitial) { Sentences.Add(Run.TrimStartAndEnd()); Run.Reset(); }
	}
	if (!Run.TrimStartAndEnd().IsEmpty()) Sentences.Add(Run.TrimStartAndEnd());
	TArray<FString> Captions;
	FString Pack;
	auto Flush = [&] { if (!Pack.IsEmpty()) { HalveInto(Pack, Captions); Pack.Reset(); } };
	for (const FString& S : Sentences)
	{
		if (!Pack.IsEmpty() && Pack.Len() + 1 + S.Len() > MaxCaptionCharacters) Flush();
		Pack += (Pack.IsEmpty() ? TEXT("") : TEXT(" ")) + S;
	}
	Flush();
	TArray<FTallowSubtitleCue> Out;
	float Weight = 0.f;
	for (const FString& C : Captions) Weight += C.Len() + 6.f;
	float At = 0.f;
	for (const FString& C : Captions)
	{
		FTallowSubtitleCue& Cue = Out.AddDefaulted_GetRef();
		Cue.Text = C; Cue.Start = At;
		At += FMath::Max(0.f, Duration) * (C.Len() + 6.f) / FMath::Max(1.f, Weight);
		Cue.End = At;
	}
	if (!Out.IsEmpty()) Out.Last().End = FMath::Max(0.f, Duration);
	return Out;
}

const FTallowSubtitleCue* TallowSubtitles::CueAt(const TArray<FTallowSubtitleCue>& Cues, float Position)
{
	for (const FTallowSubtitleCue& Cue : Cues) if (Position >= Cue.Start && Position < Cue.End) return &Cue;
	return nullptr;
}

UTallowSubtitleSubsystem* UTallowSubtitleSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UTallowSubtitleSubsystem>() : nullptr;
}

int32 UTallowSubtitleSubsystem::Begin(FName Asset, float Duration, ETallowSubtitleVoice Voice, bool bSelfTimed)
{
	const FString* Words = TallowSubtitles::TextFor(Asset);
	if (!Words || Duration <= 0.f) return 0;
	FTrack& Track = Tracks.AddDefaulted_GetRef();
	Track.Id = NextId++; Track.Voice = Voice; Track.Duration = Duration; Track.bSelfTimed = bSelfTimed;
	Track.Cues = TallowSubtitles::Split(*Words, Duration);
	EnsureWidget();
	return Track.Id;
}

void UTallowSubtitleSubsystem::SetPosition(int32 Track, float Position, bool bAudible)
{
	if (FTrack* T = Tracks.FindByPredicate([Track](const FTrack& X) { return X.Id == Track; })) { T->Position = Position; T->bAudible = bAudible; }
}

void UTallowSubtitleSubsystem::End(int32 Track)
{
	Tracks.RemoveAll([Track](const FTrack& X) { return X.Id == Track; });
}

void UTallowSubtitleSubsystem::SetHidden(const FString& Text)
{
	HiddenText = Text;
	if (!Text.IsEmpty()) EnsureWidget();
}

TArray<TPair<ETallowSubtitleVoice, FString>> UTallowSubtitleSubsystem::Visible() const
{
	TArray<TPair<ETallowSubtitleVoice, FString>> Out;
	if (!TallowSubtitles::Enabled()) return Out;
	for (const FTrack& T : Tracks)
	{
		if (!T.bAudible) continue;
		if (const FTallowSubtitleCue* Cue = TallowSubtitles::CueAt(T.Cues, T.Position)) Out.Emplace(T.Voice, Cue->Text);
	}
	// The hidden voice is opt-in on its own (Settings: show hidden voice), and never shown with subtitles off.
	if (!HiddenText.IsEmpty() && TallowCorrupted::ShowHiddenVoice()) Out.Emplace(ETallowSubtitleVoice::Hidden, HiddenText);
	return Out;
}

void UTallowSubtitleSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Advance(DeltaTime);
}

void UTallowSubtitleSubsystem::Advance(float DeltaTime)
{
	for (int32 I = Tracks.Num() - 1; I >= 0; --I)
	{
		FTrack& T = Tracks[I];
		if (!T.bSelfTimed) continue;
		T.Position += DeltaTime;
		if (T.Position >= T.Duration) Tracks.RemoveAtSwap(I);
	}
}

void UTallowSubtitleSubsystem::EnsureWidget()
{
	if (!bLive || Widget.IsValid()) return;
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!Viewport) return;
	using namespace TallowPaper;
	// One line each for the story's voices and the hidden one, on a little soot like the lesson line; never hit-testable.
	auto Line = [this](bool bHiddenLine) -> TSharedRef<SWidget>
	{
		auto Words = [this, bHiddenLine]() -> FString
		{
			FString Joined;
			for (const auto& Entry : Visible())
				if ((Entry.Key == ETallowSubtitleVoice::Hidden) == bHiddenLine) Joined += (Joined.IsEmpty() ? TEXT("") : TEXT("\n")) + Entry.Value;
			return Joined;
		};
		auto Colour = [this, bHiddenLine]() -> FSlateColor
		{
			if (bHiddenLine) return Blood.CopyWithNewOpacity(.82f);
			const auto Lines = Visible();
			const ETallowSubtitleVoice Voice = Lines.IsEmpty() ? ETallowSubtitleVoice::Told : Lines[0].Key;
			if (Voice == ETallowSubtitleVoice::Nell) return Ivory.CopyWithNewOpacity(.95f);
			if (Voice == ETallowSubtitleVoice::Mim) return FMath::Lerp(Ink, Blood, .3f).CopyWithNewOpacity(.95f);
			return Ink.CopyWithNewOpacity(.95f);
		};
		return SNew(SOverlay)
			.Visibility_Lambda([Words]() { return Words().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			+ SOverlay::Slot()[SNew(STallowSmudge).Alpha(.8f)]
			+ SOverlay::Slot().Padding(FMargin(14.f, 5.f))
			[SNew(STextBlock).Text_Lambda([Words]() { return FText::FromString(Words()); }).Font(FellItalic(Pt::Small + 4))
				.ColorAndOpacity_Lambda(Colour).Justification(ETextJustify::Center).WrapTextAt(1100.f).LineHeightPercentage(1.05f)];
	};
	Widget = SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 0.f, TallowSubtitles::BottomMargin))
		[SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Line(true)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Line(false)]];
	Viewport->AddViewportWidgetContent(Widget.ToSharedRef(), 79);
}

void UTallowSubtitleSubsystem::Deinitialize()
{
	bLive = false;
	Tracks.Reset();
	if (Widget.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr) Viewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
		Widget.Reset();
	}
	Super::Deinitialize();
}
