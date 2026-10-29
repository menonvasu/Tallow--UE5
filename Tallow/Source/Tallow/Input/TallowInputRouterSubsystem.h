#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Containers/Ticker.h"
#include "Chapter/TallowChapterTypes.h"
#include "TallowInputRouterSubsystem.generated.h"

class UEnhancedInputComponent;
class UTallowGamepadContext;
class UTallowLockpickGame;
class ATallowPlayDirector;
class ATallowChairEscape;
class ATallowGameMode;
class APlayerController;

/** Routes the DualSense into the game without touching the character, the director or the controller: it applies the
 *  runtime gamepad contexts, binds the pad-only actions to the director's public calls, swaps in the lockpick feel while a
 *  lock is open, and turns game events into rumble. A world subsystem, so it is rebuilt on every travel. */
UCLASS()
class TALLOW_API UTallowInputRouterSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	static UTallowInputRouterSubsystem* Get(const UObject* WorldContext);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UTallowGamepadContext* GetPadContext() const { return Pad; }
	bool IsMainContextApplied() const;
	bool IsLockpickContextApplied() const;
	bool IsSkipContextApplied() const;
	/** Re-checks everything the 10 Hz poll checks (tests call this so they need not wait). */
	void Refresh();

protected:
	UFUNCTION() void HandlePause();
	UFUNCTION() void HandleInventory();
	UFUNCTION() void HandleMap();
	UFUNCTION() void HandleSkip();
	UFUNCTION() void HandleReadToggle();
	UFUNCTION() void HandleReadSkip();
	UFUNCTION() void HandleStopLockpick();
	UFUNCTION() void HandlePinSet();
	UFUNCTION() void HandleSnap();
	UFUNCTION() void HandleLockOpen();
	UFUNCTION() void HandleCaught();
	UFUNCTION() void HandleBeat(ETallowChapterBeat Beat, const FTallowBeatContext& Context);

	void Bind(APlayerController* PC);
	void SetContext(class UInputMappingContext* Context, int32 Priority, bool bWanted);
	bool WantsLockpickContext() const;
	bool WantsSkipContext();
	ATallowPlayDirector* Director();

	UPROPERTY(Transient) TObjectPtr<UTallowGamepadContext> Pad;
	UPROPERTY(Transient) TObjectPtr<UEnhancedInputComponent> Component;
	TWeakObjectPtr<APlayerController> BoundController;
	TWeakObjectPtr<UTallowLockpickGame> BoundLock;
	TWeakObjectPtr<ATallowGameMode> BoundMode;
	TWeakObjectPtr<UObject> BoundChapters;
	TWeakObjectPtr<ATallowPlayDirector> CachedDirector;
	TWeakObjectPtr<ATallowChairEscape> CachedChair;
	int32 LastRocks = 0;
	bool bWasTied = false;
	FTSTicker::FDelegateHandle PollHandle;
};
