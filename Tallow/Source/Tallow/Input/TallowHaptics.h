#pragma once
#include "CoreMinimal.h"
#include "TallowHaptics.generated.h"

UENUM(BlueprintType) enum class ETallowHaptic : uint8 { ChairRock, ChairSlam, LockPin, LockSnap, LockOpen, PryBoard, FuseSeat, Capture, Transformation, Heartbeat, DoorSlam };
namespace TallowHaptics
{
	TALLOW_API void Play(const UObject* WorldContext, ETallowHaptic Kind, float Scale = 1.f);
	TALLOW_API void SetEnabled(bool bEnabled);
	TALLOW_API bool IsEnabled();
}
