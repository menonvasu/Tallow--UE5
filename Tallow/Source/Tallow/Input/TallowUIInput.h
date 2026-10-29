#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "TallowUIInput.generated.h"

UENUM(BlueprintType) enum class ETallowPadIntent : uint8 { None, Up, Down, Left, Right, Confirm, Back, Secondary, Tertiary, PrevTab, NextTab, Menu, View };
namespace TallowUIInput
{
	TALLOW_API bool IsGamepadKey(const FKey& Key);
	/** Cross Confirm, Circle Back, Square Secondary, Triangle Tertiary, L1/R1 tabs, Options Menu, Create View, d-pad and left stick directions. */
	TALLOW_API ETallowPadIntent ClassifyGamepad(const FKey& Key);
}
