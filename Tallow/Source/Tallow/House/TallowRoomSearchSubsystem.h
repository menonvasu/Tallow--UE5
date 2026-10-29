#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Chapter/TallowChapterTypes.h"
#include "House/TallowRoomSearchTracker.h"
#include "TallowRoomSearchSubsystem.generated.h"

USTRUCT(BlueprintType)
struct TALLOW_API FTallowRoomSearchState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Room;
	UPROPERTY(BlueprintReadOnly) bool bVisited = false;
	UPROPERTY(BlueprintReadOnly) bool bSearched = false;
	UPROPERTY(BlueprintReadOnly) bool bHere = false;
	UPROPERTY(BlueprintReadOnly) float Progress = 0.f;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTallowRoomSearchChanged, FName, Room);

UCLASS()
class TALLOW_API UTallowRoomSearchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	static UTallowRoomSearchSubsystem* Get(const UObject* WorldContext);
	void Begin(const FTallowRoomSearchRules& Rules);
	void ResetRound();
	void RestoreVisited(const TArray<FName>& Rooms);
	TArray<FName> GetVisited() const;
	UFUNCTION(BlueprintPure, Category="Search") FTallowRoomSearchState GetRoomState(FName Room) const;
	UFUNCTION(BlueprintPure, Category="Search") bool IsRunning() const { return bRunning; }
	UPROPERTY(BlueprintAssignable, Category="Search") FTallowRoomSearchChanged OnRoomStateChanged;
    FName GetCurrentRoom() const;
    void GatherSearchPoints(FName Room, TArray<AActor*>& Out) const;
    void Draw(float Seconds) const;
protected:
    UFUNCTION() void OnRoundReset(int32 ChapterIndex, ETallowRestartReason Reason);
    FTallowRoomSearchTracker Tracker;
    TArray<TWeakObjectPtr<AActor>> SearchActors;
    TWeakObjectPtr<class UTallowChapterSubsystem> BoundChapter;
    FName LastRoom;
    float SampleAge = 0.f;
	bool bRunning = false;
};
