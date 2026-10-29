#include "Save/TallowSaveSubsystem.h"
#include "Audio/TallowCorruptedVoice.h"
#include "Save/TallowSaveGame.h"
#include "Save/TallowSaveStore.h"
#include "Input/TallowHaptics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void UTallowSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SaveDirectory = FTallowSaveStore::DefaultDirectory();
	bool bVibration = true;
	if (GConfig) GConfig->GetBool(TEXT("Tallow.Settings"), TEXT("Vibration"), bVibration, GGameUserSettingsIni);
	TallowHaptics::SetEnabled(bVibration);
	RefreshAvailability();
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("TallowContinue"))) PrepareContinue();
#endif
}

UTallowSaveSubsystem* UTallowSaveSubsystem::Get(const UObject* Context)
{
	const auto* World = Context ? Context->GetWorld() : nullptr;
	auto* Instance = World ? World->GetGameInstance() : nullptr;
	return Instance ? Instance->GetSubsystem<UTallowSaveSubsystem>() : nullptr;
}

FString UTallowSaveSubsystem::GetSaveDirectory() const
{
	return SaveDirectory.IsEmpty() ? FTallowSaveStore::DefaultDirectory() : SaveDirectory;
}

bool UTallowSaveSubsystem::HasContinue() const
{
	if (!bAvailabilityKnown)
	{
		FTallowCampaignState State;
		bContinueAvailable = LoadCampaign(State);
		bAvailabilityKnown = true;
	}
	return bContinueAvailable;
}

void UTallowSaveSubsystem::RefreshAvailability()
{
	FTallowCampaignState State;
	bContinueAvailable = LoadCampaign(State);
	bAvailabilityKnown = true;
	OnSaveChanged.Broadcast();
}

bool UTallowSaveSubsystem::PrepareContinue()
{
	Pending = ETallowPendingStart::None;
	PendingState = {};
	const bool bLoaded = LoadCampaign(PendingState);
	bContinueAvailable = bLoaded;
	bAvailabilityKnown = true;
	if (bLoaded) Pending = ETallowPendingStart::Continue;
	OnSaveChanged.Broadcast();
	return bLoaded;
}

void UTallowSaveSubsystem::PrepareNewGame(int32 Seed)
{
	PendingState = {};
	PendingState.CampaignSeed = Seed;
	Pending = ETallowPendingStart::NewCampaign;
}

void UTallowSaveSubsystem::PrepareRestart(bool bFromEnding)
{
	if (bFromEnding || !PrepareContinue()) PrepareNewGame();
}

bool UTallowSaveSubsystem::ConsumePendingStart(ETallowPendingStart& OutKind, FTallowCampaignState& OutState)
{
	OutKind = Pending;
	OutState = PendingState;
	const bool bHadPending = Pending != ETallowPendingStart::None;
	Pending = ETallowPendingStart::None;
	PendingState = {};
	return bHadPending;
}

bool UTallowSaveSubsystem::SaveCampaign(const FTallowCampaignState& State)
{
	auto* Save = NewObject<UTallowSaveGame>(this);
	Save->BuildId = FApp::GetBuildVersion();
	Save->SavedAt = FDateTime::UtcNow();
	Save->Campaign = State;
	int32 Bytes = 0;
	FString Reason;
	const bool bWrote = FTallowSaveStore(GetSaveDirectory()).Write(Save, Bytes, Reason);
	if (bWrote) { UE_LOG(LogTemp, Display, TEXT("TALLOW_SAVE wrote ch=%d seed=%d bytes=%d"), State.ChapterIndex, State.CampaignSeed, Bytes); }
	else { UE_LOG(LogTemp, Warning, TEXT("TALLOW_SAVE rejected write: %s"), *Reason); }
	RefreshAvailability();
	return bWrote;
}

bool UTallowSaveSubsystem::LoadCampaign(FTallowCampaignState& OutState) const
{
	FString Reason;
	auto* Save = FTallowSaveStore(GetSaveDirectory()).Load(Reason);
	if (!Save || Save->Campaign.bComplete)
	{
		UE_LOG(LogTemp, Display, TEXT("TALLOW_SAVE rejected %s"), Save ? TEXT("campaign complete") : *Reason);
		return false;
	}
	OutState = Save->Campaign;
	UE_LOG(LogTemp, Display, TEXT("TALLOW_SAVE loaded %s ch=%d seed=%d"), *Reason, OutState.ChapterIndex, OutState.CampaignSeed);
	return true;
}

void UTallowSaveSubsystem::DeleteSave()
{
	Pending = ETallowPendingStart::None;
	PendingState = {};
	if (!FTallowSaveStore(GetSaveDirectory()).Delete()) UE_LOG(LogTemp, Warning, TEXT("TALLOW_SAVE rejected delete"));
	RefreshAvailability();
}

void UTallowSaveSubsystem::MarkCampaignComplete() { TallowCorrupted::MarkStoryCompleted(); DeleteSave(); }

