#include "Audio/TallowCorruptedVoice.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameMode/TallowGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundConcurrency.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SOverlay.h"
#include "UI/Presentation/TallowSubtitles.h"

DEFINE_LOG_CATEGORY_STATIC(LogTallowCorrupt, Log, All);

namespace
{
	constexpr float CancelFadeSeconds = .15f;   // named apart: unity builds put this beside other files' FadeSeconds
	const TCHAR* const Section = TEXT("Tallow.Settings");
}

const TArray<FTallowCorruptCue>& TallowCorrupted::Cues()
{
	static const TArray<FTallowCorruptCue> All = []
	{
		struct FRow { const TCHAR* Line; const TCHAR* Group; const TCHAR* Key; float Start, Gap, Duration, Interrupt; ETallowCorruptPlacement Placement; int32 MinPlaythrough; const TCHAR* Asset; const TCHAR* Subtitle; };
		static const FRow Rows[] = {
#include "Audio/TallowCorruptedCues.inl"
		};
		TArray<FTallowCorruptCue> Out;
		for (const FRow& R : Rows)
		{
			FTallowCorruptCue C;
			C.Line = R.Line; C.Group = R.Group; C.Key = R.Key; C.Start = R.Start; C.Gap = R.Gap; C.Duration = R.Duration;
			C.Interrupt = R.Interrupt; C.Placement = R.Placement; C.MinPlaythrough = R.MinPlaythrough; C.Asset = R.Asset; C.Subtitle = R.Subtitle;
			Out.Add(C);
		}
		return Out;
	}();
	return All;
}

bool TallowCorrupted::ShowHiddenVoice()
{
	bool bShow = false;
	if (GConfig) GConfig->GetBool(Section, TEXT("ShowHiddenVoice"), bShow, GGameUserSettingsIni);
	return bShow;
}

void TallowCorrupted::SetShowHiddenVoice(bool bShow)
{
	if (!GConfig) return;
	GConfig->SetBool(Section, TEXT("ShowHiddenVoice"), bShow, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

bool TallowCorrupted::StoryCompleted()
{
	bool bDone = false;
	if (GConfig) GConfig->GetBool(Section, TEXT("StoryCompleted"), bDone, GGameUserSettingsIni);
	return bDone;
}

void TallowCorrupted::MarkStoryCompleted()
{
	if (!GConfig || StoryCompleted()) return;
	GConfig->SetBool(Section, TEXT("StoryCompleted"), true, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

float TallowCorrupted::AfterEnd(FName Key, bool bSecondPlaythrough)
{
	float End = 0.f;
	TMap<FName, float> GroupEnd;
	for (const FTallowCorruptCue& C : Cues())
	{
		if (C.Key != Key || C.MinPlaythrough > (bSecondPlaythrough ? 2 : 1)) continue;
		float& E = GroupEnd.FindOrAdd(C.Group, C.Start);
		E += C.Gap + C.Duration;
		if (C.Placement == ETallowCorruptPlacement::After) End = FMath::Max(End, E);
	}
	return End;
}

// Scheduler

const FTallowCorruptScheduler::FSession* FTallowCorruptScheduler::Find(int32 Session) const
{
	return Sessions.FindByPredicate([Session](const FSession& S) { return S.Id == Session; });
}

FTallowCorruptScheduler::FSession* FTallowCorruptScheduler::Find(int32 Session)
{
	return Sessions.FindByPredicate([Session](const FSession& S) { return S.Id == Session; });
}

int32 FTallowCorruptScheduler::NumDetached() const
{
	int32 N = 0;
	for (const FSession& S : Sessions) N += S.bDetached;
	return N;
}

int32 FTallowCorruptScheduler::Begin(FName Key, const TArray<FTallowCorruptCue>& All, float Duration, bool bSecondPlaythrough, TArray<FTallowCorruptEvent>& Out)
{
	for (FSession& Old : Sessions) if (Old.Key == Key) Cancel(Old, Out);
	Prune();
	FSession S;
	S.Key = Key; S.Duration = FMath::Max(0.f, Duration);
	for (const FTallowCorruptCue& C : All)
	{
		if (C.Key != Key || C.MinPlaythrough > (bSecondPlaythrough ? 2 : 1)) continue;
		FGroup* G = S.Groups.FindByPredicate([&C](const FGroup& X) { return X.Members[0].Cue->Group == C.Group; });
		if (!G)
		{
			G = &S.Groups.AddDefaulted_GetRef();
			G->Start = C.Start; G->bAfter = C.Placement == ETallowCorruptPlacement::After;
		}
		FMember M; M.Cue = &C;
		if (!G->Members.IsEmpty()) M.Offset = G->Members.Last().Offset + G->Members.Last().Cue->Duration + C.Gap;
		G->Members.Add(M);
		G->Hold = FMath::Max(G->Hold, C.Interrupt);
	}
	if (S.Groups.IsEmpty()) return 0;
	S.Id = NextId++;
	Sessions.Add(S);
	return S.Id;
}

void FTallowCorruptScheduler::Step(FSession& S, float Delta, float Narration, TArray<FTallowCorruptEvent>& Out)
{
	auto StartMember = [&](FGroup& G, FMember& M)
	{
		M.bStarted = true;
		FTallowCorruptEvent E; E.Kind = FTallowCorruptEvent::EKind::Start; E.Cue = M.Cue; E.Session = S.Id; E.Time = G.FiredAt + M.Offset;
		Out.Add(E);
	};
	auto Run = [&](FGroup& G)
	{
		bool bAll = true;
		for (FMember& M : G.Members)
		{
			if (!M.bStarted && G.ChainTime >= M.Offset) StartMember(G, M);
			if (M.bStarted && !M.bEnded && G.ChainTime >= M.Offset + M.Cue->Duration)
			{
				M.bEnded = true;
				FTallowCorruptEvent E; E.Kind = FTallowCorruptEvent::EKind::End; E.Cue = M.Cue; E.Session = S.Id;
				Out.Add(E);
			}
			bAll &= M.bEnded;
		}
		G.bDone = bAll;
	};
	for (FGroup& G : S.Groups) if (G.bActive && !G.bDone) { G.ChainTime += Delta; Run(G); }
	for (FGroup& G : S.Groups)
		if (!G.bActive && !G.bDone && Narration >= G.Start) { G.bActive = true; G.ChainTime = 0.f; G.FiredAt = Narration; Run(G); }
}

void FTallowCorruptScheduler::Advance(int32 Session, float Delta, float Position, ETallowCorruptClock Clock, TArray<FTallowCorruptEvent>& Out)
{
	FSession* S = Find(Session);
	if (!S || Clock == ETallowCorruptClock::Paused) return;
	Delta = FMath::Max(0.f, Delta);
	float Narration = Position;
	if (Clock == ETallowCorruptClock::Finished) { S->PostEnd += Delta; Narration = S->Duration + S->PostEnd; }
	Step(*S, Delta, Narration, Out);
}

void FTallowCorruptScheduler::AdvanceDetached(float Delta, bool bPaused, TArray<FTallowCorruptEvent>& Out)
{
	if (!bPaused)
		for (FSession& S : Sessions)
			if (S.bDetached) { S.PostEnd += Delta; Step(S, Delta, S.Duration + S.PostEnd, Out); }
	Prune();
}

void FTallowCorruptScheduler::Cancel(FSession& S, TArray<FTallowCorruptEvent>& Out)
{
	for (FGroup& G : S.Groups)
	{
		for (FMember& M : G.Members)
			if (M.bStarted && !M.bEnded)
			{
				FTallowCorruptEvent E; E.Kind = FTallowCorruptEvent::EKind::Cancel; E.Cue = M.Cue; E.Session = S.Id;
				Out.Add(E);
			}
		G.bDone = true;
	}
	S.bDetached = true;
}

void FTallowCorruptScheduler::Release(int32 Session, bool bFinished, TArray<FTallowCorruptEvent>& Out)
{
	FSession* S = Find(Session);
	if (!S) return;
	if (!bFinished) { Cancel(*S, Out); Prune(); return; }
	S->bDetached = true;
	for (FGroup& G : S->Groups) if (!G.bActive && !G.bAfter) G.bDone = true;
	Prune();
}

void FTallowCorruptScheduler::CancelAll(TArray<FTallowCorruptEvent>& Out)
{
	for (FSession& S : Sessions) Cancel(S, Out);
	Sessions.Reset();
}

void FTallowCorruptScheduler::Prune()
{
	Sessions.RemoveAll([](const FSession& S)
	{
		if (!S.bDetached) return false;
		for (const FGroup& G : S.Groups) if (!G.bDone) return false;
		return true;
	});
}

float FTallowCorruptScheduler::HoldRemaining(int32 Session) const
{
	const FSession* S = Find(Session);
	float Hold = 0.f;
	if (S) for (const FGroup& G : S->Groups) if (G.bActive && !G.bDone && G.Hold > 0.f) Hold = FMath::Max(Hold, G.Hold - G.ChainTime);
	return Hold;
}

// Subsystem

UTallowCorruptedVoiceSubsystem* UTallowCorruptedVoiceSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UTallowCorruptedVoiceSubsystem>() : nullptr;
}

int32 UTallowCorruptedVoiceSubsystem::Begin(FName Key, float Duration)
{
	TArray<FTallowCorruptEvent> Events;
	const int32 Session = Scheduler.Begin(Key, TallowCorrupted::Cues(), Duration, TallowCorrupted::StoryCompleted(), Events);
	Handle(Events);
	return Session;
}

void UTallowCorruptedVoiceSubsystem::Advance(int32 Session, float Delta, float Position, ETallowCorruptClock Clock)
{
	TArray<FTallowCorruptEvent> Events;
	Scheduler.Advance(Session, Delta, Position, Clock, Events);
	Handle(Events);
}

void UTallowCorruptedVoiceSubsystem::Release(int32 Session, bool bFinished)
{
	TArray<FTallowCorruptEvent> Events;
	Scheduler.Release(Session, bFinished, Events);
	Handle(Events);
}

void UTallowCorruptedVoiceSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Scheduler.Num() == 0 && Playing.IsEmpty()) return;
	TArray<FTallowCorruptEvent> Events;
	// A capture ends everything, including lines owed after a page that was put away.
	const auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ATallowGameMode>() : nullptr;
	if (GM && GM->IsCaptureInProgress()) Scheduler.CancelAll(Events);
	const bool bPaused = UGameplayStatics::IsGamePaused(this);
	Scheduler.AdvanceDetached(DeltaTime, bPaused, Events);
	Handle(Events);
	if (bPaused != bPausedAudio)
	{
		bPausedAudio = bPaused;
		for (const FPlaying& P : Playing) if (P.Component.IsValid()) P.Component->SetPaused(bPaused);
	}
}

void UTallowCorruptedVoiceSubsystem::Handle(const TArray<FTallowCorruptEvent>& Events)
{
	bool bSubtitles = false;
	for (const FTallowCorruptEvent& E : Events)
	{
		if (E.Kind == FTallowCorruptEvent::EKind::Start) { Play(E); bSubtitles = true; continue; }
		const int32 Index = Playing.IndexOfByPredicate([&E](const FPlaying& P) { return P.Session == E.Session && P.Line == E.Cue->Line; });
		if (Index == INDEX_NONE) continue;
		if (E.Kind == FTallowCorruptEvent::EKind::Cancel && Playing[Index].Component.IsValid()) Playing[Index].Component->FadeOut(CancelFadeSeconds, 0.f);
		Playing.RemoveAtSwap(Index);
		bSubtitles = true;
	}
	if (bSubtitles) ShowSubtitles();
}

void UTallowCorruptedVoiceSubsystem::Play(const FTallowCorruptEvent& Event)
{
	const FTallowCorruptCue& Cue = *Event.Cue;
	UE_LOG(LogTemp, Display, TEXT("TALLOW_CORRUPT cue=%s reading=%s t=%.2f"), *Cue.Line.ToString(), *Cue.Key.ToString(), Event.Time);
	// The same line never stacks on itself; the scheduler already ended the earlier run of a restarted recording.
	if (Playing.ContainsByPredicate([&Cue](const FPlaying& P) { return P.Line == Cue.Line && P.Component.IsValid() && P.Component->IsPlaying(); })) return;
	FPlaying Entry; Entry.Session = Event.Session; Entry.Line = Cue.Line; Entry.Subtitle = Cue.Subtitle;
	if (GEngine && GEngine->UseSound())
	{
		if (!Concurrency)
		{
			Concurrency = NewObject<USoundConcurrency>(this);
			Concurrency->Concurrency.MaxCount = 2;
			Concurrency->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopOldest;
		}
		if (!NarrationClass) NarrationClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/Mix/SC_Tallow_Narration.SC_Tallow_Narration"));
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Cue.Asset))
			if (UAudioComponent* Component = UGameplayStatics::CreateSound2D(this, Sound, 1.f, 1.f, 0.f, Concurrency, false, true))
			{
				// Levels are baked into the takes: unity here, the narration class carries the ducking. The class goes on
				// before Play: a sound takes its class when it starts, so set after it the line played on no class at all.
				if (NarrationClass) Component->SoundClassOverride = NarrationClass;
				Component->bIsUISound = false;
				Component->Play();
				Entry.Component = Component;
			}
	}
	Playing.Add(Entry);
}

void UTallowCorruptedVoiceSubsystem::ShowSubtitles()
{
	// The words go to the one subtitle display, in the hidden voice's own ink; it shows them only with subtitles on and
	// "show hidden voice" chosen.
	FString Joined;
	for (const FPlaying& P : Playing) Joined += (Joined.IsEmpty() ? TEXT("") : TEXT("\n")) + P.Subtitle;
	if (auto* Subtitles = UTallowSubtitleSubsystem::Get(this)) Subtitles->SetHidden(Joined);
}

void UTallowCorruptedVoiceSubsystem::Deinitialize()
{
	TArray<FTallowCorruptEvent> Events;
	Scheduler.CancelAll(Events);
	Playing.Reset();
	Super::Deinitialize();
}
