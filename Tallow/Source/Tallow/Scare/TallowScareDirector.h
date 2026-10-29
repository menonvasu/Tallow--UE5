#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Chapter/TallowChapterTypes.h"
#include "Scare/TallowScareScheduler.h"
#include "Scare/TallowScareSight.h"
#include "TallowScareDirector.generated.h"

class UAudioComponent;
class ULightComponent;
class USoundAttenuation;
class USoundBase;
class UTallowSoundscapeComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTallowScareFired, FName, Group, AActor*, Emitter, bool, bAlertedMim);

UCLASS()
class TALLOW_API UTallowScareDirector : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	static UTallowScareDirector* Get(const UObject* WorldContext);
	void BeginChapter(int32 ChapterIndex, const TArray<FTallowScareGroupDef>& Groups, const TArray<FName>& EnabledGroups, const FTallowScareRules& Rules, int32 Seed);
	void SetPhase(ETallowChapterPhase Phase);
	void Stop();
	UFUNCTION(BlueprintPure, Category="Scares") bool OwnsScheduling() const { return bOwnsScheduling; }
	UFUNCTION(BlueprintPure, Category="Scares") int32 GetChapterEventCount() const { return ChapterEvents; }
	UPROPERTY(BlueprintAssignable, Category="Scares") FTallowScareFired OnScareFired;
    bool TryFireGroup(FName Group);
    /** Reviews only (level probes): fire a group now, as placed and heard in play, past its gaps and gates. */
    UFUNCTION(BlueprintCallable, Category="Scares|Review") bool FireGroupForReview(FName Group);
    /** The same from a script, which cannot reach a world subsystem directly. */
    UFUNCTION(BlueprintCallable, Category="Scares|Review", meta=(WorldContext="WorldContext")) static bool FireInWorldForReview(const UObject* WorldContext, FName Group)
    {
        UTallowScareDirector* Director = Get(WorldContext);
        return Director && Director->FireGroupForReview(Group);
    }
    void Describe(TArray<FString>& Lines) const;
    float GetLastFireTime() const { return Scheduler.GetLastFireTime(); }
    /** A placed scare sound is still sounding (the soundscape keeps its herrings and whispers off it). */
    bool IsCuePlaying() const;
    /** What a sight scare would use for this view, without doing anything (tests and level probes). */
    struct FSightPick
    {
        TWeakObjectPtr<AActor> Target;
        FVector At = FVector::ZeroVector, From = FVector::ZeroVector, To = FVector::ZeroVector, Axis = FVector::ZeroVector;
        float Distance = 0.f, Degrees = 0.f;
        bool bShut = false;   // Door: an open door easing shut (else a shut one coming off its latch)
    };
    bool PlaceSight(ETallowScarePlace Place, const TallowScareSight::FView& View, FSightPick& Out);
    /** Her eye, where she is looking and the room she is in, as the sight scares see it. */
    bool ReadView(TallowScareSight::FView& Out) const;
    static bool IsSight(ETallowScarePlace Place) { return Place >= ETallowScarePlace::Door; }
protected:
    UFUNCTION() void OnPhaseChanged(ETallowChapterPhase OldPhase, ETallowChapterPhase NewPhase);
    bool Fire(int32 Index);
    void BindChapter();
    /** Nothing fires over a chase, a reading, a held breath, inside earned relief, or with Mim close enough to be heard. */
    bool HouseAllowsScare(bool& bOutNormal, bool& bOutMajor) const;
    bool FireSound(const FTallowScareGroupDef& Group, bool bAlert, FString& OutWhere);
    /** Something she sees happen, near her and where she is looking (TallowScareSights.cpp). */
    bool FireSight(const FTallowScareGroupDef& Group, FString& OutWhere, AActor*& OutTarget);
    bool StageSight(const FTallowScareGroupDef& Group, const FSightPick& Pick, FString& OutWhere);
    void UpdateSights(float Now);
    void ClearSights();
    /** The house's own small sounds for the sight scares, at a point in the world, now or a moment later. */
    void PlayAt(const TCHAR* Path, const FVector& Where, float Volume, float Delay = 0.f, bool bSilent = false);
    USoundBase* LoadSound(const FString& Path);
    bool IsClear(const FVector& Eye, const FVector& At, const AActor* Target, float Slack = 0.f) const;
    struct FPendingCue { float At = 0.f; FString Path; FVector Where = FVector::ZeroVector; float Volume = 1.f; };
    TArray<FPendingCue> PendingCues;
    /** A candle or lamp guttering out and catching again. */
    struct FGutter { TWeakObjectPtr<ULightComponent> Light; TWeakObjectPtr<AActor> Flame; float Base = 0.f, Started = 0.f, OutAt = 0.f, BackAt = 0.f; bool bOut = false; };
    TArray<FGutter> Gutters;
    /** Stand-ins spawned this chapter, and the props they hide; both put back when the chapter resets. */
    UPROPERTY(Transient) TArray<TObjectPtr<AActor>> Staged;
    TArray<TWeakObjectPtr<AActor>> HiddenProps;
    TSet<FName> UsedTargets;
    /** The music box behind her: it plays until she turns to look at it. */
    FVector LookAt = FVector::ZeroVector;
    bool bLookCue = false;
    float SightWaitSince = -1.f;
    bool PlaceSound(const FTallowScareGroupDef& Group, FVector& OutFrom, FVector& OutTo);
    USoundBase* PickSound(const FTallowScareGroupDef& Group);
    void MakeAttenuation();
    /** Things she sees (and the plush, and the moving props) come before sounds placed out of sight. */
    bool IsSeenGroup(int32 Index) const;
    UTallowSoundscapeComponent* FindSoundscape() const;
    struct FEmitterSettings
    {
        TWeakObjectPtr<class ATallowHousePresence> Emitter;
        bool bAlerts = false;
        float Loudness = 0.f, Range = 0.f;
        TWeakObjectPtr<USoundBase> Sound;
    };
    TArray<FEmitterSettings> OriginalSettings;
    TMap<FName, TArray<TWeakObjectPtr<class ATallowHousePresence>>> Emitters;
    TWeakObjectPtr<class UTallowChapterSubsystem> BoundChapter;
    mutable TWeakObjectPtr<UTallowSoundscapeComponent> Soundscape;
    FTallowScareScheduler Scheduler;
    FRandomStream Random;
    ETallowChapterPhase CurrentPhase = ETallowChapterPhase::None;
    int32 Chapter = 0;
    float SampleAge = 0.f;
	bool bOwnsScheduling = false;
	int32 ChapterEvents = 0;
    /** The scare sound now playing; Above cues walk from one point to another while they sound. */
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> Cue;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> PlacedAttenuation;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> ThroughAttenuation;
    UPROPERTY(Transient) TMap<FString, TObjectPtr<USoundBase>> Loaded;
    TMap<FName, FString> LastSound;
    FVector CueFrom = FVector::ZeroVector, CueTo = FVector::ZeroVector;
    float CueStarted = 0.f, CueSeconds = 0.f, ResumeAt = 0.f, BlockedSince = -1.f;
};
