#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"

/** A reversible presentation-only lease. Never changes world FOV or prop transforms. */
struct FTallowFirstPersonProjectionState
{
	FTallowFirstPersonProjectionState() = default;
	FTallowFirstPersonProjectionState(const FTallowFirstPersonProjectionState&) = delete;
	FTallowFirstPersonProjectionState& operator=(const FTallowFirstPersonProjectionState&) = delete;
	~FTallowFirstPersonProjectionState() { Restore(); }

	void Apply(UCameraComponent* InCamera, UPrimitiveComponent* Candle, UPrimitiveComponent* Flame, float FOV, float Scale)
	{
		Restore();
		Camera = InCamera;
		OldFOV = InCamera->FirstPersonFieldOfView;
		OldScale = InCamera->FirstPersonScale;
		bOldFOVEnabled = InCamera->bEnableFirstPersonFieldOfView;
		bOldScaleEnabled = InCamera->bEnableFirstPersonScale;
		AppliedFOV = FOV; AppliedScale = Scale;
		InCamera->SetEnableFirstPersonFieldOfView(true);
		InCamera->SetEnableFirstPersonScale(true);
		InCamera->SetFirstPersonFieldOfView(FOV);
		InCamera->SetFirstPersonScale(Scale);
		for (UPrimitiveComponent* Primitive : { Candle, Flame })
		{
			FPrimitiveState& State = Primitives.AddDefaulted_GetRef();
			State.Primitive = Primitive;
			State.Previous = Primitive->FirstPersonPrimitiveType;
			Primitive->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
		}
	}

	void Restore()
	{
		if (UCameraComponent* View = Camera.Get())
		{
			// Do not undo a newer explicit caller adjustment made while this lease was active.
			if (FMath::IsNearlyEqual(View->FirstPersonFieldOfView, AppliedFOV))
			{
				View->SetFirstPersonFieldOfView(OldFOV);
				if (View->bEnableFirstPersonFieldOfView) View->SetEnableFirstPersonFieldOfView(bOldFOVEnabled);
			}
			if (FMath::IsNearlyEqual(View->FirstPersonScale, AppliedScale))
			{
				View->SetFirstPersonScale(OldScale);
				if (View->bEnableFirstPersonScale) View->SetEnableFirstPersonScale(bOldScaleEnabled);
			}
		}
		for (const FPrimitiveState& State : Primitives)
		{
			if (UPrimitiveComponent* Primitive = State.Primitive.Get())
			{
				if (Primitive->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::FirstPerson)
					Primitive->SetFirstPersonPrimitiveType(State.Previous);
			}
		}
		Camera.Reset(); Primitives.Reset();
	}

private:
	struct FPrimitiveState
	{
		TWeakObjectPtr<UPrimitiveComponent> Primitive;
		EFirstPersonPrimitiveType Previous = EFirstPersonPrimitiveType::None;
	};
	TWeakObjectPtr<UCameraComponent> Camera;
	TArray<FPrimitiveState> Primitives;
	float OldFOV = 0.f, OldScale = 1.f, AppliedFOV = 0.f, AppliedScale = 1.f;
	bool bOldFOVEnabled = false, bOldScaleEnabled = false;
};
