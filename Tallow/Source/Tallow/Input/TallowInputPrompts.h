#pragma once
#include "CoreMinimal.h"
#include "TallowInputPrompts.generated.h"

UENUM(BlueprintType) enum class ETallowInputDevice : uint8 { KeyboardMouse, Gamepad };
UENUM(BlueprintType) enum class ETallowPromptAction : uint8
{
	Interact, RockChair, Cup, Relight, HoldBreath, Tension, PickMove, Run, Crouch, Drop, Inventory, Map, Pause, Skip, Back, Confirm,
	ReadToggle, ReadSkip, PagePrev, PageNext, FloorPrev, FloorNext, StopLockpick, KeypadErase, KeypadClear, KeypadSubmit
};
USTRUCT(BlueprintType) struct TALLOW_API FTallowPrompt
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FText Label;   // "E", "space" / "cross", "R2"
	UPROPERTY(BlueprintReadOnly) FName Glyph;   // "Key.E", "Mouse.Right" / "PS.Cross", "PS.R2": the art side draws these
};
namespace TallowInputPrompts
{
	TALLOW_API FTallowPrompt Get(ETallowPromptAction Action, ETallowInputDevice Device);
	TALLOW_API FTallowPrompt GetCurrent(const UObject* WorldContext, ETallowPromptAction Action);
	TALLOW_API FString Expand(const UObject* WorldContext, const FString& TextWithTokens);   // "{Interact}" etc.
	TALLOW_API FText ControlsSheet(ETallowInputDevice Device);
}
