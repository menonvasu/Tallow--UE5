#include "Input/TallowGamepadContext.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "EnhancedActionKeyMapping.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	FTallowPadRow R(const FKey& Key, const TCHAR* Action, const TCHAR* Pad, const TCHAR* Does) { return {Key, FName(Action), Pad, Does}; }
	bool IsRuntimeAction(FName Name) { return Name.ToString().StartsWith(TEXT("IA_Tallow")); }
	bool ReadSetting(const TCHAR* Key)
	{
		bool bValue = false;
		if (GConfig) GConfig->GetBool(TEXT("Tallow.Settings"), Key, bValue, GGameUserSettingsIni);
		return bValue;
	}
	void WriteSetting(const TCHAR* Key, bool bValue)
	{
		if (!GConfig) return;
		GConfig->SetBool(TEXT("Tallow.Settings"), Key, bValue, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	UInputModifierNegate* YFlip(UObject* Outer)
	{
		auto* Flip = NewObject<UInputModifierNegate>(Outer);
		Flip->bX = false; Flip->bY = false; Flip->bZ = false;   // off until ApplyInversion says otherwise
		return Flip;
	}
}

bool UTallowGamepadContext::InvertLook() { return ReadSetting(TEXT("InvertLook")); }
bool UTallowGamepadContext::InvertMove() { return ReadSetting(TEXT("InvertMove")); }
void UTallowGamepadContext::SetInvertLook(bool bInvert) { WriteSetting(TEXT("InvertLook"), bInvert); }
void UTallowGamepadContext::SetInvertMove(bool bInvert) { WriteSetting(TEXT("InvertMove"), bInvert); }
void UTallowGamepadContext::ApplyInversion()
{
	const bool bLook = InvertLook(), bMove = InvertMove();
	// Real DualSense axes (SDL3) report the right stick's Y the other way round, so look is flipped by default and the setting undoes it.
	for (UInputModifierNegate* Flip : LookFlips) if (Flip) Flip->bY = !bLook;
	for (UInputModifierNegate* Flip : MoveFlips) if (Flip) Flip->bY = bMove;
}

const TArray<FTallowPadRow>& UTallowGamepadContext::MainRows()
{
	static const TArray<FTallowPadRow> Rows = {
		R(EKeys::Gamepad_Left2D, TEXT("IA_Move"), TEXT("left stick"), TEXT("move")),
		R(EKeys::Gamepad_Right2D, TEXT("IA_Look"), TEXT("right stick"), TEXT("look")),
		R(EKeys::Gamepad_FaceButton_Bottom, TEXT("IA_Jump"), TEXT("cross"), TEXT("rock the chair")),
		R(EKeys::Gamepad_FaceButton_Left, TEXT("IA_Interact"), TEXT("square"), TEXT("use, pick up, read (hold where the key is held)")),
		R(EKeys::Gamepad_LeftShoulder, TEXT("IA_CupCandle"), TEXT("L1"), TEXT("hold to cup the flame")),
		R(EKeys::Gamepad_RightShoulder, TEXT("IA_Still"), TEXT("R1"), TEXT("hold to hold your breath")),
		R(EKeys::Gamepad_DPad_Up, TEXT("IA_Relight"), TEXT("d-pad up"), TEXT("strike a match, or put the candle out")),
		R(EKeys::Gamepad_DPad_Down, TEXT("IA_Drop"), TEXT("d-pad down"), TEXT("put down what you carry")),
		R(EKeys::Gamepad_LeftThumbstick, TEXT("IA_Sprint"), TEXT("L3"), TEXT("hold to run")),
		R(EKeys::Gamepad_FaceButton_Right, TEXT("IA_Crouch"), TEXT("circle"), TEXT("hold to crouch")),
		R(EKeys::Gamepad_RightTriggerAxis, TEXT("IA_Tension"), TEXT("R2"), TEXT("tension, when picking a lock")),
		R(EKeys::Gamepad_FaceButton_Top, TEXT("IA_TallowInventory"), TEXT("triangle"), TEXT("look through your things")),
		R(EKeys::Gamepad_Special_Left, TEXT("IA_TallowMap"), TEXT("create"), TEXT("unfold your map, once found")),
		R(EKeys::Gamepad_Special_Right, TEXT("IA_TallowPause"), TEXT("options"), TEXT("pause")),
		R(EKeys::Gamepad_DPad_Left, TEXT("IA_TallowReadToggle"), TEXT("d-pad left"), TEXT("start or stop the reading aloud")),
		R(EKeys::Gamepad_DPad_Right, TEXT("IA_TallowReadSkip"), TEXT("d-pad right"), TEXT("skip the reading aloud")),
	};
	return Rows;
}
const TArray<FTallowPadRow>& UTallowGamepadContext::LockpickRows()
{
	static const TArray<FTallowPadRow> Rows = {
		R(EKeys::Gamepad_Right2D, TEXT("IA_Look"), TEXT("right stick"), TEXT("move the pick (while picking a lock)")),
		R(EKeys::Gamepad_FaceButton_Right, TEXT("IA_TallowStopLockpick"), TEXT("circle"), TEXT("put the picks away (while picking a lock)")),
	};
	return Rows;
}
const TArray<FTallowPadRow>& UTallowGamepadContext::SkipRows()
{
	static const TArray<FTallowPadRow> Rows = {
		R(EKeys::Gamepad_FaceButton_Bottom, TEXT("IA_TallowSkip"), TEXT("cross"), TEXT("skip the slides and the intro (only while they show)")),
	};
	return Rows;
}

UInputAction* UTallowGamepadContext::Resolve(FName Name)
{
	if (TObjectPtr<UInputAction>* Found = Actions.Find(Name)) return Found->Get();
	UInputAction* Action = nullptr;
	if (IsRuntimeAction(Name))
	{
		Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = EInputActionValueType::Boolean;
		// Pause, skip and the reading controls also work with the game paused.
		Action->bTriggerWhenPaused = Name == TEXT("IA_TallowPause") || Name == TEXT("IA_TallowSkip") || Name == TEXT("IA_TallowReadToggle") || Name == TEXT("IA_TallowReadSkip");
	}
	else Action = LoadObject<UInputAction>(nullptr, *FString::Printf(TEXT("/Game/Input/Actions/%s.%s"), *Name.ToString(), *Name.ToString()));
	if (Action) Actions.Add(Name, Action);
	return Action;
}

void UTallowGamepadContext::Fill(UInputMappingContext* Context, const TArray<FTallowPadRow>& Rows, float YawScale, float PitchScale)
{
	const bool bLockpick = Context == Lockpick;
	for (const FTallowPadRow& Row : Rows)
	{
		UInputAction* Action = Resolve(Row.Action);
		if (!Action) continue;
		FEnhancedActionKeyMapping& Map = Context->MapKey(Action, Row.Key);
		if (Row.Key == EKeys::Gamepad_Left2D)
		{
			auto* Dead = NewObject<UInputModifierDeadZone>(Context);
			Dead->Type = EDeadZoneType::Radial; Dead->LowerThreshold = .18f; Dead->UpperThreshold = 1.f;
			UInputModifierNegate* Flip = YFlip(Context);
			MoveFlips.Add(Flip);
			Map.Modifiers.Add(Flip); Map.Modifiers.Add(Dead);
		}
		else if (Row.Key == EKeys::Gamepad_Right2D)
		{
			auto* Dead = NewObject<UInputModifierDeadZone>(Context);
			Dead->Type = EDeadZoneType::Radial; Dead->LowerThreshold = .15f; Dead->UpperThreshold = 1.f;
			auto* Curve = NewObject<UInputModifierResponseCurveExponential>(Context);
			Curve->CurveExponent = FVector(1.6, 1.6, 1.0);
			Map.Modifiers.Add(Dead); Map.Modifiers.Add(Curve);
			if (bLockpick)
			{
				// The character feeds the pick X*.04 and -Y*.04 each frame, so a per-second rate times delta time gives the pace.
				auto* Flip = NewObject<UInputModifierNegate>(Context);   // stick up lifts the pick, as mouse up does
				Flip->bX = false; Flip->bY = true; Flip->bZ = false;
				auto* Time = NewObject<UInputModifierScaleByDeltaTime>(Context);
				auto* Rate = NewObject<UInputModifierScalar>(Context);
				Rate->Scalar = FVector(PickDepthRate, PickLiftRate, 1.0);
				Map.Modifiers.Add(Flip); Map.Modifiers.Add(Time); Map.Modifiers.Add(Rate);
			}
			else
			{
				// Degrees per second at full tilt. The controller's own legacy look scale is divided out (its sign is
				// the pitch direction: stick up looks up).
				UInputModifierNegate* Flip = YFlip(Context);   // the right stick's real axis sign, and the player's invert-look setting
				LookFlips.Add(Flip);
				Map.Modifiers.Insert(Flip, 0);
				auto* Time = NewObject<UInputModifierScaleByDeltaTime>(Context);
				auto* Rate = NewObject<UInputModifierScalar>(Context);
				Rate->Scalar = FVector(LookYawRate / YawScale, LookPitchRate / PitchScale, 1.0);
				Map.Modifiers.Add(Time); Map.Modifiers.Add(Rate);
			}
		}
		else if (Row.Key == EKeys::Gamepad_RightTriggerAxis)
		{
			auto* Dead = NewObject<UInputModifierDeadZone>(Context);
			Dead->Type = EDeadZoneType::Axial; Dead->LowerThreshold = .35f; Dead->UpperThreshold = 1.f;
			Map.Modifiers.Add(Dead);
		}
	}
}

void UTallowGamepadContext::Build(float YawScale, float PitchScale)
{
	if (FMath::IsNearlyZero(YawScale)) YawScale = 1.f;
	if (FMath::IsNearlyZero(PitchScale)) PitchScale = 1.f;
	Main = NewObject<UInputMappingContext>(this, TEXT("IMC_TallowPad"));
	Lockpick = NewObject<UInputMappingContext>(this, TEXT("IMC_TallowPadLockpick"));
	Skip = NewObject<UInputMappingContext>(this, TEXT("IMC_TallowPadSkip"));
	Fill(Main, MainRows(), YawScale, PitchScale);
	Fill(Lockpick, LockpickRows(), YawScale, PitchScale);
	Fill(Skip, SkipRows(), YawScale, PitchScale);
	ApplyInversion();
}
