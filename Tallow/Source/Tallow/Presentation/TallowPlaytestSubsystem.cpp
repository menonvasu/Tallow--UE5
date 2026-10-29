#include "Presentation/TallowPlaytestSubsystem.h"
#include "Presentation/TallowPlayDirector.h"
#include "AI/TallowCreature.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Environment/TallowCandlePickup.h"
#include "Components/LocalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
TAutoConsoleVariable<int32> BabyMode(TEXT("tallow.PlaytestBaby"), 0,
	TEXT("Ty is a baby: 1 enables playtest room lights and the live Mim note; 0 restores normal lighting."));
bool bReadCommandLine = false;

FString ReadIntent(AController* Controller)
{
	if (!Controller) return {};
	// The UI also builds with the original creature. Read the hunter's existing
	// pure getter without taking a dependency on its independently maintained classes.
	UFunction* Function = Controller->FindFunction(TEXT("GetIntent"));
	if (!Function || !Function->HasAllFunctionFlags(FUNC_BlueprintPure | FUNC_Const)) return {};
	FStructOnScope Parameters(Function);
	Controller->ProcessEvent(Function, Parameters.GetStructMemory());
	if (const auto* Property = CastField<FEnumProperty>(Function->GetReturnProperty()))
		return Property->GetEnum()->GetNameStringByValue(Property->GetUnderlyingProperty()->GetSignedIntPropertyValue(
			Property->ContainerPtrToValuePtr<void>(Parameters.GetStructMemory())));
	if (const auto* Property = CastField<FByteProperty>(Function->GetReturnProperty()); Property && Property->Enum)
		return Property->Enum->GetNameStringByValue(Property->GetPropertyValue_InContainer(Parameters.GetStructMemory()));
	return {};
}
}

void UTallowPlaytestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!bReadCommandLine)
	{
		bReadCommandLine = true;
		if (FParse::Param(FCommandLine::Get(), TEXT("TyIsABaby"))) SetBabyModeEnabled(true);
	}
}

void UTallowPlaytestSubsystem::Deinitialize()
{
	Restore();
	Super::Deinitialize();
}

bool UTallowPlaytestSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game || Type == EWorldType::PIE;
}

TStatId UTallowPlaytestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTallowPlaytestSubsystem, STATGROUP_Tickables);
}

bool UTallowPlaytestSubsystem::IsBabyModeEnabled() { return BabyMode.GetValueOnGameThread() != 0; }
void UTallowPlaytestSubsystem::SetBabyModeEnabled(bool bEnabled) { BabyMode.AsVariable()->Set(bEnabled ? 1 : 0, ECVF_SetByConsole); }

FString UTallowPlaytestSubsystem::DescribeLocation(FVector P)
{
	if (P.X < -40.f || P.X > 1640.f || P.Y < -1360.f || P.Y > 60.f)
		return TEXT("outside the house");
	const int32 Floor = P.Z < 0.f ? 0 : P.Z < 390.f ? 1 : 2;
	const TCHAR* Floors[] = {TEXT("cellar"), TEXT("ground floor"), TEXT("upstairs")};
	const TCHAR* Rooms[3][5] = {
		{TEXT("chair room"), TEXT("tool room"), TEXT("hall"), TEXT("boiler room"), TEXT("back stairs")},
		{TEXT("sitting room"), TEXT("dining room"), TEXT("hall"), TEXT("study"), TEXT("kitchen")},
		{TEXT("toy room"), TEXT("collection room"), TEXT("hall"), TEXT("bedroom"), TEXT("nursery")}
	};
	const int32 Room = P.X < 585.f ? (P.Y > -690.f ? 0 : 1) : P.X < 975.f ? 2 : P.Y > -650.f ? 3 : 4;
	return FString::Printf(TEXT("%s / %s"), Floors[Floor], Rooms[Floor][Room]);
}

void UTallowPlaytestSubsystem::Tick(float DeltaTime)
{
	if (!IsBabyModeEnabled()) { if (bApplied) Restore(); return; }
	// The house's normal darkness pass finishes at .35s. Capture its final state,
	// so switching this option off returns exactly to the ordinary candle-lit run.
	if (GetWorld()->GetTimeSeconds() < .6f) return;
	if (!bApplied)
	{
		bool bGameplay = false;
		for (TActorIterator<ATallowPlayDirector> It(GetWorld()); It; ++It)
			if (It->Scene == ETallowPlayScene::Game) { bGameplay = true; break; }
		if (!bGameplay) return;
		if (!UGameplayStatics::GetActorOfClass(this, ATallowCreature::StaticClass())) return;
		Apply();
	}
	RefreshAge += DeltaTime;
	if (RefreshAge >= .25f) { RefreshAge = 0.f; UpdateNote(); }
}

void UTallowPlaytestSubsystem::Apply()
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsA<APawn>() || It->IsA<ATallowPlayDirector>() || It->IsA<ATallowCandlePickup>()) continue;
		for (auto* Light : TInlineComponentArray<ULocalLightComponent*>(*It))
		{
			// Candle/scare practicals already animate their own small flame. The
			// room fills provide visibility without turning those into floodlights.
			if (Light->GetFName() == TEXT("Practical")) continue;
			FLightBefore Saved;
			Saved.Light = Light; Saved.Intensity = Light->Intensity; Saved.Radius = Light->AttenuationRadius;
			Saved.Indirect = Light->IndirectLightingIntensity; Saved.Units = uint8(Light->IntensityUnits);
			Saved.Mobility = uint8(Light->Mobility); Saved.bVisible = Light->IsVisible(); Saved.bHidden = Light->bHiddenInGame;
			Saved.bCastShadows = Light->CastShadows;
			Before.Add(Saved);
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetIntensityUnits(ELightUnits::Lumens); Light->SetIntensity(FMath::Max(450.f, Saved.Intensity));
			Light->SetAttenuationRadius(FMath::Max(650.f, Saved.Radius)); Light->SetIndirectLightingIntensity(0.f);
			Light->SetCastShadows(false);
			Light->SetVisibility(true); Light->SetHiddenInGame(false);
		}
	}
	// Separate, shadow-free fill keeps every room readable even where the abandoned
	// house has no working fixture. It never changes a player's candle or capture light.
	const FVector2D Centres[] = {{290,-345},{290,-1000},{780,-235},{780,-700},{780,-1145},{1280,-325},{1280,-990}};
	for (float Floor : {-330.f, 0.f, 390.f}) for (const FVector2D& Centre : Centres)
	{
		FActorSpawnParameters Parameters; Parameters.ObjectFlags |= RF_Transient;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Actor = GetWorld()->SpawnActor<APointLight>(FVector(Centre, Floor + 210.f), FRotator::ZeroRotator, Parameters);
		if (!Actor) continue;
		Actor->Tags.Add(TEXT("Tallow.Playtest.Light"));
		auto* Light = Actor->PointLightComponent.Get();
		Light->SetMobility(EComponentMobility::Movable); Light->SetIntensityUnits(ELightUnits::Lumens);
		Light->SetIntensity(600.f); Light->SetAttenuationRadius(750.f); Light->SetCastShadows(false);
		Light->SetIndirectLightingIntensity(0.f); Light->SetVolumetricScatteringIntensity(0.f);
		Light->SetLightColor(FLinearColor(1.f, .88f, .73f));
		FillLights.Add(Actor);
	}
	bApplied = true;
	UpdateNote();
}

void UTallowPlaytestSubsystem::Restore()
{
	for (auto& Saved : Before) if (auto* Light = Saved.Light.Get())
	{
		Light->SetIntensityUnits(ELightUnits(Saved.Units)); Light->SetIntensity(Saved.Intensity);
		Light->SetAttenuationRadius(Saved.Radius); Light->SetIndirectLightingIntensity(Saved.Indirect);
		Light->SetVisibility(Saved.bVisible); Light->SetHiddenInGame(Saved.bHidden);
		Light->SetCastShadows(Saved.bCastShadows);
		Light->SetMobility(EComponentMobility::Type(Saved.Mobility));
	}
	for (auto& Light : FillLights) if (Light.IsValid()) Light->Destroy();
	Before.Reset(); FillLights.Reset(); bApplied = false;
	Activity = TEXT("Mim: not present"); Whereabouts.Reset();
}

void UTallowPlaytestSubsystem::UpdateNote()
{
	const auto* Mim = Cast<ATallowCreature>(UGameplayStatics::GetActorOfClass(this, ATallowCreature::StaticClass()));
	if (!Mim) { Activity = TEXT("Mim: not present"); Whereabouts.Reset(); return; }
	const auto* Controller = Mim->GetController();
	FString Action;
	if (Controller)
	{
		if (const auto* Decision = FindFProperty<FNameProperty>(Controller->GetClass(), TEXT("LastDecision")))
			if (Decision->GetPropertyValue_InContainer(Controller).ToString().Contains(TEXT("no feasible")))
				Action = TEXT("blocked / looking for a route");
		if (Action.IsEmpty())
			if (const auto* Brain = Controller->FindComponentByClass<UBrainComponent>(); Brain && Brain->IsPaused())
				Action = TEXT("waiting");
	}
	if (Action.IsEmpty())
	{
		const FString Intent = ReadIntent(const_cast<AController*>(Controller));
		if (Intent == TEXT("Wander")) Action = TEXT("roaming");
		else if (Intent == TEXT("Stalk")) Action = TEXT("stalking");
		else if (Intent == TEXT("Hunt")) Action = TEXT("following a sound");
		else if (Intent == TEXT("Chase")) Action = TEXT("chasing");
		else if (Intent == TEXT("Listen")) Action = TEXT("listening");
		else if (Intent == TEXT("Search")) Action = TEXT("searching");
		else if (Intent == TEXT("Inspect")) Action = TEXT("checking a hiding place");
		else if (Intent == TEXT("Lurk")) Action = TEXT("waiting to ambush");
		else if (Intent == TEXT("Disguise")) Action = TEXT("disguised / lying still");
		else if (Intent == TEXT("Hold")) Action = TEXT("holding its position");
		else if (Intent == TEXT("Lunge") || Intent == TEXT("Reveal")) Action = TEXT("reaching for you");
		else
		{
			switch (Mim->CurrentState)
			{
			case ETallowCreatureState::Chasing: Action = TEXT("chasing"); break;
			case ETallowCreatureState::Investigating: Action = TEXT("investigating"); break;
			case ETallowCreatureState::SearchingCorner: case ETallowCreatureState::SearchHigh: case ETallowCreatureState::SearchLow: Action = TEXT("searching"); break;
			default: Action = Mim->GetVelocity().Size() > 5.f ? TEXT("roaming") : TEXT("standing still"); break;
			}
		}
	}
	Activity = TEXT("Mim: ") + Action;
	Whereabouts = DescribeLocation(Mim->GetActorLocation());
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
		Whereabouts += FString::Printf(TEXT("  /  %.0f m away"), FVector::Dist(Mim->GetActorLocation(), Player->GetActorLocation()) / 100.f);
}
