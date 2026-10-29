#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AI/Hunter/TallowHunterDirector.h"
#include "Chapter/TallowChapterTypes.h"
#include "TallowChapterDefinition.generated.h"

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class TALLOW_API UTallowChapterDefinition : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chapter") FName ChapterId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chapter") ETallowChapterKind Kind = ETallowChapterKind::Hunt;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chapter") FText CardNumber;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chapter") FText CardTitle;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chapter") bool bStartsInChair = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chapter") bool bPlushPhase = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story") TArray<FName> StoryObjectCandidates;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story") FName StoryFlag;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story") FText BloodWriting;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chair") int32 FirstChairRocks = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chair") int32 RestraintChairRocks = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chair") ETallowRestraintCandle RestraintCandle = ETallowRestraintCandle::KeepInHandOut;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout") FTallowDoorRules Doors;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout") TArray<FName> RequiredRooms;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resources") FTallowResourceRules Resources;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hunt") ETallowStoryAct ExplorationAct = ETallowStoryAct::Dormant;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hunt") ETallowStoryAct HuntAct = ETallowStoryAct::FirstContact;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hunt") FName AIProfile;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hunt") float HuntStartMinWalkCm = 1400.f;
	/** The draught and the battery's hum lead to the battery in the hunt (29 Oct: every objective is led to). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hunt") bool bDraughtGuidesToBattery = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scares") TArray<FName> ScareGroups;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Text") TMap<ETallowChapterPhase, FText> MapClues;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Text") TMap<ETallowChapterPhase, FText> Objectives;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beats") FTallowBeatTimeouts Beats;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battery") FText BatteryNoun;
	FTallowChapterRules MakeRules(int32 ChapterIndex) const;
};

UCLASS(BlueprintType)
class TALLOW_API UTallowCampaignDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="Campaign") TArray<TObjectPtr<UTallowChapterDefinition>> Chapters;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") TArray<FTallowScareGroupDef> ScareGroups;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") FTallowScareRules Scares;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") FTallowRoomSearchRules RoomSearch;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") FTallowPlushRules Plush;
	/** An ATallowPlushMim subclass (the art side's Blueprint); empty spawns the native class. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") TSoftClassPtr<AActor> PlushClass;
	int32 GetBatteryChapterCount() const;
	static UTallowCampaignDefinition* LoadOrBuildDefault(UObject* Outer);
	static UTallowCampaignDefinition* BuildDefault(UObject* Outer);
};
