#include "TallowLockpickWidget.h"

#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Tasks/TallowLockpickGame.h"

void UTallowLockpickWidget::InitializeLockpickWidget(UTallowLockpickGame* InGameComponent)
{
	if (!InGameComponent) { return; }
	LockpickGame = InGameComponent;
	
	PinWidgets.Reset();
	PinWidgets.Add(Pin0);
	PinWidgets.Add(Pin1);
	PinWidgets.Add(Pin2);
	PinWidgets.Add(Pin3);
}


void UTallowLockpickWidget::NativeTick(const FGeometry& Geometry, float InDeltaTime)
{
	Super::NativeTick(Geometry, InDeltaTime);
	if (!LockpickGame.IsValid() || !PickImage) { return; }

	bool bIsAnyPinOverset = false;
	
	// Update pick position
	PickImage->SetRenderTranslation(FVector2D(LockpickGame->GetPickDepth() * PinSpacing,-LockpickGame->GetPickLift() * MaxLiftPixels));
	
	// Offset pin by change and update color
	const TArray<FPinChamber>& PinChambers = LockpickGame->GetPinChambers();
	for (int i = 0; i < PinWidgets.Num(); i++)
	{
		const float OffsetY = -(PinChambers[i].CurrentHeight * MaxLiftPixels);
		PinWidgets[i]->SetRenderTranslation(FVector2D(0, OffsetY));
		
		switch (PinChambers[i].PinState)
		{
		case EPinState::Free:
			PinWidgets[i]->SetBrushColor(FLinearColor(0.3f, 0.3f, 0.3f));
			break;
		case EPinState::Binding:
			PinWidgets[i]->SetBrushColor(FLinearColor(1.0f, 1.0f, 1.0f));
			break;
		case EPinState::Set:
			PinWidgets[i]->SetBrushColor(FLinearColor(0.2f, 0.95f, 0.35f));
			break;
		case EPinState::Overset:
			PinWidgets[i]->SetBrushColor(FLinearColor(0.95f, 0.15f, 0.12f));
			bIsAnyPinOverset = true;
			break;
		case EPinState::FalseSet:
			PinWidgets[i]->SetBrushColor(FLinearColor(0.95f, 0.69f, 0.28f));
			break;
		}
	}
	
	if (TensionBar)
	{
		const float CurrentTorque = LockpickGame->GetCurrentTorque();
		TensionBar->SetPercent(CurrentTorque);
		
		if (CurrentTorque >= 0.75f && bIsAnyPinOverset)
		{
			TensionBar->SetFillColorAndOpacity(FLinearColor(0.95f, 0.15f, 0.12f));
		}
		else
		{
			TensionBar->SetFillColorAndOpacity(FLinearColor(0.2f, 0.95f, 0.35f));
		}
	}
}


