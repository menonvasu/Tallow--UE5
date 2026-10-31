// Copyright Epic Games, Inc. All Rights Reserved.


#include "TallowCameraManager.h"

ATallowCameraManager::ATallowCameraManager()
{
	// set the min/max pitch
	ViewPitchMin = -70.0f;
	ViewPitchMax = 80.0f;
}

void ATallowCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	// The engine clears the blend cache here every update, so this adds exactly one entry a frame. Override-order
	// entries are blended after every volume and the view target's own settings (ULocalPlayer::CalcSceneView), so no
	// map volume or camera can bring a vignette back.
	Super::ApplyCameraModifiers(DeltaTime, InOutPOV);
	static FPostProcessSettings NoVignette = []
	{
		FPostProcessSettings Settings;
		Settings.bOverride_VignetteIntensity = true; Settings.VignetteIntensity = 0.f;
		return Settings;
	}();
	AddCachedPPBlend(NoVignette, 1.f, VTBlendOrder_Override);
}

