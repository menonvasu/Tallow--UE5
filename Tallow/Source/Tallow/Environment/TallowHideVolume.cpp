#include "Environment/TallowHideVolume.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/TallowCharacter.h"
#include "Interactable/TallowInteractionComponent.h"
#include "GameMode/TallowGameMode.h"
#include "Engine/World.h"

ATallowHideVolume::ATallowHideVolume()
{
 PrimaryActorTick.bCanEverTick = true;
 PrimaryActorTick.bStartWithTickEnabled = false;
 PrimaryActorTick.TickGroup = TG_PostUpdateWork;
 HideBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Hide Box"));
 HideBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 HideBox->SetCollisionResponseToAllChannels(ECR_Ignore);
 HideBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
 HideBox->SetCanEverAffectNavigation(false);
 RootComponent = HideBox;
 HidingViewPoint = CreateDefaultSubobject<USceneComponent>(TEXT("Hiding View Point"));
 HidingViewPoint->SetupAttachment(RootComponent);
 ExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("Exit Point"));
 ExitPoint->SetupAttachment(RootComponent);
 ExitPoint->SetRelativeLocation(FVector(100.f, 0.f, 0.f));
}

bool ATallowHideVolume::CanInteract_Implementation(AActor* Interactor) const
{
 const auto* Player = Cast<ATallowCharacter>(Interactor);
 if (!Player || Player->IsTiedToChair()) return false;
 if (const auto* GM = GetWorld()->GetAuthGameMode<ATallowGameMode>(); GM && GM->IsCaptureInProgress()) return false;
 return bIsHidden ? CachedPC.IsValid() && CachedPC->GetPawn() == Player : !Player->IsHiddenInSpot();
}

void ATallowHideVolume::OnInteract_Implementation(AActor* Interactor)
{
 if (!CanInteract_Implementation(Interactor)) return;
 auto* Player = Cast<ATallowCharacter>(Interactor);
 auto* PC = Cast<APlayerController>(Player->GetController());
 if (bIsHidden) ExitHiddenSpot(PC); else EnterHiddenSpot(PC);
}

FText ATallowHideVolume::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
 return FText::FromString(bIsHidden ? TEXT("come out") : TEXT("hide"));
}

void ATallowHideVolume::EnterHiddenSpot(APlayerController* PC)
{
 auto* Player = PC ? Cast<ATallowCharacter>(PC->GetPawn()) : nullptr;
 if (!Player || !CanInteract_Implementation(Player)) return;
 Player->PrepareForHiding();
 CachedPC = PC;
 EntryTransform = Player->GetActorTransform();
 SavedCollision = Player->GetCapsuleComponent()->GetCollisionEnabled();
 SavedMovementMode = Player->GetCharacterMovement()->MovementMode;
 bSavedControllerYaw = Player->bUseControllerRotationYaw;
 FRotator Rotation = HidingViewPoint->GetComponentRotation();
 // A sofa back can fill an old authored view after furnishing changes. Face the clear
 // lane around its arm when that happens, so hiding still gives her a view out.
 const FVector Eye=HidingViewPoint->GetComponentLocation();
 FCollisionQueryParams ViewQuery(SCENE_QUERY_STAT(HidePeekView),false,Player);ViewQuery.AddIgnoredActor(this);
 FHitResult ViewHit;
 if(GetWorld()->LineTraceSingleByChannel(ViewHit,Eye,Eye+Rotation.Vector()*90.f,ECC_Visibility,ViewQuery))
 {
  float Best=-MAX_flt;const float AuthoredYaw=Rotation.Yaw;const FVector ToExit=(ExitPoint->GetComponentLocation()-Eye).GetSafeNormal2D();
  for(float Angle:{-90.f,-60.f,-30.f,0.f,30.f,60.f,90.f})
  {
   const FRotator Candidate(0.f,AuthoredYaw+Angle,0.f);FHitResult Hit;
   const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+Candidate.Vector()*240.f,ECC_Visibility,ViewQuery);
   const float Clear=Blocked?Hit.Distance:240.f;
   const float Score=Clear+FVector::DotProduct(Candidate.Vector(),ToExit)*30.f-FMath::Abs(Angle)*.08f;
   if(Score>Best){Best=Score;Rotation=Candidate;}
  }
 }
 if (auto* Camera = PC->PlayerCameraManager.Get())
 {
  CachedPitchMin = Camera->ViewPitchMin; CachedPitchMax = Camera->ViewPitchMax;
  CachedYawMin = Camera->ViewYawMin; CachedYawMax = Camera->ViewYawMax;
  Camera->ViewPitchMin = Rotation.Pitch - 25.f; Camera->ViewPitchMax = Rotation.Pitch + 25.f;
  Camera->ViewYawMin = Rotation.Yaw - 40.f; Camera->ViewYawMax = Rotation.Yaw + 40.f;
 }
 Player->SetCurrentHideVolume(this);
 Player->GetCharacterMovement()->StopMovementImmediately();
 Player->GetCharacterMovement()->DisableMovement();
 Player->bUseControllerRotationYaw = false;
 Player->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Player->SetActorRotation(Rotation);
 PC->SetControlRotation(Rotation);
 bIsHidden = true;
 AddTickPrerequisiteActor(Player);
 SetActorTickEnabled(true);
 Tick(0.f);
 OnHide.Broadcast(true);
}

void ATallowHideVolume::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 if (!bIsHidden || !CachedPC.IsValid()) return;
 auto* Player = Cast<ATallowCharacter>(CachedPC->GetPawn());
 if (!Player || Player->GetCurrentHideVolume() != this) { LeaveHidingSpot(); return; }
 // The full eye offset includes camera sockets, crouching and the breathing pose.
 // Anchor after the character's tick so those systems cannot lift her through the furniture.
 const FVector Correction = HidingViewPoint->GetComponentLocation() - Player->GetFirstPersonCameraComponent()->GetComponentLocation();
 Player->SetActorLocation(Player->GetActorLocation() + Correction, false, nullptr, ETeleportType::TeleportPhysics);
}

void ATallowHideVolume::ExitHiddenSpot(APlayerController* PC)
{
 if (!bIsHidden || PC != CachedPC.Get()) return;
 auto* Player = PC ? Cast<ATallowCharacter>(PC->GetPawn()) : nullptr;
 if (!Player) return;
 // Test the whole capsule, including headroom. A blocked authored exit falls back to where she entered.
 FCollisionQueryParams Query(SCENE_QUERY_STAT(TallowHideExit), false, Player);
 Query.AddIgnoredActor(this);
 const auto* Capsule = Player->GetCapsuleComponent();
 const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
 FVector Exit = ExitPoint->GetComponentLocation();
 const auto Clear = [&](const FVector& At) { return !GetWorld()->OverlapBlockingTestByChannel(At, FQuat::Identity, ECC_Pawn, Shape, Query); };
 if (!Clear(Exit)) { Exit = EntryTransform.GetLocation(); if (!bForceExit && !Clear(Exit)) return; }
 SetActorTickEnabled(false);
 RemoveTickPrerequisiteActor(Player);
 if (auto* Camera = PC->PlayerCameraManager.Get())
 {
  Camera->ViewPitchMin = CachedPitchMin; Camera->ViewPitchMax = CachedPitchMax;
  Camera->ViewYawMin = CachedYawMin; Camera->ViewYawMax = CachedYawMax;
 }
 Player->SetCurrentHideVolume(nullptr);
 Player->SetActorLocation(Exit, false, nullptr, ETeleportType::TeleportPhysics);
 Player->bUseControllerRotationYaw = bSavedControllerYaw;
 Player->GetCapsuleComponent()->SetCollisionEnabled(SavedCollision);
 Player->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(SavedMovementMode));
 bIsHidden = false;
 CachedPC.Reset();
 OnHide.Broadcast(false);
}

void ATallowHideVolume::LeaveHidingSpot(bool bForce)
{
 bForceExit = bForce;
 ExitHiddenSpot(CachedPC.Get());
 bForceExit = false;
}

void ATallowHideVolume::ReleaseForCapture()
{
 if (!bIsHidden || !CachedPC.IsValid()) return;
 auto* Player = Cast<ATallowCharacter>(CachedPC->GetPawn());
 if (!Player) return;
 SetActorTickEnabled(false);
 RemoveTickPrerequisiteActor(Player);
 if (auto* Camera = CachedPC->PlayerCameraManager.Get())
 {
  Camera->ViewPitchMin = CachedPitchMin; Camera->ViewPitchMax = CachedPitchMax;
  Camera->ViewYawMin = CachedYawMin; Camera->ViewYawMax = CachedYawMax;
 }
 Player->GetCharacterMovement()->StopMovementImmediately();
 Player->GetCharacterMovement()->DisableMovement();
 Player->SetCurrentHideVolume(nullptr);
 Player->bUseControllerRotationYaw = bSavedControllerYaw;
 Player->GetCapsuleComponent()->SetCollisionEnabled(SavedCollision);
 bIsHidden = false;
 CachedPC.Reset();
 OnHide.Broadcast(false);
}

void ATallowHideVolume::EndPlay(const EEndPlayReason::Type Reason)
{
 if (bIsHidden && CachedPC.IsValid())
 {
  // During teardown an occupied exit cannot prevent restoring the player and camera.
  if (auto* Player = Cast<ATallowCharacter>(CachedPC->GetPawn()))
  {
   Player->SetCurrentHideVolume(nullptr);
   Player->bUseControllerRotationYaw = bSavedControllerYaw;
   Player->GetCapsuleComponent()->SetCollisionEnabled(SavedCollision);
   Player->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(SavedMovementMode));
  }
  if (auto* Camera = CachedPC->PlayerCameraManager.Get())
  {
   Camera->ViewPitchMin = CachedPitchMin; Camera->ViewPitchMax = CachedPitchMax;
   Camera->ViewYawMin = CachedYawMin; Camera->ViewYawMax = CachedYawMax;
  }
 }
 Super::EndPlay(Reason);
}
