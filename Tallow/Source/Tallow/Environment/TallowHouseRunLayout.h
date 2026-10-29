#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TallowHouseRunLayout.generated.h"
class ATallowDoor;
class ATallowOptionalBoards;

USTRUCT(BlueprintType)
struct FTallowRunDoorCandidate
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<ATallowDoor> Door;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<ATallowOptionalBoards> OptionalBoards;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName RouteGroup;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 VariantIndex = 0;
};

USTRUCT(BlueprintType)
struct FTallowRunDoorState
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TObjectPtr<ATallowDoor> Door;
    UPROPERTY(BlueprintReadOnly) FName RouteGroup;
    UPROPERTY(BlueprintReadOnly) FVector DoorwayPosition = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) bool bLocked = false;
    UPROPERTY(BlueprintReadOnly) bool bBarricaded = false;
    UPROPERTY(BlueprintReadOnly) bool bStartsLocked = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTallowRunDoorsChanged);

/** Seeded optional locks only. Required workshop, power and ending gates are
 * excluded. Each two-door room keeps one unboarded, lockpickable entrance. */
UCLASS()
class TALLOW_API ATallowHouseRunLayout : public AActor
{
    GENERATED_BODY()
public:
    ATallowHouseRunLayout();
    virtual void Tick(float DeltaSeconds) override;
    void SetChapterControlled(bool bControlled) { bChapterControlled = bControlled; }
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="House|Run") TArray<FTallowRunDoorCandidate> Candidates;
    UPROPERTY(BlueprintReadOnly, Category="House|Run") int32 LayoutSeed = 0;
    UPROPERTY(BlueprintReadOnly, Category="House|Run") bool bLayoutReady = false;
    /** Live door states only; contains no pickup coordinates or discovery rules. */
    UFUNCTION(BlueprintPure, Category="House|Run") TArray<FTallowRunDoorState> GetDoorStates() const;
    UFUNCTION(BlueprintPure, Category="House|Run") TArray<bool> PreviewStartingLocks(int32 Seed) const;
    UFUNCTION(BlueprintCallable, Category="House|Run") void RestoreStartingLayout();
    UPROPERTY(BlueprintAssignable, Category="House|Run") FOnTallowRunDoorsChanged OnRunDoorsChanged;
private:
    bool bChapterControlled = false;
    TArray<bool> StartingLocks;
    bool bWasTied = true;
    uint32 PreviousStateHash = 0;
};
