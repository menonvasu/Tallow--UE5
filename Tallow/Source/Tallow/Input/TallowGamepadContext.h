#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/Object.h"
#include "TallowGamepadContext.generated.h"

class UInputAction;
class UInputMappingContext;
class UInputModifierNegate;

/** One row of the DualSense layout. Edit the table in TallowGamepadContext.cpp to change a button. */
struct FTallowPadRow
{
	FKey Key;
	FName Action;      // asset name (IA_Jump) or runtime action name (IA_TallowPause)
	const TCHAR* Pad;  // what the button is called, for the layout table
	const TCHAR* Does; // what it does
};

/** Builds the gamepad mapping contexts at runtime; no assets on disk. Three contexts:
 *  Main (priority 1, above IMC_Default), Lockpick (priority 2, only while a lock is open), Skip (priority 3, only while
 *  slides or the intro show, so Cross can skip without also being Jump). */
UCLASS()
class TALLOW_API UTallowGamepadContext : public UObject
{
	GENERATED_BODY()
public:
	static constexpr int32 MainPriority = 1;
	static constexpr int32 LockpickPriority = 2;
	static constexpr int32 SkipPriority = 3;

	/** Degrees per second at full tilt. */
	static constexpr float LookYawRate = 220.f;
	static constexpr float LookPitchRate = 160.f;
	/** Pick units per second at full tilt: depth crosses its 3 pins in 1.2 s, lift its range in about 0.9 s. */
	static constexpr float PickDepthRate = 62.5f;
	static constexpr float PickLiftRate = 28.f;

	static const TArray<FTallowPadRow>& MainRows();
	static const TArray<FTallowPadRow>& LockpickRows();
	static const TArray<FTallowPadRow>& SkipRows();

	/** Pad-only axis flips, kept in the Tallow.Settings section of GameUserSettings (default off). Keyboard and mouse never read them. */
	static bool InvertLook();
	static bool InvertMove();
	static void SetInvertLook(bool bInvert);
	static void SetInvertMove(bool bInvert);
	/** Sets the Y negate on the right stick (on unless invert look is set) and left stick (on only if invert move is set); cheap, safe to call every poll. */
	void ApplyInversion();

	/** Loads the IA_* assets and creates the runtime actions, then fills the three contexts. */
	void Build(float YawScale = 1.f, float PitchScale = 1.f);

	UInputAction* FindAction(FName Name) const { const TObjectPtr<UInputAction>* Found = Actions.Find(Name); return Found ? Found->Get() : nullptr; }

	UPROPERTY() TObjectPtr<UInputMappingContext> Main;
	UPROPERTY() TObjectPtr<UInputMappingContext> Lockpick;
	UPROPERTY() TObjectPtr<UInputMappingContext> Skip;
	UPROPERTY() TMap<FName, TObjectPtr<UInputAction>> Actions;
	UPROPERTY() TArray<TObjectPtr<UInputModifierNegate>> LookFlips;
	UPROPERTY() TArray<TObjectPtr<UInputModifierNegate>> MoveFlips;
private:
	UInputAction* Resolve(FName Name);
	void Fill(UInputMappingContext* Context, const TArray<FTallowPadRow>& Rows, float YawScale, float PitchScale);
};
