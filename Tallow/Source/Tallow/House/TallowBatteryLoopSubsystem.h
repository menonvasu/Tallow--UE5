#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Chapter/TallowChapterTypes.h"
#include "TallowBatteryLoopSubsystem.generated.h"

UCLASS()
class TALLOW_API UTallowBatteryLoopSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	static UTallowBatteryLoopSubsystem* Get(const UObject* WorldContext);
	void PrepareRound(const FTallowChapterLayout& Layout, int32 InstalledFuses, int32 RequiredFuses, const FText& Noun);
	void ReleaseBattery();
	void RetireBattery();
	bool IsBatteryCarried() const;
	bool DoesBatteryExist() const;
	void EnsureBattery();
	int32 GetInstalledFuses() const;
	FName GetFuseBoxRoom() const;
	AActor* GetBatteryActor() const;
	class ATallowFuseBox* GetFuseBox() const;
    DECLARE_MULTICAST_DELEGATE_OneParam(FBatteryEvent, ETallowChapterEvent);
    FBatteryEvent OnBatteryEvent;
protected:
    UFUNCTION() void OnFuseInserted();
    void PollInventory();
    void Signal(ETallowChapterEvent Event);
    void SeatBattery();
    TArray<TWeakObjectPtr<class ATallowFusePickup>> OriginalFuses;
    TWeakObjectPtr<class ATallowFusePickup> Battery;
    TWeakObjectPtr<class ATallowFuseBox> Box;
    TWeakObjectPtr<AActor> BatterySpot;
    FTransform BatteryTransform;
    FTimerHandle InventoryTimer;
    FText BatteryNoun;
    bool bCapturedOriginals = false;
    bool bReleased = false;
    int32 PreviousCarried = 0;
};
