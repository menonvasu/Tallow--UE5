#include "Presentation/TallowEscapePoint.h"
#include "Presentation/TallowPlayDirector.h"
#include "Player/TallowCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "Tasks/TallowProjector.h"
#include "Kismet/GameplayStatics.h"
#include "UI/TallowKeypadWidget.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "GameMode/TallowGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sound/SoundBase.h"

ATallowEscapePoint::ATallowEscapePoint()
{
 Latch=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EscapeLatch"));
 RootComponent=Latch;
 Latch->SetCollisionProfileName(TEXT("BlockAll"));
 ReleaseLeverMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReleaseLeverMesh"));
 ReleaseLeverMesh->SetupAttachment(RootComponent);
 ReleaseLeverMesh->SetRelativeLocation(FVector(6.5f,7.5f,8.f));
 ReleaseLeverMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 KeypadAudio=CreateDefaultSubobject<UAudioComponent>(TEXT("KeypadAudio"));
 KeypadAudio->SetupAttachment(RootComponent);
 KeypadAudio->bAutoActivate=false;
 KeypadAudio->bAllowSpatialization=true;
 KeypadAudio->bOverrideAttenuation=true;
 KeypadAudio->AttenuationOverrides.bAttenuate=true;
 KeypadAudio->AttenuationOverrides.bSpatialize=true;
 KeypadAudio->AttenuationOverrides.AttenuationShapeExtents=FVector(80.f);
 KeypadAudio->AttenuationOverrides.FalloffDistance=1200.f;
 KeypadWidgetClass=UTallowKeypadWidget::StaticClass();
}

void ATallowEscapePoint::BeginPlay()
{
 Super::BeginPlay();
 if (!LinkedProjector) LinkedProjector=Cast<ATallowProjector>(UGameplayStatics::GetActorOfClass(this,ATallowProjector::StaticClass()));
 if (!KeypadWidgetClass) KeypadWidgetClass=UTallowKeypadWidget::StaticClass();
 if (!Latch->GetStaticMesh()) Latch->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/ProjectorFinal/SM_ExitLatch.SM_ExitLatch")));
 if (!ReleaseLeverMesh->GetStaticMesh()) ReleaseLeverMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/ProjectorFinal/SM_ExitLever.SM_ExitLever")));
 if (!ErrorSound) ErrorSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/Projector/S_Keypad_Error.S_Keypad_Error"));
 if (!AcceptSound) AcceptSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/Projector/S_Keypad_Accept.S_Keypad_Accept"));
 LeverRestRotation=ReleaseLeverMesh->GetRelativeRotation();
 if (auto* GM=GetWorld()->GetAuthGameMode<ATallowGameMode>()) GM->OnCaught.AddUniqueDynamic(this,&ATallowEscapePoint::HandlePlayerCaptured);
}

void ATallowEscapePoint::EndPlay(const EEndPlayReason::Type Reason)
{
 if (auto* GM=GetWorld()->GetAuthGameMode<ATallowGameMode>()) GM->OnCaught.RemoveDynamic(this,&ATallowEscapePoint::HandlePlayerCaptured);
 CloseKeypad();
 Super::EndPlay(Reason);
}

void ATallowEscapePoint::HandlePlayerCaptured()
{
 SetCodeVerified(false);
}

void ATallowEscapePoint::SetCodeVerified(bool bNewState)
{
 // Blueprint reset cannot bypass watching the film.
 bCodeVerified=bNewState && LinkedProjector && LinkedProjector->HasCompletedProjection();
 if (!bNewState)
 {
  CloseKeypad();
  ReleaseLeverMesh->SetRelativeRotation(LeverRestRotation);
 }
}

void ATallowEscapePoint::CloseKeypad()
{
 UTallowKeypadWidget* Widget=ActiveWidgetInstance;
 ActiveWidgetInstance=nullptr;
 if (bIsCurrentlyInputtingKeypad)
 {
  bIsCurrentlyInputtingKeypad=false;
  if (APlayerController* PC=CachedPC.Get())
  {
   PC->SetIgnoreMoveInput(false);
   PC->SetIgnoreLookInput(false);
   PC->SetInputMode(FInputModeGameOnly());
   PC->bShowMouseCursor=bHadCursor;
   PC->FlushPressedKeys();
  }
 }
 CachedPC.Reset();
 if (IsValid(Widget)) Widget->RemoveFromParent();
}

bool ATallowEscapePoint::IsReleasePowered() const
{
 const auto* Director=Cast<ATallowPlayDirector>(UGameplayStatics::GetActorOfClass(this,ATallowPlayDirector::StaticClass()));
 return Director && Director->CanApproachExit();
}

bool ATallowEscapePoint::CanInteract_Implementation(AActor* Interactor) const
{
 const auto* Player=Cast<ATallowCharacter>(Interactor);
 // Held interactions themselves make CanChangeInventory false; keep a lever pull valid through its whole hold.
 const auto* GM=GetWorld()->GetAuthGameMode<ATallowGameMode>();
 return Player && !Player->IsTiedToChair() && !Player->IsHiddenInSpot() && !Player->IsHandOccupied()
  && !(GM && GM->IsCaptureInProgress()) && IsReleasePowered() && LinkedProjector && LinkedProjector->HasCompletedProjection()
  && !bIsCurrentlyInputtingKeypad;
}

float ATallowEscapePoint::GetInteractionDuration_Implementation() const { return bCodeVerified?1.4f:0.f; }

FText ATallowEscapePoint::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
 if (!IsReleasePowered()) return FText::FromString(TEXT("THE RELEASE NEEDS POWER AND A CLEAR WAY OUT"));
 if (!LinkedProjector || !LinkedProjector->HasCompletedProjection()) return FText::FromString(TEXT("THE FILM HOLDS THE ACCESS CODE"));
 return FText::FromString(bCodeVerified?TEXT("HOLD E — RELEASE THE LATCH"):TEXT("ENTER THE FIVE FILM DIGITS"));
}

void ATallowEscapePoint::OnInteract_Implementation(AActor* Interactor)
{
 if (!CanInteract_Implementation(Interactor) || bCodeVerified) return;
 auto* Player=Cast<ATallowCharacter>(Interactor);
 auto* PC=Cast<APlayerController>(Player->GetController());
 if (!PC || !KeypadWidgetClass) return;
 ActiveWidgetInstance=CreateWidget<UTallowKeypadWidget>(PC,KeypadWidgetClass);
 if (!ActiveWidgetInstance) return;
 Player->PrepareForInventory();
 PC->FlushPressedKeys();
 CachedPC=PC;
 bIsCurrentlyInputtingKeypad=true;
 bHadCursor=PC->bShowMouseCursor;
 PC->SetIgnoreMoveInput(true);
 PC->SetIgnoreLookInput(true);
 ActiveWidgetInstance->InitializeKeypad(this);
 ActiveWidgetInstance->AddToViewport(45);
 FInputModeUIOnly Input;
 Input.SetWidgetToFocus(ActiveWidgetInstance->TakeWidget());
 PC->SetInputMode(Input);
 PC->bShowMouseCursor=true;
 ActiveWidgetInstance->SetKeyboardFocus();
}

void ATallowEscapePoint::OnInteractStart_Implementation(AActor* Interactor)
{
 ReleaseLeverMesh->SetRelativeRotation(LeverRestRotation);
}
void ATallowEscapePoint::OnInteractTick_Implementation(AActor* Interactor,float DeltaTime,float Progress)
{
 if (!bCodeVerified || !CanInteract_Implementation(Interactor)) { OnInteractCancel_Implementation(Interactor); return; }
 FRotator Pose=LeverRestRotation;
 Pose.Roll+=25.f*FMath::SmoothStep(0.f,1.f,Progress);
 ReleaseLeverMesh->SetRelativeRotation(Pose);
}
void ATallowEscapePoint::OnInteractCancel_Implementation(AActor* Interactor)
{
 ReleaseLeverMesh->SetRelativeRotation(LeverRestRotation);
}
void ATallowEscapePoint::OnInteractComplete_Implementation(AActor* Interactor)
{
 if (!bCodeVerified || !CanInteract_Implementation(Interactor)) { OnInteractCancel_Implementation(Interactor); return; }
 FRotator Pose=LeverRestRotation;
 Pose.Roll+=25.f;
 ReleaseLeverMesh->SetRelativeRotation(Pose);
 if (auto* Director=Cast<ATallowPlayDirector>(UGameplayStatics::GetActorOfClass(this,ATallowPlayDirector::StaticClass()))) Director->FinishRun();
}

bool ATallowEscapePoint::CheckSubmittedCode(const TArray<int32>& EnteredCode)
{
 if (!bIsCurrentlyInputtingKeypad || !IsReleasePowered() || !LinkedProjector || !LinkedProjector->HasCompletedProjection()) return false;
 if (EnteredCode.Num()==5 && EnteredCode==LinkedProjector->GetCodes())
 {
  bCodeVerified=true;
  KeypadAudio->SetSound(AcceptSound);
  if (AcceptSound) KeypadAudio->Play();
  CloseKeypad();
  OnCorrectAccessCode.Broadcast();
  return true;
 }
 KeypadAudio->SetSound(ErrorSound);
 if (ErrorSound) KeypadAudio->Play();
 if (auto* Noise=GetWorld()->GetSubsystem<UTallowNoiseSubsystem>()) Noise->EmitCustomNoise(GetActorLocation(),CachedPC.IsValid()?CachedPC->GetPawn():nullptr,InputFailedLoudness,InputFailedRange,true);
 OnWrongAccessCode.Broadcast();
 return false;
}
