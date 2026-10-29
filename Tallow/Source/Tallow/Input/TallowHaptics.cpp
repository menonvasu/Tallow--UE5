#include "Input/TallowHaptics.h"
#include "Input/TallowInputDeviceSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ForceFeedbackEffect.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/World.h"

namespace
{
	struct FHapticSpec { float Intensity; float Seconds; bool bLeftLarge, bLeftSmall, bRightLarge, bRightSmall; };
	// The whole effect table. Left large is the heavy motor, right small the fine one.
	FHapticSpec SpecFor(ETallowHaptic Kind)
	{
		switch (Kind)
		{
		case ETallowHaptic::ChairRock: return {.25f, .08f, true, false, false, false};
		case ETallowHaptic::ChairSlam: return {.6f, .25f, true, true, true, true};
		case ETallowHaptic::LockPin: return {.15f, .04f, false, false, false, true};
		case ETallowHaptic::LockSnap: return {.5f, .15f, true, true, true, true};
		case ETallowHaptic::LockOpen: return {.3f, .12f, true, true, true, true};
		case ETallowHaptic::PryBoard: return {.2f, .06f, true, false, false, true};
		case ETallowHaptic::FuseSeat: return {.45f, .2f, true, true, true, true};
		case ETallowHaptic::Capture: return {1.f, .5f, true, true, true, true};
		case ETallowHaptic::Transformation: return {.7f, 1.f, true, true, true, true};
		case ETallowHaptic::Heartbeat: return {.2f, .1f, true, false, false, false};
		case ETallowHaptic::DoorSlam: return {.4f, .15f, true, true, true, true};
		}
		return {0.f, 0.f, false, false, false, false};
	}
	int32 GEnabled = -1;   // -1: not read from the settings file yet
}

void TallowHaptics::SetEnabled(bool bEnabled) { GEnabled = bEnabled ? 1 : 0; }
bool TallowHaptics::IsEnabled()
{
	if (GEnabled < 0)
	{
		bool bStored = true;
		if (GConfig) GConfig->GetBool(TEXT("Tallow.Settings"), TEXT("Vibration"), bStored, GGameUserSettingsIni);
		GEnabled = bStored ? 1 : 0;
	}
	return GEnabled == 1;
}
void TallowHaptics::Play(const UObject* WorldContext, ETallowHaptic Kind, float Scale)
{
	if (!IsEnabled() || !WorldContext || UTallowInputDeviceSubsystem::GetCurrent(WorldContext) != ETallowInputDevice::Gamepad) return;
	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContext, 0);
	if (!PC || !PC->IsLocalController()) return;
	const FHapticSpec S = SpecFor(Kind);
	const float Intensity = FMath::Clamp(S.Intensity * Scale, 0.f, 1.f);
	if (Intensity <= 0.f) return;
	PC->PlayDynamicForceFeedback(Intensity, S.Seconds, S.bLeftLarge, S.bLeftSmall, S.bRightLarge, S.bRightSmall);
	UE_LOG(LogTemp, Display, TEXT("TALLOW_INPUT haptic=%d intensity=%.2f"), static_cast<int32>(Kind), Intensity);
}
