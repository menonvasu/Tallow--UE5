#pragma once
#include "CoreMinimal.h"
#include "TallowChapterTypes.generated.h"

/** Where a chapter is. Paused is reported by GetEffectivePhase only and never stored. */
UENUM(BlueprintType)
enum class ETallowChapterPhase : uint8
{
	None, InitialRestraint, Exploration, StoryInteraction, Transformation, Restraint, HuntPreparation,
	ActiveHunt, BatteryAcquired, FuseSequence, ChapterTransition, FinalSequence, Caught, Paused
};

/** Inputs go through NotifyEvent; the last five are broadcast only. */
UENUM(BlueprintType)
enum class ETallowChapterEvent : uint8
{
	StoryStarted, StoryClueDone, ChairEscaped, BatteryCollected, BatteryDropped, BatteryLost, FuseActivated, Caught, FinalReached,
	TransformationStarted, TransformationFinished, ChapterStarted, ChapterCompleted, RoundRebuilt
};

UENUM(BlueprintType) enum class ETallowChapterKind : uint8 { Hunt, Final };
UENUM(BlueprintType) enum class ETallowMimForm : uint8 { Absent, Plush, Transforming, Full };
UENUM(BlueprintType) enum class ETallowDoorState : uint8 { Open, Closed, Lockpick, Crowbar, StoryGated };
UENUM(BlueprintType) enum class ETallowEdgeKind : uint8 { Opening, Door, Stairs, Crawl, RouteGate };
UENUM(BlueprintType) enum class ETallowChapterBeat : uint8 { None, BloodWriting, Transformation, Restraint, HuntRelease, FuseEvent, ChapterReset, FinalIntro };
UENUM(BlueprintType) enum class ETallowRestartReason : uint8 { NewCampaign, Continue, Caught, NextChapter, Debug, Recovery };
UENUM(BlueprintType) enum class ETallowSpotKind : uint8 { Battery, Crowbar, Matchbox, Wax, Chair, Story, ChapterStart, MimHuntStart };
UENUM(BlueprintType) enum class ETallowRestraintCandle : uint8 { KeepInHandOut, PlaceBesideChair };
UENUM(BlueprintType) enum class ETallowPlushState : uint8 { Hidden, Perched, Watching, Relocating, Guiding, Scaring, Transforming, Retired };
UENUM(BlueprintType) enum class ETallowPendingStart : uint8 { None, NewCampaign, Continue };

USTRUCT(BlueprintType) struct TALLOW_API FTallowDoorRules
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MinLockpick = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MaxLockpick = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MinCrowbar = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MaxCrowbar = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MinChangedFromPrevious = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<ETallowDoorState, float> StateWeights;   // empty: Open .25 Closed .35 Lockpick .25 Crowbar .15
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, ETallowDoorState> ForcedStates;    // edge id -> state
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> FixedEdges;                       // keep DefaultState
};

USTRUCT(BlueprintType) struct TALLOW_API FTallowResourceRules
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MinMatchesAtStart = 4;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MaxCarriedMatches = 12;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Matchboxes = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MatchesPerBox = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 WaxPickups = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MinWaxAtStart = .5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bCrowbarInHouse = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bKeepCrowbar = false;
};

USTRUCT(BlueprintType) struct TALLOW_API FTallowBeatTimeouts
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BloodWriting = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Transformation = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Restraint = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HuntRelease = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float FuseEvent = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ChapterReset = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float FinalIntro = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TransformationDeferSeconds = 40.f;
};

/** Where a minor scare happens. Prop scares move a house prop in view; the rest are sounds placed around Nell. */
UENUM(BlueprintType) enum class ETallowScarePlace : uint8
{
	Prop,       // a house prop (ActorNamePatterns) moves where she can see it, with the group's sound if it has one
	Above,      // on the floor overhead, crossing the room
	InWall,     // inside the nearest wall, an arm's length away
	Elsewhere,  // at a door in another room
	Near,       // in a room close by, out of sight (PreferredRooms first)
	Head,       // a whisper that passes from one ear to the other
	Herring     // one of the soundscape's red herrings
};

USTRUCT(BlueprintType) struct TALLOW_API FTallowScareGroupDef
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Group;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Weight = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CooldownMin = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CooldownMax = 160.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<ETallowChapterPhase> Phases;   // empty: Exploration and ActiveHunt
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 FirstChapter = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 LastChapter = 99;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bSilent = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bMajor = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bAlertsMimInHunt = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float NoiseLoudness = .35f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float NoiseRange = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FString> ActorNamePatterns;   // wildcard on actor names, e.g. "A6_04*_HangingDrawing"
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bPlush = false;                 // fired by the plush, not a prop
	UPROPERTY(EditAnywhere, BlueprintReadOnly) ETallowScarePlace Place = ETallowScarePlace::Prop;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FString> Sounds;             // object paths; a different one each time
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Volume = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> PreferredRooms;       // Near: rooms tried first (TallowHouseRooms ids)
};

USTRUCT(BlueprintType) struct TALLOW_API FTallowScareRules
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MinGapSeconds = 35.f;
	/** Each gap is drawn between MinGapSeconds and this (no larger than the minimum: a fixed gap). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaxGapSeconds = 0.f;
	/** The first scare of a chapter waits this long, drawn in the range (zero: the ordinary gap). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D FirstGapSeconds = FVector2D::ZeroVector;
	/** Gap multiplier by chapter index, so the house presses harder as the chapters go (missing entries: 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<float> ChapterGapScale;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HuntGapScale = 1.3f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MaxPerChapter = 8;
};

USTRUCT(BlueprintType) struct TALLOW_API FTallowRoomSearchRules
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SeenRadiusCm = 320.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DarkSeenRadiusCm = 150.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ViewConeDegrees = 35.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RequiredFraction = .75f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MinDwellSeconds = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float EmptyRoomDwellSeconds = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float EmptyRoomYawCoverageDegrees = 270.f;
};

USTRUCT(BlueprintType) struct TALLOW_API FTallowPlushRules
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RelocateMinSeconds = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RelocateMaxSeconds = 28.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float UnseenSeconds = .6f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ViewConeDegrees = 50.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MinPlayerDistanceCm = 250.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MinPathDistanceCm = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float GuideAheadMinCm = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float GuideAheadMaxCm = 900.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bMinorScares = true;
};

USTRUCT(BlueprintType) struct TALLOW_API FTallowChapterRules
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 ChapterIndex = 0;
	UPROPERTY(BlueprintReadOnly) ETallowChapterKind Kind = ETallowChapterKind::Hunt;
	UPROPERTY(BlueprintReadOnly) FTallowDoorRules Doors;
	UPROPERTY(BlueprintReadOnly) FTallowResourceRules Resources;
	UPROPERTY(BlueprintReadOnly) TArray<FName> StoryCandidates;
	UPROPERTY(BlueprintReadOnly) TArray<FName> RequiredRooms;
	UPROPERTY(BlueprintReadOnly) bool bBattery = true;
};

USTRUCT(BlueprintType) struct TALLOW_API FTallowDoorAssignment
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Edge;
	UPROPERTY(BlueprintReadOnly) ETallowDoorState State = ETallowDoorState::Closed;
};

USTRUCT(BlueprintType) struct TALLOW_API FTallowChapterLayout
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 Seed = 0;
	UPROPERTY(BlueprintReadOnly) int32 Attempt = 0;
	UPROPERTY(BlueprintReadOnly) bool bFallback = false;
	UPROPERTY(BlueprintReadOnly) TArray<FTallowDoorAssignment> Doors;
	UPROPERTY(BlueprintReadOnly) FName StartRoom;
	UPROPERTY(BlueprintReadOnly) FName StoryObject;
	UPROPERTY(BlueprintReadOnly) FName StoryRoom;
	UPROPERTY(BlueprintReadOnly) FName ChairRoom;
	UPROPERTY(BlueprintReadOnly) FName ChairSpot;
	UPROPERTY(BlueprintReadOnly) FName BatterySpot;
	UPROPERTY(BlueprintReadOnly) FName BatteryRoom;
	UPROPERTY(BlueprintReadOnly) FName CrowbarSpot;
	UPROPERTY(BlueprintReadOnly) TArray<FName> MatchboxSpots;
	UPROPERTY(BlueprintReadOnly) TArray<FName> WaxSpots;
	UPROPERTY(BlueprintReadOnly) FName MimHuntStart;
	UPROPERTY(BlueprintReadOnly) TArray<FString> Notes;   // rejected attempts, for logs
	ETallowDoorState GetDoorState(FName Edge, ETallowDoorState Fallback) const;
	int32 DoorHash() const;
};

USTRUCT(BlueprintType) struct TALLOW_API FTallowLayoutHistory
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TArray<FName> UsedBatterySpots;
	UPROPERTY(BlueprintReadOnly) TArray<FName> UsedChairRooms;
	UPROPERTY(BlueprintReadOnly) TArray<FName> UsedStoryObjects;
	UPROPERTY(BlueprintReadOnly) int32 PreviousDoorHash = 0;
	UPROPERTY(BlueprintReadOnly) TArray<FTallowDoorAssignment> PreviousDoors;
};

USTRUCT(BlueprintType) struct TALLOW_API FTallowResourceSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 Matches = 0;
	UPROPERTY(BlueprintReadOnly) float Wax = 1.f;
	UPROPERTY(BlueprintReadOnly) bool bHasCandle = false;
	UPROPERTY(BlueprintReadOnly) bool bHasMap = false;
	UPROPERTY(BlueprintReadOnly) bool bHasCrowbar = false;
};

USTRUCT(BlueprintType) struct TALLOW_API FTallowCampaignState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 Version = 1;
	UPROPERTY(BlueprintReadOnly) int32 CampaignSeed = 0;
	UPROPERTY(BlueprintReadOnly) int32 ChapterIndex = 0;
	UPROPERTY(BlueprintReadOnly) int32 ChapterSeed = 0;
	UPROPERTY(BlueprintReadOnly) int32 Attempt = 0;
	UPROPERTY(BlueprintReadOnly) int32 InstalledFuses = 0;
	UPROPERTY(BlueprintReadOnly) TArray<FName> CompletedChapters;
	UPROPERTY(BlueprintReadOnly) TArray<FName> StoryFlags;
	UPROPERTY(BlueprintReadOnly) TArray<FName> LoreRead;
	UPROPERTY(BlueprintReadOnly) TArray<FName> VisitedRooms;
	UPROPERTY(BlueprintReadOnly) FTallowLayoutHistory History;
	UPROPERTY(BlueprintReadOnly) FTallowResourceSnapshot Resources;
	UPROPERTY(BlueprintReadOnly) float PlaySeconds = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bComplete = false;
};

USTRUCT(BlueprintType) struct TALLOW_API FTallowBeatContext
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 Serial = 0;
	UPROPERTY(BlueprintReadOnly) ETallowChapterBeat Beat = ETallowChapterBeat::None;
	UPROPERTY(BlueprintReadOnly) int32 ChapterIndex = 0;
	UPROPERTY(BlueprintReadOnly) FName Room;
	UPROPERTY(BlueprintReadOnly) FTransform Where;
	UPROPERTY(BlueprintReadOnly) FTransform ChairTransform;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Subject = nullptr;
	UPROPERTY(BlueprintReadOnly) FText Text;
	UPROPERTY(BlueprintReadOnly) float MaxSeconds = 0.f;
};
