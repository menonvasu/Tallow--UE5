// Copyright Epic Games, Inc. All Rights Reserved.


#include "TallowPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "TallowCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "Tallow.h"
#include "Widgets/Input/SVirtualJoystick.h"

ATallowPlayerController::ATallowPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = ATallowCameraManager::StaticClass();
}

void ATallowPlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void ATallowPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}
		}
	}
}

