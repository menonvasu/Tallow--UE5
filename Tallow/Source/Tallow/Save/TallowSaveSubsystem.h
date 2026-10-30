#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Chapter/TallowChapterTypes.h"
#include "TallowSaveSubsystem.generated.h"

UCLASS()
class TALLOW_API UTallowSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	static UTallowSaveSubsystem* Get(const UObject* WorldContext);
	UFUNCTION(BlueprintPure, Category="Save") bool HasContinue() const;
	/** Which checkpoint Continue resumes at (None: the saved chapter's start). */
	UFUNCTION(BlueprintPure, Category="Save") ETallowCheckpoint GetSavedCheckpoint() const { HasContinue(); return SavedCheckpoint; }
	UFUNCTION(BlueprintCallable, Category="Save") bool PrepareContinue();
	UFUNCTION(BlueprintCallable, Category="Save") void PrepareNewGame(int32 Seed = 0);
	UFUNCTION(BlueprintCallable, Category="Save") void PrepareRestart(bool bFromEnding);
	bool ConsumePendingStart(ETallowPendingStart& OutKind, FTallowCampaignState& OutState);
	bool SaveCampaign(const FTallowCampaignState& State);
	bool LoadCampaign(FTallowCampaignState& OutState) const;
	UFUNCTION(BlueprintCallable, Category="Save") void DeleteSave();
	UFUNCTION(BlueprintCallable, Category="Save") void MarkCampaignComplete();
	FSimpleMulticastDelegate OnSaveChanged;
	FString GetSaveDirectory() const;
protected:
	ETallowPendingStart Pending = ETallowPendingStart::None;
	FTallowCampaignState PendingState;
	FString SaveDirectory;
	mutable bool bContinueAvailable = false;
	mutable bool bAvailabilityKnown = false;
	mutable ETallowCheckpoint SavedCheckpoint = ETallowCheckpoint::None;
	void RefreshAvailability();
};
