#include "Audio/TallowSoundscapeComponent.h"
#include "Audio/TallowAudioPolicy.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/Presentation/TallowJournalWidget.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "NavigationSystem.h"
#include "Presentation/TallowStorySubsystem.h"
#if __has_include("AI/Hunter/TallowHunterDirector.h")
#include "AI/Hunter/TallowHunterDirector.h"
#include "AI/Hunter/TallowHunterAIController.h"
#define TALLOW_HAS_HUNTER_PACING 1
#else
#define TALLOW_HAS_HUNTER_PACING 0
#endif
#include "Audio/TallowKeeperVoiceComponent.h"
#include "Audio/TallowHouseAtmosphereComponent.h"
#include "Scare/TallowScareDirector.h"
#include "AudioDevice.h"
#include "Misc/App.h"
#include "GameFramework/PlayerController.h"

#include "AI/TallowCreature.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "Candle/TallowCandleComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Player/TallowCharacter.h"
#include "Sound/AmbientSound.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Tasks/TallowFuseBox.h"
#include "Tasks/TallowFusePickup.h"
#include "Tasks/TallowPlank.h"

namespace
{
	// The lies (MIX-PLAN.md section 4): the first five families before the power comes back, the rest after.
	struct FLieSound { const TCHAR* Name; float Volume; bool bAfterPower; bool bAbove; };
	// Red herrings come from the house itself and never imitate the Keeper's real tells (its steps, its voice):
	// RE2 never fakes Mr. X's footsteps, so the real ones stay trustworthy. Whispers are authored moments now.
	// Up 4 dB with the calm house bed (28 Oct), so a herring still clears the bed it plays over.
	const FLieSound Lies[] = {
		{TEXT("T_Knock_Wall"), .47f, false, false}, {TEXT("T_Scratch_Door_Frantic"), .4f, false, false},
		{TEXT("T_Scratch_Wall_00"), .47f, false, false}, {TEXT("T_Scratch_Wall_01"), .47f, false, false},
		{TEXT("T_Scratch_Wall_02"), .47f, false, false}, {TEXT("T_Scratch_Wall_03"), .47f, false, false},
		{TEXT("T_Kid_Cry_Distant_00"), .5f, true, false}, {TEXT("T_Kid_Cry_Distant_01"), .5f, true, false},
		{TEXT("T_Kid_Cry_Distant_02"), .5f, true, false},
		{TEXT("T_Toy_Musicbox"), .47f, false, false},
		{TEXT("T_Candle_Memory_00"), .41f, true, false},
		{TEXT("T_Candle_Memory_01"), .41f, true, false},
	};
	bool IsShriek(const TCHAR* Name) { return FCString::Strncmp(Name, TEXT("K_Shriek"), 8) == 0; }

	// The beds were mastered near -20 dB RMS, not the -26 to -30 the plan assumed (measured 25 Oct), so these trims
	// come from measured loudness, not the plan's multipliers. I found the house "a bit low" (28 Oct): the calm fan
	// comes up 3.5 dB, the cellar rumble 8 and the drip 4, so the cellar is the heaviest room. The tension profile
	// (TallowAudioPolicy) ducks the fan back to its old level once Mim is close, so her tells keep their margin;
	// the rumble is almost all below 100 Hz and masks none of them.
	// Tools/FinalPass/ambience/mix_table.py has the measurements. The final sound pass (29 Oct) gives the house its own
	// room tone, weather and rooms (UTallowHouseAtmosphereComponent), so the generic fan goes back 4 dB under them.
	float BedTrim(const FString& Name)
	{
		if (Name.StartsWith(TEXT("S_Amb_HouseBed"))) return .55f;    // the fan: always there, under the house's own rooms (29 Oct)
		if (Name.StartsWith(TEXT("S_Amb_BasementRumble"))) return .48f;
		if (Name.StartsWith(TEXT("S_Amb_Drip"))) return 1.6f;
		return 1.f;
	}
}

UTallowSoundscapeComponent::UTallowSoundscapeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

USoundBase* UTallowSoundscapeComponent::Load(const FString& Folder, const FString& Name)
{
	if (const TObjectPtr<USoundBase>* Found = Sounds.Find(Name)) return *Found;
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/%s/%s.%s"), *Folder, *Name, *Name));
	if (!Sound) UE_LOG(LogTemp, Warning, TEXT("TALLOW_AUDIO missing %s/%s"), *Folder, *Name);
	Sounds.Add(Name, Sound);
	return Sound;
}

UAudioComponent* UTallowSoundscapeComponent::MakeLoop(const FString& Folder, const FString& Name)
{
	USoundBase* Sound = Load(Folder, Name);
	if (!Sound) return nullptr;
	auto* Loop = NewObject<UAudioComponent>(GetOwner());
	Loop->SetSound(Sound); Loop->bAutoActivate = false; Loop->bAllowSpatialization = false;
	Loop->RegisterComponent(); Loop->SetVolumeMultiplier(.001f);
	return Loop;
}

void UTallowSoundscapeComponent::Setup(ATallowCharacter* InPlayer, ATallowCreature* InKeeper)
{
	Player = InPlayer; Keeper = InKeeper;
	StateMix=NewObject<USoundMix>(this);
	for(const FName Channel:{FName(TEXT("Narration")),FName(TEXT("PlayerVoice")),FName(TEXT("PlayerFoley")),FName(TEXT("Mim")),FName(TEXT("MimTells")),FName(TEXT("Distractions"))})
	{
		const FString Path=FString::Printf(TEXT("/Game/Audio/Mix/SC_Tallow_%s.SC_Tallow_%s"),*Channel.ToString(),*Channel.ToString());
		if(auto* Class=LoadObject<USoundClass>(nullptr,*Path)) MixClasses.Add(Channel,Class);
	}
	UGameplayStatics::PushSoundMixModifier(this,StateMix);
	DarkDrone = MakeLoop(TEXT("Tension"), TEXT("T_Drone_Dark_Loop"));
	BassDrone = MakeLoop(TEXT("Tension"), TEXT("T_Drone_Bass_Loop"));
	HorrorBed = MakeLoop(TEXT("Tension"), TEXT("T_Horror_Bed_Loop"));
	// The house bed, rumble and drip are placed in the map; the mix only ducks them.
	for (TActorIterator<AAmbientSound> It(GetWorld()); It; ++It)
	{
		UAudioComponent* Bed = It->GetAudioComponent();
		if (!Bed || !Bed->Sound || !Bed->Sound->GetName().StartsWith(TEXT("S_Amb_"))) continue;
		Beds.Add(Bed); BedBase.Add(Bed->VolumeMultiplier * BedTrim(Bed->Sound->GetName()));
	}
	LieAttenuation = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& Far = LieAttenuation->Attenuation;
	Far.bAttenuate = true; Far.bSpatialize = true;
	Far.AttenuationShapeExtents = FVector(200.f); Far.FalloffDistance = 2500.f;
	Far.bAttenuateWithLPF = true; Far.LPFRadiusMin = 400.f; Far.LPFRadiusMax = 2500.f;
	Far.LPFFrequencyAtMin = 20000.f; Far.LPFFrequencyAtMax = 3000.f;
	Far.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound; Far.dBAttenuationAtMax = -30.f;
	Far.bEnableOcclusion=true; Far.OcclusionLowPassFilterFrequency=1800.f; Far.OcclusionVolumeAttenuation=.65f;
	NextLie = GetWorld()->GetTimeSeconds() + FMath::FRandRange(LieGap.X, LieGap.Y);
	Atmosphere = NewObject<UTallowHouseAtmosphereComponent>(GetOwner());
	Atmosphere->RegisterComponent();
	Atmosphere->Setup(this, InPlayer, InKeeper);
}

void UTallowSoundscapeComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorld()->GetTimerManager().ClearTimer(GaspHandle);
	for(auto* Sound:{DarkDrone.Get(),BassDrone.Get(),HorrorBed.Get(),CurrentLie.Get(),WhisperVoice.Get(),PowerHum.Get()}) if(Sound) Sound->Stop();
	if(StateMix)UGameplayStatics::PopSoundMixModifier(this,StateMix);
	Super::EndPlay(Reason);
}

void UTallowSoundscapeComponent::SetPowerRestored(bool bRestored)
{
	bPowerRestored = bRestored;
	if (!bRestored) { if (PowerHum) PowerHum->Stop(); return; }
	if (!bPowerHum) return;
	if (PowerHum) { PowerHum->Play(); return; }
	// The mended circuit hums at the fuse box. A room tone you find there, gone in a few steps: the lamps stay
	// dead, so no mains hum runs through the house. Low-passed so its buzz stays off Mim's voice and steps.
	TActorIterator<ATallowFuseBox> Box(GetWorld());
	USoundBase* Hum = Load(TEXT("CC0"), TEXT("H_Power_Hum"));
	if (!Box || !Hum) return;
	auto* Near = NewObject<USoundAttenuation>(this);
	Near->Attenuation.bAttenuate = true; Near->Attenuation.bSpatialize = true;
	Near->Attenuation.AttenuationShapeExtents = FVector(100.f); Near->Attenuation.FalloffDistance = 500.f;   // the box is on an outside wall
	PowerHum = UGameplayStatics::SpawnSoundAtLocation(this, Hum, Box->GetActorLocation(), FRotator::ZeroRotator, FMath::Max(.12f * BedGain, .001f), 1.f, 0.f, Near, nullptr, false);
	if (!PowerHum) return;
	PowerHum->SetLowPassFilterEnabled(true); PowerHum->SetLowPassFilterFrequency(1200.f);
	Beds.Add(PowerHum); BedBase.Add(.12f);   // ducks with the house
}

bool UTallowSoundscapeComponent::HasDistractionPlaying() const
{
	const UTallowScareDirector* Scares=UTallowScareDirector::Get(this);
	return (CurrentLie&&CurrentLie->IsPlaying()) || (WhisperVoice&&WhisperVoice->IsPlaying()) || (Scares&&Scares->IsCuePlaying());
}

bool UTallowSoundscapeComponent::AllowsMinorScare() const
{
	if (!Player.IsValid() || bCaught || bNarrating || Player->IsHoldingStill() || HasDistractionPlaying()) return false;
	if (Tension != ETallowTension::Calm && Tension != ETallowTension::Uneasy) return false;
	// Never on top of the real thing: nothing while she is within 10 m.
	return !(bKeeperAwake && Keeper.IsValid() && FVector::Dist(Keeper->GetActorLocation(), Player->GetActorLocation()) < 1000.f);
}

bool UTallowSoundscapeComponent::PlayWhisperPass()
{
	return Player.IsValid() && Whisper(GetWorld()->GetTimeSeconds(), 0, true);
}

void UTallowSoundscapeComponent::InterruptDistractions()
{
	if(CurrentLie&&CurrentLie->IsPlaying()&&CurrentLie->GetPlayState()!=EAudioComponentPlayState::FadingOut)CurrentLie->FadeOut(.08f,0.f);
	if(WhisperVoice&&WhisperVoice->IsPlaying()&&WhisperVoice->GetPlayState()!=EAudioComponentPlayState::FadingOut)WhisperVoice->FadeOut(.08f,0.f);
}

void UTallowSoundscapeComponent::PlayPlayer(const FString& Name,float Volume)
{UGameplayStatics::PlaySound2D(this,Load(TEXT("Player"),Name),Volume,1.f,0.f,nullptr,GetOwner(),false);}

void UTallowSoundscapeComponent::RefreshReadingFocus(float Now)
{
	if(Now<NextReadingCheck)return;
	NextReadingCheck=Now+.15f;bNarrating=false;
	TArray<UUserWidget*> Readers;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Readers,UTallowJournalWidget::StaticClass(),false);
	for(auto* Reader:Readers)if(auto* Journal=Cast<UTallowJournalWidget>(Reader))bNarrating|=Journal->IsReading();
}

void UTallowSoundscapeComponent::SetKeeperAwake(bool bAwake)
{
	bKeeperAwake = bAwake;
	if (Keeper.IsValid())
		if (auto* Voice = Keeper->FindComponentByClass<UTallowKeeperVoiceComponent>()) Voice->SetAwake(bAwake);
}

void UTallowSoundscapeComponent::UpdateRoomTell(float Now)
{
	// A rare real laugh tells you it has changed rooms. The voice component owns
	// the bag, spatialization and quiet gap so this cannot overlap its dialogue.
	if (!Keeper.IsValid() || !bKeeperAwake || bCaught || Tension==ETallowTension::After) return;
	const FVector At = Keeper->GetActorLocation();
	const FIntVector Cell(FMath::FloorToInt(At.X / 520.f), FMath::FloorToInt(At.Y / 520.f), FMath::FloorToInt((At.Z + 330.f) / 330.f));
	if (Cell == KeeperCell) return;
	const bool bFirst = KeeperCell.X == MAX_int32;
	KeeperCell = Cell;
	if (bFirst || Now - LastRoomTell < RoomTellGap || Keeper->CurrentState == ETallowCreatureState::Chasing) return;
	if (auto* Voice = Keeper->FindComponentByClass<UTallowKeeperVoiceComponent>())
		if (Voice->Say(ETallowKeeperLine::Giggle)) LastRoomTell = Now;
}

namespace
{
	// Lying in wait (the heap, a hold, a watch, a covered ambush) must not be announced by the mix or a gasp.
	const ATallowHunterAIController* HunterBrainOf(const ATallowCreature* Keeper)
	{
#if TALLOW_HAS_HUNTER_PACING
		return Keeper ? Cast<ATallowHunterAIController>(Keeper->GetController()) : nullptr;
#else
		return nullptr;
#endif
	}
}

ETallowTension UTallowSoundscapeComponent::ComputeTension(float Now) const
{
	if (bCaught) return ETallowTension::Caught;
	if (!Player.IsValid() || !Keeper.IsValid()) return ETallowTension::Calm;
	if (!bKeeperAwake) return ETallowTension::Calm;
	const ETallowCreatureState State = Keeper->CurrentState;
	float Distance = FVector::Dist(Keeper->GetActorLocation(), Player->GetActorLocation());
	const ATallowHunterAIController* Brain = HunterBrainOf(Keeper.Get());
	const bool bLurking = Brain && Brain->IsLurking();
	const bool bSearching = !bLurking && (State == ETallowCreatureState::SearchingCorner || State == ETallowCreatureState::SearchHigh
		|| State == ETallowCreatureState::SearchLow);
	if (State == ETallowCreatureState::Chasing || Player->KeeperSight > .5f) return ETallowTension::Chase;
#if TALLOW_HAS_HUNTER_PACING
	if (const auto* Pacing = GetWorld()->GetSubsystem<UTallowHunterDirector>())
	{
		Distance = FMath::Max(Distance,Pacing->GetWalkingDistance());
		// A real close search overrides the breather. A room away, let the retreat be heard as relief.
		if (Pacing->GetPhase()==ETallowTensionPhase::Relax && Distance>600.f && Player->KeeperSight<.1f)
			return ETallowTension::After;
	}
#endif
	// Still and waiting it is unease at most until she is almost on it.
	if (bLurking && Distance >= 450.f) return Distance < 1600.f ? ETallowTension::Uneasy : ETallowTension::Calm;
	// Searching another floor is a distant tell, not an unbroken close-threat drone.
	if ((bSearching && Distance < 1400.f) || Distance < 900.f) return ETallowTension::Hunted;
	if ((State == ETallowCreatureState::Investigating && Distance < 1800.f) || Distance < 1600.f) return ETallowTension::Uneasy;
	return ETallowTension::Calm;
}

void UTallowSoundscapeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Player.IsValid()) return;
	const float Now = GetWorld()->GetTimeSeconds();
	FollowEarsWithoutRenderer(DeltaTime);
	const ETallowTension Level = ComputeTension(Now);
	const bool bThreat = Level==ETallowTension::Hunted || Level==ETallowTension::Chase || Level==ETallowTension::Caught;
	if (bThreat) { bPendingRelease=true; SafeSince=-1.f; }
	else if (bPendingRelease)
	{
		if (SafeSince<0.f) SafeSince=Now;
		if (Now-SafeSince>=2.f)
		{
			bPendingRelease=false;
			AfterUntil=Now+ReliefSeconds;NoLiesUntil=FMath::Max(NoLiesUntil,AfterUntil+8.f);
			if(Now-LastBreath>4.f){TallowNellVoice::NoteBreath(TEXT("P_Breath_Out relief"));PlayPlayer(TEXT("P_Breath_Out"),.16f);LastBreath=Now;}
		}
	}
	if(Level==ETallowTension::After) NoLiesUntil=FMath::Max(NoLiesUntil,Now+8.f);
	LastLevel=Level;
	Tension=(!bThreat && Now<AfterUntil)?ETallowTension::After:Level;
	RefreshReadingFocus(Now);
	if(Tension==ETallowTension::After || Tension==ETallowTension::Hunted || Tension==ETallowTension::Chase || Tension==ETallowTension::Caught || bNarrating || Player->IsHoldingStill())
	{
		InterruptDistractions();
	}
	if(Keeper.IsValid())if(auto* Voice=Keeper->FindComponentByClass<UTallowKeeperVoiceComponent>())
		Voice->SetRecoveryQuiet(Tension==ETallowTension::After);
	if (Tension == ETallowTension::Calm) bSeenThisEncounter = false;
	UpdateMix(DeltaTime);
	UpdateLies(Now);
	UpdateVoice(Now);
	UpdateRoomTell(Now);
	UpdateWhispers(Now);
	// Her gasp a beat after its call: the third part of being seen, after the call and the candle.
	if (Player->SeenCallAt > HeardSeenCall + .01f)
	{
		HeardSeenCall = Player->SeenCallAt;
		GetWorld()->GetTimerManager().SetTimer(GaspHandle, FTimerDelegate::CreateWeakLambda(this, [this] { Gasp(GetWorld()->GetTimeSeconds()); }), .15f, false);
	}
}

void UTallowSoundscapeComponent::FollowEarsWithoutRenderer(float DeltaTime)
{
	// The engine moves the listener when the viewport draws. Without a renderer (-nullrhi tests and offline
	// mixdowns) nothing draws, so the ears would stay at the world origin; put them where Nell's are.
	if (FApp::CanEverRender()) return;
	APlayerController* Controller = Player->GetController<APlayerController>();
	FAudioDeviceHandle Device = GetWorld()->GetAudioDevice();
	if (!Controller || !Device) return;
	FVector At, Front, Right;
	Controller->GetAudioListenerPosition(At, Front, Right);
	FTransform Ears(FRotationMatrix::MakeFromXY(Front, Right));
	Ears.SetTranslation(At); Ears.NormalizeRotation();
	Device->SetListener(GetWorld(), 0, Ears, DeltaTime);
	if (GFrameCounter % 150 == 0)
	{
		FTransform Heard; Device->GetListenerTransform(0, Heard);
		UE_LOG(LogTemp, Display, TEXT("TALLOW_AUDIO ears %s listener %s"), *At.ToCompactString(), *Heard.GetLocation().ToCompactString());
	}
}

bool UTallowSoundscapeComponent::Whisper(float Now, int32 EndSide, bool bDirected)
{
	// Head-locked: a dry voice that crosses from one ear to the other over about three seconds, whichever way she
	// faces (Hellblade's voices circle a dummy head). Rare and placed: at most one every 4 minutes, five a run.
	// The scare director's pass keeps its own cooldown in place of those two limits.
	if (!bLies || bNarrating || Player->IsHoldingStill() || (CurrentLie&&CurrentLie->IsPlaying()) || bCaught || Now<NoLiesUntil || Tension==ETallowTension::After) return false;
	if (!bDirected && (Now - LastWhisper < 240.f || WhispersPlayed >= 5)) return false;
	if (bDirected && WhisperVoice && WhisperVoice->IsPlaying()) return false;
#if TALLOW_HAS_HUNTER_PACING
	// Whispers keep their own authored triggers; the arc only keeps them out of protected
	// moments (the screening, reading) and off the final run, where every cue is the truth.
	if (const auto* Pacing = GetWorld()->GetSubsystem<UTallowHunterDirector>())
		if (Pacing->IsProtected() || Pacing->GetAct() == ETallowStoryAct::EndRun) return false;
#endif
	if (Tension != ETallowTension::Calm && Tension != ETallowTension::Uneasy && Tension != ETallowTension::After) return false;
	if (Keeper.IsValid() && FVector::Dist(Keeper->GetActorLocation(), Player->GetActorLocation()) < 1000.f) return false;
	USoundBase* Voice = Load(TEXT("Tension"), FString::Printf(TEXT("T_Whisper_%02d"), (WhispersPlayed + FMath::RandRange(0, 3)) % 4));
	UCameraComponent* Head = Player->GetFirstPersonCameraComponent();
	if (!Voice || !Head) return false;
	if (!WhisperAttenuation)
	{
		WhisperAttenuation = NewObject<USoundAttenuation>(this);
		WhisperAttenuation->Attenuation.bAttenuate = false; WhisperAttenuation->Attenuation.bSpatialize = true;
		WhisperAttenuation->Attenuation.bEnableReverbSend = false;   // dry: inside her head, not in the room
	}
	if (WhisperVoice) WhisperVoice->DestroyComponent();
	WhisperVoice = UGameplayStatics::SpawnSoundAttached(Voice, Head, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset,
		true, .3f, 1.f, 0.f, WhisperAttenuation);
	WhisperFrom = EndSide != 0 ? -EndSide : (WhisperFrom > 0 ? -1 : 1);   // alternate the starting ear
	WhisperStarted = Now; LastWhisper = Now; ++WhispersPlayed;
	UE_LOG(LogTemp, Display, TEXT("TALLOW_AUDIO whisper %d from %s"), WhispersPlayed, WhisperFrom > 0 ? TEXT("right") : TEXT("left"));
	return true;
}

void UTallowSoundscapeComponent::UpdateWhispers(float Now)
{
	// Sweep the playing whisper across the head.
	if (WhisperVoice && WhisperVoice->IsPlaying())
	{
		const float T = FMath::Clamp((Now - WhisperStarted) / 3.f, 0.f, 1.f);
		const float Side = FMath::Lerp(float(WhisperFrom), float(-WhisperFrom), FMath::SmoothStep(0.f, 1.f, T));
		WhisperVoice->SetRelativeLocation(FVector(25.f * FMath::Sin(T * PI), 55.f * Side, 5.f));
	}
	// Progress resets the "stuck" clock: anything picked up or carried, the power, fuses taken.
	int32 Signature = static_cast<int32>(Player->GetCarriedItem()) + (bPowerRestored ? 100 : 0);
	for (TActorIterator<ATallowFusePickup> It(GetWorld()); It; ++It) Signature += It->IsHidden() ? 1000 : 0;
	if (Signature != ProgressSignature) { ProgressSignature = Signature; LastProgressAt = Now; }
	// 1. The first time the house is truly dark around her.
	const UTallowCandleComponent* Candle = Player->GetCandleComponent();
	const bool bDark = Candle && Candle->IsAcquired() && Candle->GetCandleState() == ECandleState::Out;
	if (bDark && !bWhisperedDark && Now - DarkSince > 4.f && DarkSince > 0.f) { bWhisperedDark = true; Whisper(Now, 0); }
	if (!bDark) DarkSince = -1.f; else if (DarkSince < 0.f) DarkSince = Now;
	// 2. Under a bed, a few seconds after it has gone.
	const float Distance = Keeper.IsValid() ? FVector::Dist(Keeper->GetActorLocation(), Player->GetActorLocation()) : 1e7f;
	if (Player->IsHiddenInSpot())
	{
		if (Distance < 600.f) KeeperWasNearHidden = Now;
		else if (KeeperWasNearHidden > 0.f && Now - KeeperWasNearHidden > 4.f) { KeeperWasNearHidden = -1.f; Whisper(Now, 0); }
	}
	// 3. The first time she climbs into the toy room (upper floor, west front).
	const FVector At = Player->GetActorLocation();
	if (!bWhisperedToyRoom && At.Z > 390.f && At.X < 585.f && At.Y > -650.f) { bWhisperedToyRoom = true; Whisper(Now, 0); }
	// 4. Stuck for two minutes with it far away: the whisper ends in the ear facing the way on.
	if (Now - LastProgressAt > 120.f && Distance > 1500.f)
	{
		const FVector Goal = NextObjective();
		int32 Side = 0;
		if (!Goal.IsZero())
			Side = FVector::DotProduct((Goal - At).GetSafeNormal2D(), Player->GetFirstPersonCameraComponent()->GetRightVector()) >= 0.f ? 1 : -1;
		LastProgressAt = Now;
		Whisper(Now, Side);
	}
}

void UTallowSoundscapeComponent::UpdateMix(float DeltaTime)
{
	const auto Profile=FTallowAudioProfile::For(Tension,Player->IsHoldingStill(),bNarrating,bPowerRestored);
	const float Bed=Profile.Bed, Dark=bDrones?Profile.Dark:0.f, Bass=bDrones?Profile.Bass:0.f;
	const float Horror=bDrones?Profile.Horror:0.f;
	const float Seconds=Tension==ETallowTension::Caught?.05f:3.f;
	if(Profile.Narration<=0.f)
	{
		TArray<UUserWidget*> Readers;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Readers,UTallowJournalWidget::StaticClass(),false);
		for(auto* Reader:Readers)if(auto* Journal=Cast<UTallowJournalWidget>(Reader))if(Journal->IsOpen())Journal->PauseReadingForImportantSound(1.25f);
	}
	auto Channel=[this,Seconds](FName Key,float Gain)
	{
		if(const auto* Previous=LastClassGains.Find(Key))if(FMath::IsNearlyEqual(*Previous,Gain,.001f))return;
		const float Fade=Key==TEXT("Narration")?.1f:Seconds;
		if(auto* Class=MixClasses.Find(Key))UGameplayStatics::SetSoundMixClassOverride(this,StateMix,*Class,Gain,1.f,Fade,true);
		LastClassGains.Add(Key,Gain);
	};
	Channel(TEXT("PlayerVoice"),Profile.PlayerVoice);Channel(TEXT("PlayerFoley"),Profile.PlayerFoley);
	Channel(TEXT("Mim"),Profile.Mim);Channel(TEXT("MimTells"),Profile.Mim);
	// Zero bus gain can virtualize and finish a one-shot. Pause the reader itself
	// above, keeping its source alive so its position survives the interruption.
	Channel(TEXT("Narration"),1.f);Channel(TEXT("Distractions"),Profile.Distractions);

	auto Approach = [DeltaTime](float& Value, float Target, float FadeSeconds) { Value = FMath::FInterpConstantTo(Value, Target, DeltaTime, 1.f / FadeSeconds); };
	UpdateHush(DeltaTime);
	Approach(BedGain, Bed, Tension == ETallowTension::Caught ? .05f : Tension == ETallowTension::After ? 6.f : Player->IsHoldingStill() ? .4f : 2.f);
	Approach(DarkGain, Dark, Seconds);
	Approach(BassGain, Bass, Seconds);
	Approach(HorrorGain, Horror, Tension == ETallowTension::Caught ? .05f : 8.f);
	for (int32 I = 0; I < Beds.Num(); ++I)
		if (Beds[I]) Beds[I]->SetVolumeMultiplier(FMath::Max(BedBase[I] * BedGain * HushGain, .001f));
	auto Drive = [](UAudioComponent* Loop, float Gain)
	{
		if (!Loop) return;
		if (Gain <= .002f) { if(Loop->IsPlaying())Loop->Stop(); return; }
		if (!Loop->IsPlaying()) Loop->Play();
		Loop->SetVolumeMultiplier(FMath::Max(Gain, .001f));
	};
	Drive(DarkDrone, DarkGain * HushGain); Drive(BassDrone, BassGain * HushGain); Drive(HorrorBed, HorrorGain * HushGain);
}

void UTallowSoundscapeComponent::UpdateHush(float DeltaTime)
{
	// The house goes quiet around her: when she is about to come into Nell's room (a few metres off and closing, or
	// right on top of her) the bed, atmosphere and drones sink to near silence, so her own sounds are all there is.
	const float Now = GetWorld()->GetTimeSeconds();
	const bool bFree = Keeper.IsValid() && bKeeperAwake && Player.IsValid() && Tension != ETallowTension::Caught && Tension != ETallowTension::Chase && Tension != ETallowTension::After;
	if (bFree)
	{
		const float Distance = FVector::Dist(Keeper->GetActorLocation(), Player->GetActorLocation());
		const float Closing = LastKeeperDistance > 0.f && DeltaTime > KINDA_SMALL_NUMBER ? (LastKeeperDistance - Distance) / DeltaTime : 0.f;
		LastKeeperDistance = Distance;
		const bool bSameStorey = FMath::Abs(Keeper->GetActorLocation().Z - Player->GetActorLocation().Z) < 220.f;
		const bool bEntering = bSameStorey && (Distance < HushNearCm * .5f || (Distance < HushNearCm && Closing > 25.f));
		// Only about a third of her entries hush the house, and never a creeping one: always going quiet would itself be a tell.
		if (bEntering && !bWasEntering)
		{
			const auto* Voice = Keeper->FindComponentByClass<UTallowKeeperVoiceComponent>();
			bHushThisEntry = !(Voice && Voice->IsCreeping()) && FMath::FRand() < HushChance;
		}
		bWasEntering = bEntering;
		if (bEntering && bHushThisEntry) HushUntil = Now + HushHoldSeconds;
	}
	else { LastKeeperDistance = -1.f; bWasEntering = false; }
	const bool bOn = Now < HushUntil && bFree;
	HushGain = FMath::FInterpConstantTo(HushGain, bOn ? HushDepth : 1.f, DeltaTime, bOn ? (1.f - HushDepth) / 1.2f : (1.f - HushDepth) / 3.f);
}

void UTallowSoundscapeComponent::UpdateLies(float Now)
{
	// Never while it is close, chasing or has you: then every sound has to be the truth.
	if (Tension != ETallowTension::Calm && Tension != ETallowTension::Uneasy)
	{
		NextLie = FMath::Max(NextLie, Now + 10.f);
		return;
	}
#if TALLOW_HAS_HUNTER_PACING
	// The arc owns the herrings too: none before Mim wakes, none inside earned relief or a
	// protected moment, none on the final run. Random lies only ever deepen a build.
	if (const auto* Pacing = GetWorld()->GetSubsystem<UTallowHunterDirector>())
		if (!Pacing->AllowsAmbientLie()) { NextLie = FMath::Max(NextLie, Now + 5.f); return; }
#endif
	// In the chapter loop the scare director owns the herrings too, so every minor scare shares one clock.
	if (const UTallowScareDirector* Scares = UTallowScareDirector::Get(this); Scares && Scares->OwnsScheduling()) return;
	if (!bLies || bNarrating || Player->IsHoldingStill() || (WhisperVoice&&WhisperVoice->IsPlaying()) || Now < NextLie || Now < NoLiesUntil || (CurrentLie && CurrentLie->IsPlaying())) return;
	// Never on top of the real thing: no herring while it is within 10 m.
	if (Keeper.IsValid() && FVector::Dist(Keeper->GetActorLocation(), Player->GetActorLocation()) < 1000.f) { NextLie = Now + 10.f; return; }
	RequestDistraction();
	NextLie = Now + FMath::FRandRange(LieGap.X, LieGap.Y) * LieGapScale();
}

float UTallowSoundscapeComponent::LieGapScale() const
{
#if TALLOW_HAS_HUNTER_PACING
	if (GetWorld())
		if (const auto* Pacing = GetWorld()->GetSubsystem<UTallowHunterDirector>()) return Pacing->GetLieGapScale();
#endif
	return 1.f;
}

bool UTallowSoundscapeComponent::RequestDistraction()
{
	if(!Player.IsValid() || !bLies || bCaught || bNarrating || Player->IsHoldingStill() || HasDistractionPlaying())return false;
	if(!FTallowAudioProfile::For(Tension,false,false,bPowerRestored).bAllowDistractions)return false;
	const float Now=GetWorld()->GetTimeSeconds();
	if(Now<NoLiesUntil || (Keeper.IsValid()&&FVector::Dist(Keeper->GetActorLocation(),Player->GetActorLocation())<1000.f))return false;
	if(!PlayLie(Now))return false;
	NoLiesUntil=Now+10.f;NextLie=Now+FMath::FRandRange(LieGap.X,LieGap.Y)*LieGapScale();
	return true;
}

bool UTallowSoundscapeComponent::PlayLie(float Now)
{
	const FVector Here = Player->GetActorLocation();
	auto* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if(!Navigation)return false;
	TArray<const FLieSound*> Pool;
	for (const FLieSound& Lie : Lies)
	{
		if (Lie.bAfterPower && !bPowerRestored) continue;
		if (RecentLies.Contains(Lie.Name)) continue;                     // nothing again until three others have played
		if (IsShriek(Lie.Name) && Now - LastShriek < 180.f) continue;   // the strongest, at most one every three minutes
		if (Lie.bAbove && Here.Z > 300.f) continue;                      // nothing above the top floor
		Pool.Add(&Lie);
	}
	if (Pool.IsEmpty()) return false;
	const FLieSound& Lie = *Pool[FMath::RandRange(0, Pool.Num() - 1)];
	USoundBase* Sound = Load(TEXT("Tension"), Lie.Name);
	if (!Sound) return false;
	const FVector Real = Keeper.IsValid() ? Keeper->GetActorLocation() : FVector(1e7f);
	FVector Where = FVector::ZeroVector;
	bool bFound = false, bFair = false;
	if (FMath::FRand() < FairLieChance)
	{
		// A fair one comes from near the next thing you need.
		const FVector Goal = NextObjective();
		const float Away = FVector::Dist2D(Goal, Here);
		if (!Goal.IsZero() && Away > 400.f && Away < 2500.f && FVector::Dist(Goal, Real) > 600.f)
		{
			FNavLocation Floor;
			if(Navigation->ProjectPointToNavigation(Goal,Floor,FVector(200,200,400)))
			{Where=Floor.Location+FVector(0,0,60);bFound=bFair=FVector::Dist(Where,Real)>600.f;}
		}
	}
	// Otherwise from behind you, 8 to 15 m off, and never near where it really is.
	const float Behind = Player->GetControlRotation().Yaw + 180.f;
	for (int32 Try = 0; !bFound && Try < 10; ++Try)
	{
		const float Yaw = Behind + FMath::FRandRange(-75.f, 75.f) * (Try < 6 ? 1.f : 2.4f);
		const FVector Candidate = Here + FRotator(0.f, Yaw, 0.f).Vector() * FMath::FRandRange(800.f, 1500.f);
		FNavLocation Floor;
		if(!Navigation->ProjectPointToNavigation(Candidate,Floor,FVector(200,200,250)))continue;
		const FVector Placed=Floor.Location+FVector(0,0,60);
		if (FVector::Dist(Placed, Real) > 600.f && FVector::Dist2D(Placed,Here)>650.f && FMath::Abs(Placed.Z-Here.Z)<300.f)
		{Where=Placed;bFound=true;}
	}
	if (!bFound) return false;
	// The navigable house supplies the location; old fixed shell dimensions do not.
	if (Lie.bAbove) Where.Z = Here.Z + 330.f;
	CurrentLie = UGameplayStatics::SpawnSoundAtLocation(this, Sound, Where, FRotator::ZeroRotator, Lie.Volume, FMath::FRandRange(.94f, 1.04f), 0.f, LieAttenuation);
	RecentLies.Add(Lie.Name);
	while (RecentLies.Num() > 3) RecentLies.RemoveAt(0);
	if (IsShriek(Lie.Name)) LastShriek = Now;
	UE_LOG(LogTemp, Display, TEXT("TALLOW_AUDIO lie %s %s fair=%d"), Lie.Name, *Where.ToCompactString(), bFair ? 1 : 0);
	return true;
}

FVector UTallowSoundscapeComponent::NextObjective() const
{
	const FVector Here = Player->GetActorLocation();
	FVector Best = FVector::ZeroVector;
	float BestDistance = TNumericLimits<float>::Max();
	auto Consider = [&](const AActor* Actor)
	{
		if (!Actor || Actor->IsHidden()) return;
		const float Distance = FVector::Dist(Actor->GetActorLocation(), Here);
		if (Distance < BestDistance) { BestDistance = Distance; Best = Actor->GetActorLocation(); }
	};
	if (!bPowerRestored)
	{
		for (TActorIterator<ATallowFusePickup> It(GetWorld()); It; ++It) Consider(*It);
		if (Best.IsZero()) for (TActorIterator<ATallowFuseBox> It(GetWorld()); It; ++It) Consider(*It);
	}
	else for (TActorIterator<ATallowPlank> It(GetWorld()); It; ++It) Consider(*It);
	return Best;
}

bool UTallowSoundscapeComponent::CanSeeKeeper() const
{
	if (!Keeper.IsValid() || !Player.IsValid()) return false;
	const UCameraComponent* Camera = Player->GetFirstPersonCameraComponent();
	if (!Camera) return false;
	const FVector Eye = Camera->GetComponentLocation();
	const FVector Body = Keeper->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	const FVector To = Body - Eye;
	const float Distance = To.Size();
	const UTallowCandleComponent* Candle = Player->GetCandleComponent();
	const bool bLit = Candle && Candle->IsAcquired() && Candle->GetCandleState() != ECandleState::Out;
	// In the dark you only make it out up close; the house lights let you see it down a hall.
	const float Range = bLit ? 1100.f : 450.f;
	if (Distance > Range || FVector::DotProduct(To / FMath::Max(Distance, 1.f), Camera->GetForwardVector()) < .8f) return false;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SoundscapeSight), false, Player.Get());
	Query.AddIgnoredActor(Keeper.Get());
	return !GetWorld()->LineTraceTestByChannel(Eye, Body, ECC_Visibility, Query);
}

void UTallowSoundscapeComponent::Gasp(float Now)
{
	if (bCaught || Now - LastGasp < 6.f) return;
	int32 Index = FMath::RandRange(0, 4);
	if (Index == LastGaspIndex) Index = (Index + 1) % 5;
	LastGaspIndex = Index; LastGasp = Now;
	// Her gasps include the whimper and the alarm cry: voiced reactions, off with the rest of her noises (30 Oct).
	const FString Name = FString::Printf(TEXT("P_Gasp_%02d"), Index);
	if (TallowNellVoice::AllowVocalFx(*Name)) PlayPlayer(Name, .22f);
}

void UTallowSoundscapeComponent::UpdateVoice(float Now)
{
	if (bCaught) return;
	// Holding her breath (the intake, the strain, the exhale) is voiced by UTallowBreathFeedbackComponent.
	// The first time it comes into view in each encounter.
	if (bKeeperAwake && !bSeenThisEncounter && Now >= NextSightCheck)
	{
		NextSightCheck = Now + .2f;
		// Not at a still heap (the gasp would name it): it comes the first time she sees it move.
		const ATallowHunterAIController* Brain = HunterBrainOf(Keeper.Get());
		if (CanSeeKeeper() && !(Brain && Brain->IsDisguised())) { bSeenThisEncounter = true; Gasp(Now); }
	}
}

void UTallowSoundscapeComponent::NotifyCandleOut()
{
	if (Tension == ETallowTension::Hunted || Tension == ETallowTension::Chase) Gasp(GetWorld()->GetTimeSeconds());
}

void UTallowSoundscapeComponent::PlayEffort()
{
	if (bCaught) return;
	const FString Name = FString::Printf(TEXT("P_Effort_%02d"), FMath::RandRange(0, 2));
	if (TallowNellVoice::AllowVocalFx(*Name)) PlayPlayer(Name, .16f);
}

void UTallowSoundscapeComponent::NotifyCaught()
{
	bCaught = true; Tension = ETallowTension::Caught;
	GetWorld()->GetTimerManager().ClearTimer(GaspHandle);
	InterruptDistractions();
	UpdateMix(.15f);
}

void UTallowSoundscapeComponent::NotifyReset()
{
	const float Now = GetWorld()->GetTimeSeconds();
	GetWorld()->GetTimerManager().ClearTimer(GaspHandle);
	InterruptDistractions();
	if (Keeper.IsValid())
		if (auto* Voice = Keeper->FindComponentByClass<UTallowKeeperVoiceComponent>()) Voice->ResetVoice();
	if (WhisperVoice) WhisperVoice->Stop();
	bCaught = false; bSeenThisEncounter = false; bWasHolding = false;
	AfterUntil = Now + 6.f; NoLiesUntil = Now + 20.f;
	SafeSince=-1.f;bPendingRelease=false;Tension=ETallowTension::After;
	NextLie = Now + FMath::FRandRange(LieGap.X, LieGap.Y);
	LastLevel = ETallowTension::Calm;
	LastProgressAt = Now;
}
