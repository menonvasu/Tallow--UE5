#include "Audio/TallowKeeperVoiceComponent.h"
#include "UI/Presentation/TallowSubtitles.h"
#include "AI/TallowCreature.h"
#include "Player/TallowCharacter.h"
#include "TimerManager.h"
#include "GameMode/TallowGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundConcurrency.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"

namespace
{
	// My lines (SourceAssets/Audio/Recording/Creature/Lines/lines.json), by what it is doing.
	struct FLines { ETallowKeeperLine Line; const TCHAR* Names[8]; float Gap, Volume; };
	const FLines KeeperLines[] = {
		{ETallowKeeperLine::Lure,    {TEXT("00"), TEXT("01"), TEXT("02"), TEXT("04"), TEXT("05"), TEXT("06"), TEXT("20")}, 90.f, .42f},
		{ETallowKeeperLine::Murmur,  {TEXT("24"), TEXT("26"), TEXT("10")}, 180.f, .38f},
		{ETallowKeeperLine::Heard,   {TEXT("13"), TEXT("14"), TEXT("11")}, 90.f, .5f},
		{ETallowKeeperLine::Search,  {TEXT("12"), TEXT("16"), TEXT("18"), TEXT("15"), TEXT("VO_Mim_Hide"), TEXT("VO_Mim_ComeOut"), TEXT("VO_Mim_WontLetGo")}, 75.f, .5f},
		{ETallowKeeperLine::Seen,    {TEXT("07"), TEXT("09"), TEXT("17"), TEXT("19")}, 30.f, .6f},
		{ETallowKeeperLine::Chase,   {TEXT("22a"), TEXT("22b"), TEXT("22c"), TEXT("03")}, 60.f, .6f},
		{ETallowKeeperLine::Lost,    {TEXT("08"), TEXT("25")}, 60.f, .5f},
		{ETallowKeeperLine::Capture, {TEXT("23"), TEXT("27")}, 0.f, .7f},
		{ETallowKeeperLine::Breath,  {TEXT("28"), TEXT("29"), TEXT("30"), TEXT("31"), TEXT("32")}, 60.f, .35f},
		{ETallowKeeperLine::Giggle,  {TEXT("VO_Mim_Giggle_00"), TEXT("VO_Mim_Giggle_01"), TEXT("VO_Mim_Giggle_02")}, 120.f, .55f},
	};

	// Ordinary lines (murmur, lure, heard, search, chase begging, breath, giggle) share one slow clock, so she speaks
	// rarely. Only the sight call, losing you and the capture are exempt: those are the moments worth a line.
	constexpr float OrdinaryGap = 75.f;
	// The same recording is never heard twice inside this many seconds.
	constexpr float RepeatBlock = 300.f;
	// She only calls out a search when you are within earshot of the room she is searching.
	constexpr float SearchCallRange = 1200.f;

	bool IsMoment(ETallowKeeperLine Line)
	{
		return Line == ETallowKeeperLine::Seen || Line == ETallowKeeperLine::Lost || Line == ETallowKeeperLine::Capture;
	}

	bool IsSearching(ETallowCreatureState State)
	{
		return State == ETallowCreatureState::SearchingCorner || State == ETallowCreatureState::SearchHigh
			|| State == ETallowCreatureState::SearchLow;
	}
}

UTallowKeeperVoiceComponent::UTallowKeeperVoiceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	for (const FLines& L : KeeperLines)
	{
		FPool& Pool = Pools.Add(L.Line);
		for (const TCHAR* N : L.Names) if (N) Pool.Names.Add(FCString::Strncmp(N, TEXT("VO_"), 3) == 0 ? FString(N) : FString::Printf(TEXT("V_Creature_%s"), N));
		Pool.Gap = L.Gap; Pool.Volume = L.Volume;
	}
}

void UTallowKeeperVoiceComponent::BeginPlay()
{
	Super::BeginPlay();
	// The voice booth export lists approved recordings here. Adopt a new pool only
	// when its assets are actually imported; an unfinished pass keeps the old pool.
	auto Recorded = [](const FString& Key)
	{
		TArray<FString> Names, Available;
		if (GConfig) GConfig->GetArray(TEXT("Tallow.RecordedVoice"), *Key, Names, GGameIni);
		for (const FString& Name : Names)
			if (Name.StartsWith(TEXT("V_Mim_")) && !Name.Contains(TEXT("/"))
				&& FPackageName::DoesPackageExist(TEXT("/Game/Audio/Creature/") + Name)) Available.AddUnique(Name);
		return Available;
	};
	for (auto& Pool : Pools)
	{
		const FString Key = StaticEnum<ETallowKeeperLine>()->GetNameStringByValue(static_cast<int64>(Pool.Key));
		auto Names = Recorded(Key);
		if (!Names.IsEmpty()) { Pool.Value.Names = MoveTemp(Names); Pool.Value.Bag.Reset(); }
	}
	DemonPool.Names = Recorded(TEXT("RareDemon"));
	NextDemon = GetWorld()->GetTimeSeconds();
	Attenuation = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& A = Attenuation->Attenuation;
	A.bAttenuate = true; A.bSpatialize = true;
	A.AttenuationShapeExtents = FVector(150.f); A.FalloffDistance = HearingDistance;
	A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound; A.dBAttenuationAtMax = -35.f;
	// Through a wall it goes dull and quieter, so you can tell whether it is in your room.
	A.bEnableOcclusion = true; A.OcclusionLowPassFilterFrequency = 900.f; A.OcclusionVolumeAttenuation = .45f;
	// Dry right beside you, wetter with distance: far off it sounds like the next room (room reverb volumes).
	A.bEnableReverbSend = true;
	A.ReverbWetLevelMin = .05f; A.ReverbDistanceMin = 200.f; A.ReverbWetLevelMax = .7f; A.ReverbDistanceMax = 1500.f;
	FoleyAttenuation = NewObject<USoundAttenuation>(this);
	FoleyAttenuation->Attenuation = A; FoleyAttenuation->Attenuation.FalloffDistance = 1400.f;
	for (const auto& Pool : Pools) for (const FString& N : Pool.Value.Names) Sound(N);
	for (const FString& Name : DemonPool.Names) Sound(Name);
	Sound(TEXT("V_Creature_21"));
	NextAny = GetWorld()->GetTimeSeconds() + FMath::FRandRange(8.f, 15.f);
	if (ATallowGameMode* Mode = Cast<ATallowGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		Mode->OnCaught.AddUniqueDynamic(this, &UTallowKeeperVoiceComponent::PlayCapture);
	}
}

void UTallowKeeperVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearCaptureTimers();
	if (Speaking) Speaking->Stop();
	if (FoleyPlaying) FoleyPlaying->Stop();
	if (ATallowGameMode* Mode = Cast<ATallowGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		Mode->OnCaught.RemoveDynamic(this, &UTallowKeeperVoiceComponent::PlayCapture);
	}
	Super::EndPlay(EndPlayReason);
}

void UTallowKeeperVoiceComponent::ClearCaptureTimers()
{
	if (!GetWorld()) return;
	auto& Timers = GetWorld()->GetTimerManager();
	Timers.ClearTimer(StrikeHandle); Timers.ClearTimer(ScreamHandle);
	Timers.ClearTimer(GiggleHandle); Timers.ClearTimer(CaptureLineHandle);
}

int32 UTallowKeeperVoiceComponent::GetPendingCaptureCueCount() const
{
	if (!GetWorld()) return 0;
	const auto& Timers = GetWorld()->GetTimerManager();
	return int32(Timers.IsTimerActive(StrikeHandle)) + int32(Timers.IsTimerActive(ScreamHandle))
		+ int32(Timers.IsTimerActive(GiggleHandle)) + int32(Timers.IsTimerActive(CaptureLineHandle));
}

void UTallowKeeperVoiceComponent::ResetVoice()
{
	ClearCaptureTimers();
	if (Speaking) Speaking->Stop();
	if (FoleyPlaying) FoleyPlaying->Stop();
	bCapturing = false;
	CaptureEndsAt = QuietUntil = 0.f;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	NextAny = Now + 5.f;
	NextOrdinary = Now + 20.f;
	PlayedAt.Reset();
	NextFoley = NextFoleyAllowed = Now + 4.f;
	ChaseStarted = -100.f;
	AggressiveUntil = 0.f;
	if (const auto* Player = Cast<ATallowCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		HeardSeenCall = Player->SeenCallAt;
	for (auto& Pool : Pools) Pool.Value.Ready = Now;
}

void UTallowKeeperVoiceComponent::SetAwake(bool bOn)
{
	if (bAwake == bOn) return;
	ResetVoice();
	bAwake = bOn;
	if (bOn && GetWorld()) NextAny = GetWorld()->GetTimeSeconds() + FMath::FRandRange(3.f, 6.f);
}

void UTallowKeeperVoiceComponent::CaptionLine(USoundBase* Cue)
{
	// Only what she could hear: its voice fades out at HearingDistance. Sounds with no words in them have no caption.
	const AActor* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Cue || !Player || !GetOwner() || FVector::Dist(Player->GetActorLocation(), GetOwner()->GetActorLocation()) > HearingDistance) return;
	if (auto* Subtitles = UTallowSubtitleSubsystem::Get(this)) Subtitles->Begin(Cue->GetFName(), Cue->GetDuration(), ETallowSubtitleVoice::Mim);
}

bool UTallowKeeperVoiceComponent::Speak(USoundBase* Cue, float Volume, float Pitch)
{
	if (!Cue || !GetOwner()) return false;
	if (Speaking && Speaking->IsPlaying()) Speaking->FadeOut(.018f, 0.f);
	if(bCapturing&&Cue->GetFName()==TEXT("K_Mim_Capture_Scream"))
	{
		// The grab is already at the camera. Keep its close scream out of the
		// ordinary body-distance and wall filters, while respecting game pause.
		// It goes out on the capture voice's own class and concurrency, which nothing else uses in the shot: the
		// moments class (0.7) and the held-breath muffle sat it 8-14 dB under the sting, where it was lost.
		auto* Concurrency=LoadObject<USoundConcurrency>(nullptr,TEXT("/Game/Audio/Mix/CC_Tallow_CaptureVoice.CC_Tallow_CaptureVoice"),nullptr,LOAD_NoWarn|LOAD_Quiet);
		Speaking=UGameplayStatics::CreateSound2D(this,Cue,Volume,Pitch,0.f,Concurrency,false,true);
		if(Speaking)
		{
			Speaking->bIsUISound=false;
			if(auto* Class=LoadObject<USoundClass>(nullptr,TEXT("/Game/Audio/Mix/SC_Tallow_CaptureVoice.SC_Tallow_CaptureVoice"),nullptr,LOAD_NoWarn|LOAD_Quiet))
				Speaking->SoundClassOverride=Class;
			Speaking->Play();
		}
		CaptionLine(Cue);
		return Speaking!=nullptr;
	}
	Speaking = UGameplayStatics::SpawnSoundAttached(Cue, GetOwner()->GetRootComponent(), NAME_None, FVector(0, 0, 60),
		EAttachLocation::KeepRelativeOffset, false, Volume, Pitch, 0.f, Attenuation);
	if (Speaking) CaptionLine(Cue);
	return Speaking != nullptr;
}

bool UTallowKeeperVoiceComponent::IsSpeaking() const
{return Speaking && Speaking->IsPlaying();}

USoundBase* UTallowKeeperVoiceComponent::Sound(const FString& Name)
{
	if (const TObjectPtr<USoundBase>* Found = Cache.Find(Name)) return *Found;
	// My creature lines and foley live in Audio/Creature; the story's scripted Mim lines (VO_Mim_*) in Audio/VO.
	const TCHAR* Folder = Name.StartsWith(TEXT("VO_")) ? TEXT("VO") : TEXT("Creature");
	USoundBase* Loaded = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/%s/%s.%s"), Folder, *Name, *Name));
	if (!Loaded) UE_LOG(LogTemp, Warning, TEXT("TALLOW_AUDIO missing %s/%s"), Folder, *Name);
	Cache.Add(Name, Loaded);
	return Loaded;
}

USoundBase* UTallowKeeperVoiceComponent::Next(ETallowKeeperLine Line)
{
	FPool* Pool = Pools.Find(Line);
	if (!Pool || Pool->Names.IsEmpty()) return nullptr;
	return Draw(*Pool);
}

USoundBase* UTallowKeeperVoiceComponent::Draw(FPool& InPool)
{
	FPool* Pool = &InPool;
	if (Pool->Names.IsEmpty()) return nullptr;
	// A shuffled bag: every line in the pool plays once before any plays again. Lines not recorded yet are skipped.
	for (int32 Tries = 0; Tries < 2 * Pool->Names.Num(); ++Tries)
	{
		if (Pool->Bag.IsEmpty())
		{
			for (int32 I = 0; I < Pool->Names.Num(); ++I) Pool->Bag.Insert(I, FMath::RandRange(0, Pool->Bag.Num()));
		}
		if (USoundBase* Cue = Sound(Pool->Names[Pool->Bag.Pop()])) return Cue;
	}
	return nullptr;
}

bool UTallowKeeperVoiceComponent::Say(ETallowKeeperLine Line, bool bForce)
{
	if(bRecoveryQuiet && !bForce && (Line==ETallowKeeperLine::Murmur || Line==ETallowKeeperLine::Lure || Line==ETallowKeeperLine::Giggle)) return false;
	const float Now = GetWorld()->GetTimeSeconds();
	FPool* Pool = Pools.Find(Line);
	if (!Pool || !bAwake || bCapturing || Now < QuietUntil || Now < Pool->Ready) return false;
	if (bCreeping && !IsMoment(Line)) return false;
	const bool bTalking = Speaking && Speaking->IsPlaying();
	if (!bForce && (bTalking || Now < NextAny)) return false;
	if (!bForce && !IsMoment(Line) && Now < NextOrdinary) return false;
	// Aggression brings the low voice out. About two in five eligible phrases,
	// with a short gap; calm wandering, lures and quiet breathing stay sweet.
	const auto* Keeper = Cast<ATallowCreature>(GetOwner());
	const bool bAggressive = Keeper && (Keeper->CurrentState == ETallowCreatureState::Chasing
		|| (IsSearching(Keeper->CurrentState) && Now < AggressiveUntil));
	const bool bEligible = bAggressive && (Line == ETallowKeeperLine::Chase || Line == ETallowKeeperLine::Search);
	const bool bDemon = bEligible && !bForce && !DemonPool.Names.IsEmpty()
		&& Now >= NextDemon && FMath::FRand() < DemonChance;
	USoundBase* Cue = bDemon ? Draw(DemonPool) : Next(Line);
	// Never the same recording close together, even when the shuffled bag turns over.
	for (int32 Tries = 0; Cue && Tries < 3; ++Tries)
	{
		const float* Last = PlayedAt.Find(Cue->GetFName());
		if (!Last || Now - *Last > RepeatBlock) break;
		Cue = bDemon ? Draw(DemonPool) : Next(Line);
	}
	if (!Cue) return false;
	if (const float* Last = PlayedAt.Find(Cue->GetFName())) if (Now - *Last <= RepeatBlock) return false;
	// Her words come up; the wordless breath pool stays at its level.
	const float Volume = Pool->Volume * (Line == ETallowKeeperLine::Breath ? 1.f : SpokenLift());
	if (!Speak(Cue, Volume, FMath::FRandRange(.985f, 1.015f))) return false;
	if (bDemon) NextDemon = Now + Cue->GetDuration() + DemonCooldown;
	UE_LOG(LogTemp, Display, TEXT("TALLOW_VOICE %s %s vol=%.2f"), *UEnum::GetValueAsString(Line), *Cue->GetName(), Volume);
	PlayedAt.Add(Cue->GetFName(), Now);
	Pool->Ready = Now + Pool->Gap * FMath::FRandRange(.8f, 1.4f);
	if (!IsMoment(Line)) NextOrdinary = Now + OrdinaryGap * FMath::FRandRange(1.f, 1.5f);
	const bool bUrgent = Line == ETallowKeeperLine::Seen || Line == ETallowKeeperLine::Chase;
	NextAny = Now + Cue->GetDuration() + (bUrgent ? FMath::FRandRange(8.f, 14.f) : FMath::FRandRange(15.f, 25.f));
	return Speaking != nullptr;
}

bool UTallowKeeperVoiceComponent::SayLine(const FString& Name, const FString& Fallback, float Volume)
{
	if (bCapturing) return false;
	USoundBase* Cue = Sound(Name);
	if (!Cue && !Fallback.IsEmpty()) Cue = Sound(Fallback);
	if (!Cue) return false;
	UE_LOG(LogTemp, Display, TEXT("TALLOW_VOICE Scripted %s vol=%.2f"), *Cue->GetName(), Volume * SpokenLift());
	if (!Speak(Cue, Volume * SpokenLift())) return false;
	NextAny = GetWorld()->GetTimeSeconds() + Cue->GetDuration() + 3.f;
	return Speaking != nullptr;
}

void UTallowKeeperVoiceComponent::Foley(bool bKnock, float Volume)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!bAwake || bCapturing || bCreeping || Now < QuietUntil || Now < NextFoleyAllowed
		|| (FoleyPlaying && FoleyPlaying->IsPlaying())) return;
	int32& Last = bKnock ? LastKnock : LastScratch;
	const int32 Count = bKnock ? 3 : 4;
	int32 Pick = FMath::RandRange(0, Count - 1);
	if (Pick == Last) Pick = (Pick + 1 + FMath::RandRange(0, Count - 2)) % Count;
	Last = Pick;
	const FString CueName = bKnock ? FString::Printf(TEXT("C_Knock_%02d"), Pick)
		: FString::Printf(TEXT("C_Scratch_%02d"), Pick);
	USoundBase* Cue = Sound(CueName);
	if (Cue)
	{
		UE_LOG(LogTemp, Display, TEXT("TALLOW_VOICE Foley %s"), *Cue->GetName());
	}
	if (Cue)
	{
		FoleyPlaying = UGameplayStatics::SpawnSoundAttached(Cue, GetOwner()->GetRootComponent(), NAME_None, FVector(0, 0, 40),
			EAttachLocation::KeepRelativeOffset, false, Volume, FMath::FRandRange(.94f, 1.03f), 0.f, FoleyAttenuation);
		NextFoleyAllowed = Now + Cue->GetDuration() + (bKnock ? 1.8f : 3.5f);
	}
}

void UTallowKeeperVoiceComponent::UpdateVoice(float Now)
{
	const ATallowCreature* Keeper = Cast<ATallowCreature>(GetOwner());
	const auto* Player = Cast<ATallowCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Keeper || !Player || !bAwake || bCapturing) return;
	const ETallowCreatureState State = Keeper->CurrentState;
	const ETallowCreatureState Previous = static_cast<ETallowCreatureState>(LastState);
	LastState = static_cast<uint8>(State);
	if (Now < QuietUntil) return;
	const float Distance = FVector::Dist(Player->GetActorLocation(), Keeper->GetActorLocation());

	// The call the moment it sees you: it cuts across anything else it was saying.
	if (Player->SeenCallAt > HeardSeenCall + .01f)
	{
		HeardSeenCall = Player->SeenCallAt;
		Say(ETallowKeeperLine::Seen, true);
		return;
	}
	if (State != Previous)
	{
		if (State == ETallowCreatureState::Chasing) ChaseStarted = Now;
		if (State == ETallowCreatureState::Investigating && FMath::FRand() < .35f) Say(ETallowKeeperLine::Heard);
		if (Previous == ETallowCreatureState::Chasing) { AggressiveUntil = Now + 25.f; Say(ETallowKeeperLine::Lost); }
	}

	if (State == ETallowCreatureState::Chasing)
	{
		// Begging you to wait, between gasps; never over its own seen call.
		const float Roll = FMath::FRand();
		const ETallowKeeperLine Pick = Roll < .2f ? ETallowKeeperLine::Giggle : Roll < .4f ? ETallowKeeperLine::Breath : ETallowKeeperLine::Chase;
		if (Now - ChaseStarted > 2.5f && !Say(Pick))
			return;
	}
	else if (bDisguised)
	{
		// The soft heap talks to you only when you are close enough to come and see. (It sits in the same low state
		// as a search, so this has to come first: it used to say "come out" and scratch the boards instead.)
		if (Distance < 500.f) Say(ETallowKeeperLine::Lure);
	}
	else if (bLurking)
	{
		// Waiting still: nothing to hear but the breath, below, when you are right beside it.
	}
	else if (IsSearching(State))
	{
		if (Distance < SearchCallRange) Say(ETallowKeeperLine::Search);
		// Its many hands feeling along the boards: the honest tell of where it is looking.
		if (Now >= NextFoley)
		{
			NextFoley = Now + FMath::FRandRange(8.f, 15.f);
			Foley(FMath::FRand() < .3f);
		}
	}
	else if (State == ETallowCreatureState::Patrol || State == ETallowCreatureState::SuspiciousIdle)
	{
		if (Distance < 1500.f) Say(ETallowKeeperLine::Murmur);
	}
	// Right beside you and not saying anything: a held breath, a hiss.
	if (!bDisguised && State != ETallowCreatureState::Chasing && Distance < 250.f) Say(ETallowKeeperLine::Breath);
}

void UTallowKeeperVoiceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateVoice(GetWorld()->GetTimeSeconds());
}

void UTallowKeeperVoiceComponent::PlayCapture()
{
	// One creature voice at a time, on the same authored strike as the hands and
	// camera. Delayed lines only play if they fit before the capture reset.
	ClearCaptureTimers();
	const float Now = GetWorld()->GetTimeSeconds();
	const auto* Keeper = Cast<ATallowCreature>(GetOwner());
	const float Strike = Keeper ? Keeper->GetCaptureStrikeTime() : .55f;
	const float Duration = Keeper ? Keeper->GetCapturePresentationDuration() : 1.9f;
	CaptureEndsAt = Now + FMath::Max(Duration, Strike + .85f);
	QuietUntil = CaptureEndsAt + 2.f;
	bCapturing = true;
	if (Speaking) Speaking->Stop();
	if (FoleyPlaying) FoleyPlaying->Stop();
	auto Strike_ = [this]
	{
		if(!bCapturing)return;
		USoundBase* Scream = Sound(TEXT("K_Mim_Capture_Scream"));
		// .25 before the 30 Oct lift; with the strike sting (peak 0 dBFS at .25) the frame peaks near -5 dBFS.
		const float ScreamVolume = .25f * SpokenLift();
		if (Scream) Speak(Scream, ScreamVolume);
		UE_LOG(LogTemp, Display, TEXT("TALLOW_VOICE CaptureStrike %s lead %.2fs class %s vol=%.2f"), *GetNameSafe(Scream), ScreamLeadSeconds,
			Speaking ? *GetNameSafe(Speaking->SoundClassOverride) : TEXT("none (no audio device)"), ScreamVolume);
		// Let the impact and scream end in silence. A spoken punchline or
		// follow-up giggle turns the capture into a routine announcement.
	};
	// The recorded scream opens on about a tenth of a second of breath, then its attack. Started early enough that
	// the attack comes as the jaw snaps open (frame 15), two frames before the sting, so the ear has it before the
	// sting lands on top.
	const float ScreamAt = Strike - ScreamLeadSeconds;
	if (ScreamAt > .02f) GetWorld()->GetTimerManager().SetTimer(StrikeHandle, FTimerDelegate::CreateWeakLambda(this, Strike_), ScreamAt, false);
	else Strike_();
}

void UTallowKeeperVoiceComponent::PlayCaptureGiggle()
{
	if (!bCapturing) return;
	USoundBase* Laugh = Next(ETallowKeeperLine::Giggle);
	const float Left = CaptureEndsAt - GetWorld()->GetTimeSeconds();
	if (Laugh && Laugh->GetDuration() + .15f < Left)
	{
		Speak(Laugh, .54f * SpokenLift());
		UE_LOG(LogTemp, Display, TEXT("TALLOW_VOICE CaptureGiggle %s"), *Laugh->GetName());
		GetWorld()->GetTimerManager().SetTimer(CaptureLineHandle, this,
			&UTallowKeeperVoiceComponent::PlayCaptureLine, Laugh->GetDuration() + .2f, false);
	}
	else PlayCaptureLine();
}

void UTallowKeeperVoiceComponent::PlayCaptureLine()
{
	if (!bCapturing) return;
	USoundBase* Mine = Next(ETallowKeeperLine::Capture);
	const float Left = CaptureEndsAt - GetWorld()->GetTimeSeconds();
	if (Mine && Mine->GetDuration() + .15f < Left)
	{
		Speak(Mine, Pools[ETallowKeeperLine::Capture].Volume * SpokenLift());
		UE_LOG(LogTemp, Display, TEXT("TALLOW_VOICE CaptureHold %s"), *Mine->GetName());
	}
}
