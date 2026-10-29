#include "TallowLockpickWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Tasks/TallowLockpickGame.h"
#include "Blueprint/WidgetTree.h"
#include "UI/Presentation/TallowPaper.h"
#include "Input/TallowInputPrompts.h"
#include "Input/TallowInputDeviceSubsystem.h"

void UTallowLockpickWidget::InitializeLockpickWidget(UTallowLockpickGame* InGameComponent)
{
	if (!InGameComponent) { return; }
	LockpickGame = InGameComponent;
	
	PinWidgets.Reset();
	PinWidgets.Add(Pin0);
	PinWidgets.Add(Pin1);
	PinWidgets.Add(Pin2);
	PinWidgets.Add(Pin3);
	
	LockpickGame->OnFalseSet.AddDynamic(this, &UTallowLockpickWidget::HandleOnFalseSet);
	LockpickGame->OnOverset.AddDynamic(this, &UTallowLockpickWidget::HandleOnOverSet);
	LockpickGame->OnSnap.AddDynamic(this, &UTallowLockpickWidget::HandleOnTensionReleasedOrOnSnap);
	LockpickGame->OnTensionReleased.AddDynamic(this, &UTallowLockpickWidget::HandleOnTensionReleasedOrOnSnap);
	LockpickGame->OnPinSet.AddDynamic(this, &UTallowLockpickWidget::HandleOnPinSet);
	LockpickGame->OnLockpickSet.AddDynamic(this, &UTallowLockpickWidget::HandleOnLockpickSet);
}

void UTallowLockpickWidget::NativeTick(const FGeometry& Geometry, float InDeltaTime)
{
	Super::NativeTick(Geometry, InDeltaTime);
	if (!LockpickGame.IsValid() || !PickImage) { return; }

	bool bIsAnyPinOverset = false;
	bool bIsAnyFalseSet = false;
	
	ShakeTime += InDeltaTime;
	const float CurrentTorque = LockpickGame->GetCurrentTorque();
	const int32 BindingPinIndex = LockpickGame->GetCurrentBindingPinIndex();
	const bool bPickOnBinding = BindingPinIndex == FMath::RoundToInt(LockpickGame->GetPickDepth());
	const bool bIsPickMovingUp = (LockpickGame->GetTargetPickLift() - LockpickGame->GetPickLift()) > 0.01f;
	const bool bShouldShake = bPickOnBinding && CurrentTorque > LockpickGame->GetMinTorque() &&bIsPickMovingUp;
	
	ShakeAlpha = FMath::FInterpTo(ShakeAlpha, bShouldShake ? 1.0f : 0.0f, InDeltaTime, ShakeFadeSpeed);
	const float Amp = CurrentTorque * MaxShakePixels * ShakeAlpha;
	const FVector2D Shake(FMath::Sin(ShakeTime * 33.0f) * Amp, FMath::Sin(ShakeTime * 44.0f + 1.3f) * Amp * 0.5f);
	
	// Update pick position
	PickImage->SetRenderTranslation(FVector2D(LockpickGame->GetPickDepth() * PinSpacing + Shake.X,-LockpickGame->GetPickLift() * MaxLiftPixels + Shake.Y));
	
	// Offset pin by change and update color
	const FLinearColor Gray(0.3f, 0.3f, 0.3f);
	const TArray<FPinChamber>& PinChambers = LockpickGame->GetPinChambers();
	for (int i = 0; i < PinWidgets.Num(); i++)
	{
		FVector2D PinOffset(0.0f, -(PinChambers[i].CurrentHeight * MaxLiftPixels));
		PinWidgets[i]->SetRenderTranslation(PinOffset);
		
		switch (PinChambers[i].PinState)
		{
		case EPinState::Free:
			PinWidgets[i]->SetBrushColor(Gray);
			break;
		case EPinState::Binding:
			{
				FLinearColor Color = Gray;
				if (i == BindingPinIndex)
				{
					// fades from gray to white as dwell fills
					Color = FMath::Lerp(Gray, FLinearColor::White, LockpickGame->GetSetDwellProgress());
				}
				PinWidgets[i]->SetBrushColor(Color);
				break;
			}
		case EPinState::Set:
			PinWidgets[i]->SetBrushColor(FLinearColor(0.2f, 0.95f, 0.35f));
			break;
		case EPinState::Overset:
			PinWidgets[i]->SetBrushColor(FLinearColor(0.95f, 0.15f, 0.12f));
			bIsAnyPinOverset = true;
			break;
		case EPinState::FalseSet:
			PinWidgets[i]->SetBrushColor(FLinearColor(0.95f, 0.69f, 0.28f));
			bIsAnyFalseSet = true;
			break;
		}
	}
	
	if (TensionBar)
	{
		TensionBar->SetPercent(CurrentTorque);
		
		FLinearColor BarColor;
		if (CurrentTorque >= 0.75f && bIsAnyPinOverset)
		{
			BarColor = FLinearColor(0.95f, 0.15f, 0.12f); 
		}
		else if (bIsAnyFalseSet)
		{
			BarColor = FLinearColor(0.95f, 0.69f, 0.28f);
		}
		else if (CurrentTorque < LockpickGame->GetMinTorque())
		{
			BarColor = Gray;                                
		}
		else
		{
			BarColor = FLinearColor(0.2f, 0.95f, 0.35f);    
		}
		
		TensionBar->SetFillColorAndOpacity(BarColor);
		
		const bool bBarDanger = bIsAnyPinOverset && CurrentTorque >= 0.75f;
		BarShakeAlpha = FMath::FInterpTo(BarShakeAlpha, bBarDanger ? 1.0f : 0.0f, InDeltaTime, ShakeFadeSpeed);
		const float BarAmp = MaxBarShakePixels * BarShakeAlpha;
		TensionBar->SetRenderTranslation(FVector2D(FMath::Sin(ShakeTime * 40.0f) * BarAmp, 0.0f));
	}
	
	// Status check
	StatusTimeRemaining = FMath::Max(0.0f, StatusTimeRemaining - InDeltaTime);
	if (EventFeedbackText)
	{
		EventFeedbackText->SetOpacity(FMath::Clamp(StatusTimeRemaining / 0.5f, 0.0f, 1.0f));
	}
	
	if (!HintFeedbackText) { return; }
	switch (LockpickGame->GetHint())
	{
	case ELockpickHint::None:
		HintFeedbackText->SetText(FText::FromString(""));
		break;
	case ELockpickHint::ApplyTension:
		HintFeedbackText->SetText(FText::FromString("Hold LMB"));
		break;
	case ELockpickHint::EaseTension:
		HintFeedbackText->SetText(FText::FromString("Ease off the tension"));
		break;
	case ELockpickHint::FindBindingPin:
		HintFeedbackText->SetText(FText::FromString("Find stiff pin"));
		break;
	case ELockpickHint::LowerPick:
		HintFeedbackText->SetText(FText::FromString("Lower the pick"));
	}
}
	
void UTallowLockpickWidget::NativeDestruct()
{
	if (LockpickGame.IsValid())
	{
		LockpickGame->OnFalseSet.RemoveDynamic(this, &UTallowLockpickWidget::HandleOnFalseSet);
		LockpickGame->OnOverset.RemoveDynamic(this, &UTallowLockpickWidget::HandleOnOverSet);
		LockpickGame->OnSnap.RemoveDynamic(this, &UTallowLockpickWidget::HandleOnTensionReleasedOrOnSnap);
		LockpickGame->OnTensionReleased.RemoveDynamic(this, &UTallowLockpickWidget::HandleOnTensionReleasedOrOnSnap);
		LockpickGame->OnPinSet.RemoveDynamic(this, &UTallowLockpickWidget::HandleOnPinSet);
		LockpickGame->OnLockpickSet.RemoveDynamic(this, &UTallowLockpickWidget::HandleOnLockpickSet);
	}
	
	Super::NativeDestruct();
}

void UTallowLockpickWidget::ShowStatus(const FText& Message)
{
	if (EventFeedbackText)
	{
		EventFeedbackText->SetText(Message);
		EventFeedbackText->SetOpacity(1.0f);
		StatusTimeRemaining = 2.0f;
	}
}

//---------------------------------------------------------------------------
//All status handlers

void UTallowLockpickWidget::HandleOnFalseSet()
{
	ShowStatus(FText::FromString("Something feels off"));
}
void UTallowLockpickWidget::HandleOnTensionReleasedOrOnSnap()
{
	ShowStatus(FText::FromString("Lock reset"));
}

void UTallowLockpickWidget::HandleOnOverSet()
{
	ShowStatus(FText::FromString("The pick is straining"));
}

void UTallowLockpickWidget::HandleOnPinSet()
{
	ShowStatus(FText::FromString("Yes!"));
}

void UTallowLockpickWidget::HandleOnLockpickSet()
{
	ShowStatus(FText::FromString("Nice!"));
}
