#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AI/Hunter/TallowHunterDirector.h"
#include "Chapter/TallowChapterTypes.h"
#include "Chapter/TallowChapterBeats.h"
#include "House/TallowLayoutSolver.h"
#include "TallowChapterSubsystem.generated.h"

class ATallowPlayDirector;
class UTallowCampaignDefinition;
class UTallowChapterDefinition;

/** What the house leads her to next (the draught in the flame and the objective's sound). */
enum class ETallowGuideKind : uint8 { None, Tape, Battery, FuseBox };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTallowChapterPhaseChanged, ETallowChapterPhase, OldPhase, ETallowChapterPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTallowChapterSignal, ETallowChapterEvent, Event, FName, Detail);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTallowChapterRoundReset, int32, ChapterIndex, ETallowRestartReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTallowChapterBeatRequested, ETallowChapterBeat, Beat, const FTallowBeatContext&, Context);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTallowMimFormChanged, ETallowMimForm, Form);

/** Pass 2: the chapter loop for the playable house. The play director activates it and keeps every widget, cue and
 *  hand; the hunter director keeps the pacing. This only decides where the chapter is and rebuilds a round. */
UCLASS()
class TALLOW_API UTallowChapterSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** tallow.Chapters 1 or -TallowChapters on the command line. */
	static bool ShouldRunChapters();
	/** The subsystem once Activate has built a round; nullptr otherwise. Every hook guards on this. */
	static UTallowChapterSubsystem* GetActive(const UObject* WorldContext);

	void Activate(ATallowPlayDirector* InDirector);
	UFUNCTION(BlueprintPure, Category="Chapter") bool IsActive() const { return bActive; }

	UFUNCTION(BlueprintPure, Category="Chapter") ETallowChapterPhase GetPhase() const { return Phase; }
	UFUNCTION(BlueprintPure, Category="Chapter") ETallowChapterPhase GetEffectivePhase() const;
	UFUNCTION(BlueprintPure, Category="Chapter") int32 GetChapterIndex() const { return Campaign.ChapterIndex; }
	UFUNCTION(BlueprintPure, Category="Chapter") int32 GetChapterSeed() const { return Campaign.ChapterSeed; }
	UFUNCTION(BlueprintPure, Category="Chapter") ETallowChapterKind GetChapterKind() const;
	UFUNCTION(BlueprintPure, Category="Chapter") ETallowMimForm GetMimForm() const { return MimForm; }
	UFUNCTION(BlueprintPure, Category="Chapter") bool AllowsCapture() const;
	UFUNCTION(BlueprintPure, Category="Chapter") int32 GetTransitionSerial() const { return Serial; }
	const UTallowChapterDefinition* GetChapterDefinition() const;
	const UTallowCampaignDefinition* GetCampaignDefinition() const { return CampaignDef; }
	const FTallowChapterLayout& GetLayout() const { return Layout; }
	const FTallowCampaignState& GetCampaign() const { return Campaign; }
	/** Copies for scripts and Blueprints, which cannot take the references above. */
	UFUNCTION(BlueprintPure, Category="Chapter") FTallowChapterLayout GetCurrentLayout() const { return Layout; }
	UFUNCTION(BlueprintPure, Category="Chapter") FTallowCampaignState GetCurrentCampaign() const { return Campaign; }

	UFUNCTION(BlueprintCallable, Category="Chapter") void NotifyEvent(ETallowChapterEvent Event, FName Detail = NAME_None);
	UFUNCTION(BlueprintCallable, Category="Chapter|Beats") bool ClaimBeat(ETallowChapterBeat Beat, int32 BeatSerial);
	UFUNCTION(BlueprintCallable, Category="Chapter|Beats") void NotifyBeatCut(ETallowChapterBeat Beat, int32 BeatSerial);
	UFUNCTION(BlueprintCallable, Category="Chapter|Beats") void NotifyBeatFinished(ETallowChapterBeat Beat, int32 BeatSerial);

	void RestartChapter(ETallowRestartReason Reason);
	bool JumpToChapter(int32 ChapterIndex, ETallowChapterPhase Phase = ETallowChapterPhase::None);

	ETallowStoryAct GetPacingAct() const;
	/** What she needs next: the chapter's tape until it has played, then the battery, then the fuse box. */
	AActor* GetGuideObjective(ETallowGuideKind& OutKind) const;
	/** Where the draught leans: the next corner of her walk to the objective (zero when there is none).
	 *  OutWalk: how far she still has to walk to it (cm). */
	FVector GetGuideTarget(float* OutWalk = nullptr) const;
	/** The next corner of her navmesh walk toward Target (Target itself on the last leg or when unreachable). */
	FVector NextCorner(const FVector& Target, float* OutWalk = nullptr) const;
	FText GetObjectiveText() const;
	FText GetMapClue() const;
	int32 GetChapterCardIndex() const;
	FText GetChapterCardNumber() const;
	FText GetChapterCardTitle() const;

	UPROPERTY(BlueprintAssignable, Category="Chapter") FTallowChapterPhaseChanged OnPhaseChanged;
	UPROPERTY(BlueprintAssignable, Category="Chapter") FTallowChapterSignal OnChapterEvent;
	UPROPERTY(BlueprintAssignable, Category="Chapter") FTallowChapterRoundReset OnRoundReset;
	UPROPERTY(BlueprintAssignable, Category="Chapter|Beats") FTallowChapterBeatRequested OnBeatRequested;
	UPROPERTY(BlueprintAssignable, Category="Chapter") FTallowMimFormChanged OnMimFormChanged;

    FString BuildRoundFingerprint() const;
    FString DescribeState() const;
    void SkipBeat();
    ETallowChapterBeat GetActiveBeat() const { return BeatState.Context.Beat; }
    float GetHuntStartWalkingDistance() const { return HuntStartWalkingDistance; }
    FTransform GetChapterStart() const;
    FTransform ResolveChairTransform();
    void RunWatchdogs();

protected:
    bool BuildRound(ETallowRestartReason Reason);
    void SetPhase(ETallowChapterPhase Next, FName Reason);
    void RequestBeat(ETallowChapterBeat Beat);
    void ApplyBeatCut(const FTallowBeatContext& Context);
    void SetMimForm(ETallowMimForm Form, bool bHunting = false);
    void StartHunt();
    void BeginTransformation(bool bDisplaced);
    void CompleteChapter();
    void CloseOverlays();
    void SetBeatInputLocked(bool bLocked);
    void PersistDiscoveries();
    void PlaceResources(const FTallowResourceSnapshot& Resources);
    void GatherFallbackSpots(FTallowSolverInput& Input) const;
    AActor* FindNamed(FName Name) const;
    UFUNCTION() void OnCaught();
    UFUNCTION() void OnLoreRead(FName Key);
    UPROPERTY(Transient) TObjectPtr<class ATallowChairEscape> Chair;
    UPROPERTY(Transient) TObjectPtr<class ATallowPlushMim> Plush;
    UPROPERTY(Transient) TArray<TObjectPtr<class ATallowCrowbarPickup>> AuthoredCrowbars;
    FTallowSolverInput SolverInput;
    FTallowChapterBeatState BeatState;
    TSet<ETallowChapterEvent> HandledEvents;
    bool bWritingDone = false;
    bool bWaitingForRoom = false;
    bool bBeatInputLocked = false;
    bool bRebuilding = false;
    bool bBuildFailed = false;
    bool bHasCommittedRound = false;
    bool bUsingContractLayout = false;
    FTallowCampaignState CommittedCampaign;
    FTallowChapterLayout CommittedLayout;
    bool bAwaitingNavigation = false;
    ETallowRestartReason PendingBuildReason = ETallowRestartReason::NewCampaign;
    double NavigationWaitStarted = 0.0;
    bool bHuntEnabled = false;
    bool bHuntStartPending = false;
    double HuntPendingSince = 0.0;
    bool bChairRoomCommitted = false;
    bool bChairUnavailable = false;
    double PhaseStartedAt = 0.0;
    double DeferredAt = 0.0;
    double NextWatchdog = 0.0;
    float HuntStartWalkingDistance = 0.f;
    FTransform ChosenChairTransform;
    FTransform HuntTransform;

protected:
	UPROPERTY(Transient) TObjectPtr<ATallowPlayDirector> Director;
	UPROPERTY(Transient) TObjectPtr<UTallowCampaignDefinition> CampaignDef;
	UPROPERTY(Transient) FTallowCampaignState Campaign;
	UPROPERTY(Transient) FTallowChapterLayout Layout;
	ETallowChapterPhase Phase = ETallowChapterPhase::None;
	ETallowMimForm MimForm = ETallowMimForm::Absent;
	int32 Serial = 0;
	bool bActive = false;
};
