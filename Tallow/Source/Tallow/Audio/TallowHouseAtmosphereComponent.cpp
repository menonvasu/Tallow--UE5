#include "Audio/TallowHouseAtmosphereComponent.h"
#include "Audio/TallowSoundscapeComponent.h"
#include "AI/TallowCreature.h"
#include "AI/Hunter/TallowHunterDirector.h"
#include "Camera/CameraComponent.h"
#include "Chapter/TallowChapterSubsystem.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Environment/TallowDoor.h"
#include "House/TallowHouseRooms.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Player/TallowCharacter.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Tasks/TallowFuseBox.h"

namespace
{
	// The house is 15.75 m by 13 m: x 0..1575, y -1300..0 (front at y = 0), floors at -330, 0 and 390 cm.
	constexpr float HouseFarX = 1575.f, HouseBackY = -1300.f;
	const TCHAR* AtmosFolder = TEXT("/Game/FinalPass/Sound/Atmos");
	const TCHAR* DetailFolder = TEXT("/Game/FinalPass/Sound/Detail");
	// Outside layers lose their top as the chapters go: the house closes in.
	const float OutsideTopHz[4] = {6000.f, 4500.f, 3000.f, 1800.f};
	const float RoomTopHz[4] = {16000.f, 9000.f, 4000.f, 2200.f};

	enum class EDetailPlace : uint8 { Rooms, Wall, Street, Far, Sky, Door };
	struct FDetailKind { const TCHAR* Folder; const TCHAR* Name; int32 Count; float Volume; float Weights[4]; EDetailPlace Place; };
	// Each chapter has its own weather and its own kind of noise: the street is still there in I, the storm comes in
	// II, in III the house itself is doing it, and in the last chapter only the boards and the pipes are left.
	const FDetailKind DetailKinds[] = {
		{DetailFolder, TEXT("AD_Creak"), 9, .55f, {1.f, 1.2f, 1.6f, 1.6f}, EDetailPlace::Rooms},
		{TEXT("/Game/Audio/CC0"), TEXT("H_Settle"), 3, .5f, {.4f, .4f, .5f, .5f}, EDetailPlace::Rooms},
		{DetailFolder, TEXT("AD_PipeTick"), 2, .5f, {.5f, .7f, .8f, .9f}, EDetailPlace::Wall},
		{DetailFolder, TEXT("AD_PipeGurgle"), 2, .45f, {.3f, .5f, .6f, .3f}, EDetailPlace::Wall},
		{DetailFolder, TEXT("AD_CarPass"), 3, .9f, {.8f, .3f, 0.f, 0.f}, EDetailPlace::Street},
		{DetailFolder, TEXT("AD_Bell"), 1, .6f, {.25f, .2f, 0.f, 0.f}, EDetailPlace::Far},
		{DetailFolder, TEXT("AD_Dog"), 1, .5f, {.25f, .15f, 0.f, 0.f}, EDetailPlace::Far},
		{DetailFolder, TEXT("AD_Thunder"), 2, .75f, {0.f, .45f, .25f, 0.f}, EDetailPlace::Sky},
		{DetailFolder, TEXT("AD_DoorDrift"), 2, .5f, {.3f, .4f, .55f, .35f}, EDetailPlace::Door},
	};
	// The objective voices, by ETallowGuideKind from Tape: the tape machine's hiss, the battery's hum, the humming box.
	// Volume and falloff from measured loudness against the house (Tools/FinalPass/tapes/guide_levels.py, its
	// Review/guide-levels.md): by it about 2 LU under the house overall and plain in its own band, faint across the
	// room, at the edge of hearing through a wall (the tape) or gone (the hums, which the battery hunt has Mim over).
	struct FObjectiveSound { const TCHAR* Folder; const TCHAR* Name; float Volume; float Falloff; float Lift; };
	const FObjectiveSound ObjectiveSounds[] = {
		{TEXT("/Game/FinalPass/Tapes"), TEXT("AT_TapeHiss_Loop"), .391f, 2600.f, 8.f},
		{TEXT("/Game/FinalPass/Tapes"), TEXT("AT_BatteryHum_Loop"), .235f, 3000.f, 5.f},
		{AtmosFolder, TEXT("AT_HummingBox_Loop"), .241f, 3000.f, 0.f},
	};
	// Seconds between details, and how close the boards get, by chapter colour.
	const FVector2D DetailGapSeconds[4] = {{16.f, 32.f}, {13.f, 26.f}, {10.f, 20.f}, {12.f, 24.f}};
	const FVector2D DetailReach[4] = {{450.f, 1000.f}, {400.f, 900.f}, {300.f, 750.f}, {250.f, 600.f}};
}

UTallowHouseAtmosphereComponent::UTallowHouseAtmosphereComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

USoundBase* UTallowHouseAtmosphereComponent::Load(const FString& Folder, const FString& Name)
{
	const FString Path = FString::Printf(TEXT("%s/%s.%s"), *Folder, *Name, *Name);
	if (const TObjectPtr<USoundBase>* Found = Sounds.Find(Path)) return *Found;
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path);
	if (!Sound) UE_LOG(LogTemp, Warning, TEXT("TALLOW_ATMOS missing %s"), *Path);
	Sounds.Add(Path, Sound);
	return Sound;
}

USoundAttenuation* UTallowHouseAtmosphereComponent::MakeAttenuation(float Falloff, bool bOcclude)
{
	const int32 Key = FMath::RoundToInt(Falloff) * 2 + (bOcclude ? 1 : 0);
	if (const TObjectPtr<USoundAttenuation>* Found = Attenuations.Find(Key)) return *Found;
	auto* Curve = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& S = Curve->Attenuation;
	S.bAttenuate = true; S.bSpatialize = true;
	S.AttenuationShapeExtents = FVector(120.f); S.FalloffDistance = Falloff;
	S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound; S.dBAttenuationAtMax = -40.f;
	S.bAttenuateWithLPF = true; S.LPFRadiusMin = 300.f; S.LPFRadiusMax = 300.f + Falloff;
	S.LPFFrequencyAtMin = 20000.f; S.LPFFrequencyAtMax = 2500.f;
	// Through the walls the house muffles what is not in the room with her.
	S.bEnableOcclusion = bOcclude; S.OcclusionLowPassFilterFrequency = 1600.f; S.OcclusionVolumeAttenuation = .65f; S.OcclusionInterpolationTime = .4f;
	Attenuations.Add(Key, Curve);
	return Curve;
}

void UTallowHouseAtmosphereComponent::Setup(UTallowSoundscapeComponent* InMix, ATallowCharacter* InPlayer, ATallowCreature* InKeeper)
{
	Mix = InMix; Player = InPlayer; Keeper = InKeeper;
	Random.Initialize(int32(FPlatformTime::Cycles()));
	NextDetail = GetWorld()->GetTimeSeconds() + Random.FRandRange(6.f, 14.f);
	BuildLayers();
}

void UTallowHouseAtmosphereComponent::BuildLayers()
{
	if (bBuilt || !bAtmosphere) return;
	bBuilt = true;
	// Sash windows on the two lived-in floors: front and back every 3.5 m, the side walls twice per room. The window
	// voices sit just outside the glass at the ones nearest Nell; the cellar hears them through the floor.
	for (const float Z : {150.f, 540.f})
	{
		for (const float X : {150.f, 430.f, 780.f, 1130.f, 1420.f}) { Windows.Add(FVector(X, 25.f, Z)); Windows.Add(FVector(X, HouseBackY - 25.f, Z)); }
		for (const float Y : {-160.f, -490.f, -810.f, -1140.f}) { Windows.Add(FVector(-25.f, Y, Z)); Windows.Add(FVector(HouseFarX + 25.f, Y, Z)); }
	}
	auto Anchor = [](const TCHAR* Room) { const FTallowRoomDef* Def = TallowHouseRooms::Find(Room); return Def ? Def->Anchor() : FVector::ZeroVector; };
	FVector FuseBox = Anchor(TEXT("boiler"));
	if (TActorIterator<ATallowFuseBox> It(GetWorld()); It) FuseBox = It->GetActorLocation();
	auto Add = [this](const TCHAR* Sound, float Volume, std::initializer_list<float> Gains, int32 Voices, float Falloff, TArray<FVector> Fixed, bool bOutside)
	{
		FLayer L; L.Sound = Sound; L.Volume = Volume; L.Voices = Voices; L.Falloff = Falloff; L.Fixed = MoveTemp(Fixed); L.bOutside = bOutside;
		int32 I = 0; for (const float G : Gains) L.Gains[I++] = G;
		Layers.Add(MoveTemp(L));
	};
	//   sound                       volume  chapter gains I..IV         voices falloff  where                         outside
	Add(TEXT("AT_RoomTone_Loop"),      .19f, {1.f, 1.f, .9f, .8f},        0, 0.f,    {},                             false);
	Add(TEXT("AT_HouseBreath_Loop"),   .24f, {0.f, .2f, .55f, 1.f},       0, 0.f,    {},                             false);
	Add(TEXT("AT_Clock_Loop"),         .24f, {1.f, 1.f, 0.f, 0.f},        1, 1100.f, {Anchor(TEXT("hall")) + FVector(0, 0, 80)}, false);
	Add(TEXT("AT_Fridge_Loop"),        .2f,  {1.f, 1.f, .7f, 0.f},        1, 700.f,  {Anchor(TEXT("back")) + FVector(140, 0, 40)}, false);
	Add(TEXT("AT_SinkDrip_Loop"),      .15f, {.8f, .9f, 1.f, 1.f},        1, 600.f,  {Anchor(TEXT("back")) + FVector(120, -200, 40)}, false);
	Add(TEXT("AT_Heater_Loop"),        .2f,  {.7f, .8f, .9f, .6f},        1, 900.f,  {Anchor(TEXT("laundry")) + FVector(0, 0, 60)}, false);
	Add(TEXT("AT_HummingBox_Loop"),    .26f, {.4f, .6f, .85f, 1.f},       1, 1000.f, {FuseBox},                      false);
	Add(TEXT("AT_Rain_Window_Loop"),   .3f,  {1.f, 0.f, .55f, 0.f},       2, 900.f,  {},                             true);
	Add(TEXT("AT_RainHeavy_Window_Loop"), .32f, {0.f, 1.f, 0.f, 0.f},     2, 900.f,  {},                             true);
	Add(TEXT("AT_Wind_Window_Loop"),   .28f, {.4f, .75f, 1.f, .45f},      2, 900.f,  {},                             true);
	Add(TEXT("AT_Wind_Whistle_Loop"),  .2f,  {0.f, .35f, .9f, .6f},       1, 700.f,  {},                             true);
	Add(TEXT("AT_Gutter_Loop"),        .2f,  {.7f, .9f, .5f, .15f},       1, 700.f,  {FVector(700.f, HouseBackY - 60.f, 40.f), FVector(1450.f, HouseBackY - 60.f, 40.f)}, true);
	Add(TEXT("AT_City_Window_Loop"),   .45f, {1.f, .5f, 0.f, 0.f},        1, 2600.f, {FVector(780.f, 700.f, 100.f)}, true);
	for (FLayer& L : Layers)
	{
		USoundBase* Sound = Load(AtmosFolder, L.Sound);
		if (!Sound) continue;
		const bool b2D = L.Voices == 0;
		for (int32 V = 0; V < FMath::Max(L.Voices, 1); ++V)
		{
			auto* Voice = NewObject<UAudioComponent>(GetOwner());
			Voice->SetSound(Sound); Voice->bAutoActivate = false; Voice->bAutoDestroy = false; Voice->bAllowSpatialization = !b2D;
			Voice->SetUsingAbsoluteLocation(true);
			if (!b2D)
			{
				Voice->bOverrideAttenuation = false;
				Voice->AttenuationSettings = MakeAttenuation(L.Falloff, true);
				Voice->SetWorldLocation(L.Fixed.IsValidIndex(V) ? L.Fixed[V] : (L.Fixed.IsEmpty() ? Windows[V] : L.Fixed[0]));
			}
			Voice->RegisterComponent(); Voice->SetVolumeMultiplier(.001f);
			L.Index.Add(Sources.Add(Voice));
			WindowAt.Add(Voice->GetComponentLocation());
		}
	}
	for (int32 I = 0; I < Layers.Num(); ++I) if (FCString::Strcmp(Layers[I].Sound, TEXT("AT_HummingBox_Loop")) == 0) HummingBoxLayer = I;
	for (const FObjectiveSound& O : ObjectiveSounds)
	{
		auto* Voice = NewObject<UAudioComponent>(GetOwner());
		Voice->SetSound(Load(O.Folder, O.Name)); Voice->bAutoActivate = false; Voice->bAutoDestroy = false; Voice->bAllowSpatialization = true;
		Voice->SetUsingAbsoluteLocation(true); Voice->bOverrideAttenuation = false; Voice->AttenuationSettings = MakeAttenuation(O.Falloff, true);
		Voice->RegisterComponent(); Voice->SetVolumeMultiplier(.001f);
		ObjectiveVoices.Add(Voice); ObjectiveGains.Add(0.f);
	}
	UE_LOG(LogTemp, Display, TEXT("TALLOW_ATMOS built layers=%d voices=%d windows=%d objectives=%d"), Layers.Num(), Sources.Num(), Windows.Num(), ObjectiveVoices.Num());
}

void UTallowHouseAtmosphereComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	for (UAudioComponent* Voice : Sources) if (Voice) Voice->Stop();
	for (UAudioComponent* Voice : ObjectiveVoices) if (Voice) Voice->Stop();
	Super::EndPlay(Reason);
}

FVector UTallowHouseAtmosphereComponent::Ears() const
{
	if (!Player.IsValid()) return FVector::ZeroVector;
	if (const UCameraComponent* Head = Player->GetFirstPersonCameraComponent()) return Head->GetComponentLocation();
	return Player->GetActorLocation();
}

int32 UTallowHouseAtmosphereComponent::ComputeColour() const
{
	if (const UTallowChapterSubsystem* Chapters = UTallowChapterSubsystem::GetActive(this)) return FMath::Clamp(Chapters->GetChapterIndex(), 0, 3);
	// The old single route: the act stands in for the chapter.
	if (const auto* Pacing = GetWorld()->GetSubsystem<UTallowHunterDirector>())
		switch (Pacing->GetAct())
		{
		case ETallowStoryAct::FuseHunt: return 1;
		case ETallowStoryAct::HouseWakes: case ETallowStoryAct::Reel: return 2;
		case ETallowStoryAct::EndRun: return 3;
		default: return 0;
		}
	return 0;
}

void UTallowHouseAtmosphereComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bAtmosphere || !Player.IsValid() || !Mix.IsValid() || GetWorld()->IsPaused()) return;
	const int32 Now = ComputeColour();
	if (Now != Colour) { Colour = Now; UE_LOG(LogTemp, Display, TEXT("TALLOW_ATMOS colour %d"), Colour); }
	UpdateObjective(DeltaTime);
	UpdateLayers(DeltaTime);
	UpdateDetails(GetWorld()->GetTimeSeconds());
}

void UTallowHouseAtmosphereComponent::UpdateLayers(float DeltaTime)
{
	const ETallowTension Tension = Mix->GetTension();
	const bool bThreat = Tension == ETallowTension::Hunted || Tension == ETallowTension::Chase || Tension == ETallowTension::Caught;
	// The soundscape's house gain already carries the tension profile, listening and reading.
	const float House = Mix->GetHouseGain();
	const FVector Here = Ears();
	TArray<int32> Nearest;
	for (int32 I = 0; I < Windows.Num(); ++I) Nearest.Add(I);
	Nearest.Sort([&](int32 A, int32 B) { return FVector::DistSquared(Windows[A], Here) < FVector::DistSquared(Windows[B], Here); });
	for (FLayer& L : Layers)
	{
		// Rain and wind sit across Mim's palms and breath, so they step further back than the house under a threat.
		const float Target = L.Gains[Colour] * L.Volume * Level * House * (L.bOutside && bThreat ? .5f : 1.f) * L.Duck;
		L.Gain = FMath::FInterpTo(L.Gain, Target, DeltaTime, 1.2f);
		for (int32 V = 0; V < L.Index.Num(); ++V)
		{
			UAudioComponent* Voice = Sources[L.Index[V]];
			if (!Voice) continue;
			if (L.Gain <= .003f) { if (Voice->IsPlaying()) Voice->Stop(); continue; }
			if (L.Voices > 0 && L.Fixed.IsEmpty())
			{
				// Window voices glide to the nearest windows; the whistle takes the nearest one.
				const FVector Goal = Windows[Nearest[FMath::Min(V, Nearest.Num() - 1)]];
				FVector& At = WindowAt[L.Index[V]];
				At = FMath::VInterpTo(At, Goal, DeltaTime, 1.5f);
				Voice->SetWorldLocation(At);
			}
			else if (L.Fixed.Num() > 1)
			{
				// Several fixed spots (the gutters): the nearest one speaks.
				int32 Best = 0;
				for (int32 F = 1; F < L.Fixed.Num(); ++F) if (FVector::DistSquared(L.Fixed[F], Here) < FVector::DistSquared(L.Fixed[Best], Here)) Best = F;
				Voice->SetWorldLocation(L.Fixed[Best]);
			}
			if (!Voice->IsPlaying()) Voice->Play(Random.FRandRange(0.f, 20.f));
			Voice->SetVolumeMultiplier(FMath::Max(L.Gain, .001f));
			const float Top = L.bOutside ? OutsideTopHz[Colour] : L.Voices == 0 ? RoomTopHz[Colour] : 20000.f;
			Voice->SetLowPassFilterEnabled(Top < 19000.f); Voice->SetLowPassFilterFrequency(Top);
		}
	}
}

void UTallowHouseAtmosphereComponent::UpdateObjective(float DeltaTime)
{
	ETallowGuideKind Kind = ETallowGuideKind::None; const AActor* Objective = nullptr;
	if (const UTallowChapterSubsystem* Chapters = UTallowChapterSubsystem::GetActive(this)) Objective = Chapters->GetGuideObjective(Kind);
	// The box hums in the house all along. While the battery is wanted it steps back, so the hum she follows is the
	// battery's; while the box itself is wanted its own further-reaching voice takes over from it.
	if (Layers.IsValidIndex(HummingBoxLayer))
		Layers[HummingBoxLayer].Duck = Kind == ETallowGuideKind::Battery ? .35f : (Kind == ETallowGuideKind::FuseBox ? 0.f : 1.f);
	// Under a threat they step back by the square root of the house gain: less than the house does, so the way stays.
	const float House = FMath::Sqrt(FMath::Max(Mix->GetHouseGain(), 0.f));
	for (int32 I = 0; I < ObjectiveVoices.Num(); ++I)
	{
		UAudioComponent* Voice = ObjectiveVoices[I];
		if (!Voice) continue;
		const bool bCurrent = Objective && int32(Kind) == I + 1;
		float& Gain = ObjectiveGains[I];
		Gain = FMath::FInterpTo(Gain, bCurrent ? ObjectiveSounds[I].Volume * Level * House : 0.f, DeltaTime, bCurrent ? .6f : 1.2f);
		if (bCurrent) Voice->SetWorldLocation(Objective->GetActorLocation() + FVector(0, 0, ObjectiveSounds[I].Lift));
		if (!bCurrent && Gain <= .003f) { if (Voice->IsPlaying()) Voice->Stop(); continue; }
		if (!Voice->IsPlaying()) Voice->Play(Random.FRandRange(0.f, 8.f));
		Voice->SetVolumeMultiplier(FMath::Max(Gain, .001f));
	}
}

void UTallowHouseAtmosphereComponent::UpdateDetails(float Now)
{
	if (Now < NextDetail) return;
	const ETallowTension Tension = Mix->GetTension();
	// Nothing over a chase or a capture: then every sound has to be hers.
	if (Tension == ETallowTension::Chase || Tension == ETallowTension::Caught) { NextDetail = Now + Random.FRandRange(6.f, 12.f); return; }
	const FVector2D Gap = DetailGapSeconds[Colour] * (Tension == ETallowTension::Hunted ? 2.2f : 1.f);
	PlayDetail(Now);
	NextDetail = Now + Random.FRandRange(Gap.X, Gap.Y);
}

bool UTallowHouseAtmosphereComponent::PlayDetail(float Now)
{
	const FVector Here = Ears();
	const float Floor = TallowHouseRooms::FloorZ(TallowHouseRooms::FloorAt(Player->GetActorLocation().Z));
	const bool bKeeperNear = Keeper.IsValid() && FVector::Dist(Keeper->GetActorLocation(), Here) < 700.f;
	float Total = 0.f;
	for (const FDetailKind& K : DetailKinds) Total += K.Weights[Colour];
	if (Total <= 0.f || bKeeperNear) return false;
	float Roll = Random.FRand() * Total;
	const FDetailKind* Kind = &DetailKinds[0];
	for (const FDetailKind& K : DetailKinds) { Roll -= K.Weights[Colour]; if (Roll <= 0.f) { Kind = &K; break; } }
	// A different take from the last few of its kind.
	FString Name;
	for (int32 Try = 0; Try < 6; ++Try)
	{
		Name = FString::Printf(TEXT("%s_%02d"), Kind->Name, Random.RandRange(0, Kind->Count - 1));
		if (!RecentDetails.Contains(Name)) break;
	}
	USoundBase* Sound = Load(Kind->Folder, Name);
	if (!Sound) return false;
	FVector Where = FVector::ZeroVector; float Falloff = 1400.f; bool bOcclude = true, bFound = false;
	switch (Kind->Place)
	{
	case EDetailPlace::Rooms:
	{
		// A board in another room on this floor, sometimes the one above; closer as the chapters go.
		const FVector2D Reach = DetailReach[Colour];
		const int32 MyFloor = TallowHouseRooms::FloorAt(Player->GetActorLocation().Z);
		const int32 Want = MyFloor < 2 && Random.FRand() < .3f ? MyFloor + 1 : MyFloor;
		const FName Mine = TallowHouseRooms::RoomAt(Player->GetActorLocation());
		TArray<const FTallowRoomDef*> Rooms;
		for (const FTallowRoomDef& Room : TallowHouseRooms::All())
		{
			const float D = FVector::Dist2D(Room.Anchor(), Here);
			if (Room.Floor == Want && Room.Id != Mine && D >= Reach.X && D <= Reach.Y) Rooms.Add(&Room);
		}
		if (Rooms.IsEmpty()) for (const FTallowRoomDef& Room : TallowHouseRooms::All()) if (Room.Floor == Want && Room.Id != Mine) Rooms.Add(&Room);
		if (Rooms.IsEmpty()) break;
		const FTallowRoomDef* Room = Rooms[Random.RandRange(0, Rooms.Num() - 1)];
		Where = Room->Anchor() + FVector(Random.FRandRange(-140.f, 140.f), Random.FRandRange(-160.f, 160.f), -40.f);
		bFound = true; Falloff = 1300.f;
		break;
	}
	case EDetailPlace::Wall:
	{
		// Pipes run in the walls: the nearest wall in some direction, a hand's depth in.
		FCollisionQueryParams Query(SCENE_QUERY_STAT(AtmosWall), false, Player.Get());
		for (int32 Try = 0; Try < 8 && !bFound; ++Try)
		{
			const FVector Dir = FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f).Vector();
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByChannel(Hit, Here, Here + Dir * 450.f, ECC_Visibility, Query) && Hit.Distance > 80.f && !(Hit.GetActor() && Hit.GetActor()->IsA<ATallowDoor>()))
			{ Where = Hit.ImpactPoint - Hit.ImpactNormal * 10.f; Where.Z = Floor + Random.FRandRange(30.f, 200.f); bFound = true; }
		}
		Falloff = 900.f; bOcclude = false;
		break;
	}
	case EDetailPlace::Street:
		Where = FVector(Random.FRandRange(0.f, HouseFarX), 650.f, 60.f); Falloff = 3000.f; bFound = true; break;
	case EDetailPlace::Far:
		Where = FVector(Random.FRandRange(-1500.f, 3000.f), Random.FRand() < .6f ? 2200.f : HouseBackY - 2500.f, 300.f); Falloff = 5000.f; bFound = true; break;
	case EDetailPlace::Sky:
		Where = FVector(Here.X + Random.FRandRange(-2000.f, 2000.f), Here.Y + Random.FRandRange(-2000.f, 2000.f), 2600.f); Falloff = 6000.f; bFound = true; break;
	case EDetailPlace::Door:
	{
		TArray<const AActor*> Doors;
		for (TActorIterator<ATallowDoor> It(GetWorld()); It; ++It)
		{
			const float D = FVector::Dist(It->GetActorLocation(), Here);
			if (D > 350.f && D < 1200.f && FMath::Abs(It->GetActorLocation().Z - Floor) < 300.f) Doors.Add(*It);
		}
		if (Doors.IsEmpty()) break;
		Where = Doors[Random.RandRange(0, Doors.Num() - 1)]->GetActorLocation() + FVector(0, 0, 100.f); Falloff = 1300.f; bFound = true;
		break;
	}
	}
	if (!bFound) return false;
	if (Keeper.IsValid() && FVector::Dist(Keeper->GetActorLocation(), Where) < 600.f) return false;   // never where she is
	const float Duck = FMath::Clamp(Mix->GetHouseGain(), .35f, 1.f) * (Mix->GetTension() == ETallowTension::Hunted ? .6f : 1.f);
	UGameplayStatics::SpawnSoundAtLocation(this, Sound, Where, FRotator::ZeroRotator, Kind->Volume * Level * Duck, Random.FRandRange(.93f, 1.05f), 0.f,
		MakeAttenuation(Falloff, bOcclude));
	RecentDetails.Add(Name); while (RecentDetails.Num() > 4) RecentDetails.RemoveAt(0);
	++Details;
	UE_LOG(LogTemp, Display, TEXT("TALLOW_ATMOS detail %s %s ch=%d"), *Name, *Where.ToCompactString(), Colour);
	return true;
}
