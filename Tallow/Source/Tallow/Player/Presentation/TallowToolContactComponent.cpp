#include "Player/Presentation/TallowToolContactComponent.h"
#include "Player/Presentation/TallowPlayerPresentationComponent.h"
#include "Player/TallowCharacter.h"
#include "Interactable/TallowInteractionComponent.h"
#include "Tasks/TallowPlank.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"

UTallowToolContactComponent::UTallowToolContactComponent()
{
 PrimaryComponentTick.bCanEverTick = true;
 PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UTallowToolContactComponent::Setup(ATallowCharacter* InPlayer, UTallowPlayerPresentationComponent* InPresentation, UStaticMeshComponent* InTool)
{
 Player=InPlayer; Presentation=InPresentation; Tool=InTool;
 if (!InPlayer || !InPresentation) return;
 Interaction=InPlayer->FindComponentByClass<UTallowInteractionComponent>();
 AddTickPrerequisiteActor(InPlayer);
 if (auto* Hands=InPresentation->GetPresentationHands()) AddTickPrerequisiteComponent(Hands);
}

FVector UTallowToolContactComponent::ProjectPoint(const FVector& Point, bool bInverse) const
{
 if (!Player.IsValid()) return Point;
 const auto* Camera=Player->GetFirstPersonCameraComponent();
 if (!Camera) return Point;
 const auto* PC=Cast<APlayerController>(Player->GetController());
 const FTransform View(PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation() : Camera->GetComponentRotation(),
  PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : Camera->GetComponentLocation());
 const float WorldFOV=PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetFOVAngle() : Camera->FieldOfView;
 const float Correction=Camera->bEnableFirstPersonFieldOfView
  ? FMath::Tan(FMath::DegreesToRadians(WorldFOV*.5f))/FMath::Tan(FMath::DegreesToRadians(Camera->FirstPersonFieldOfView*.5f)) : 1.f;
 const float Scale=Camera->bEnableFirstPersonScale ? FMath::Max(Camera->FirstPersonScale,.001f) : 1.f;
 const FVector Stretch(Scale,Scale*Correction,Scale*Correction);
 const FVector Local=View.InverseTransformPosition(Point);
 // Same view-space correction as FViewMatrices::WorldToFirstPerson. Solving in this space
 // keeps the rendered hook on the real board even when hands use a separate FOV and scale.
 return View.TransformPosition(bInverse ? Local/Stretch : Local*Stretch);
}

FVector UTallowToolContactComponent::GetRenderedTipLocation() const
{
 if (!Tool.IsValid()) return FVector::ZeroVector;
 const FVector Tip=Tool->GetComponentTransform().TransformPosition(CrowbarTip);
 return Tool->FirstPersonPrimitiveType==EFirstPersonPrimitiveType::FirstPerson ? ProjectPoint(Tip,false) : Tip;
}

void UTallowToolContactComponent::RestoreRoot()
{
 auto* Hands=Presentation.IsValid() ? Presentation->GetPresentationHands() : nullptr;
 if (Hands && !Offset.IsNearlyZero() && Hands->GetRelativeTransform().Equals(LastApplied,.001f))
  Hands->SetRelativeLocation(Hands->GetRelativeLocation()-Offset);
 Offset=FVector::ZeroVector;
}

void UTallowToolContactComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
 Super::TickComponent(DeltaTime,TickType,TickFunction);
 RestoreRoot();
 if (!Player.IsValid() || !Tool.IsValid() || !Presentation.IsValid()) return;
 auto* Hands=Presentation->GetPresentationHands();
 if (!Hands) return;
 AActor* Current=Interaction.IsValid() && Interaction->IsInteracting() ? Interaction->GetInteractionTarget() : nullptr;
 const bool Contact=Current && Current->IsA<ATallowPlank>() && Player->GetCarriedItem()==ECarriedItemType::Crowbar
  && !Player->IsHiddenInSpot() && !Player->IsTiedToChair() && !Player->IsHandOccupied();
 if (Contact)
 {
  if (LastTarget.Get()!=Current) Weight=0.f;
  LastTarget=Current;
  Target=Interaction->GetContactLocation()+Interaction->GetContactNormal()*.3f;
 }
 // Judge reach once per hold, when the pry has brought the hook to its bite, and keep the answer for the hold.
 if (!Contact) { ReachTarget.Reset(); bReachJudged=bInReach=false; }
 else if (ReachTarget.Get()!=Current) { ReachTarget=Current; bReachJudged=bInReach=false; }
 if (Contact && !bReachJudged && Interaction->GetInteractionProgress()>=.15f)
 {
  bReachJudged=true;
  bInReach=FVector::Dist(GetRenderedTipLocation(),Target)<=ContactReachCm;
 }
 const bool Seat=Contact && bInReach;
 Weight=FMath::FInterpConstantTo(Weight,Seat?1.f:0.f,DeltaTime,Seat?6.f:8.f);
 // Travel advances the hand motion, so creeping has slow spacing and stopping settles the grip.
 // The candle and tool follow the same hand mesh; their fingers never separate from the props.
 const float Speed=Player->GetVelocity().Size2D();
 const bool bMove=Speed>15.f && !Player->IsHiddenInSpot() && !Player->IsTiedToChair() && !Player->IsHoldingStill();
 StridePhase=FMath::Fmod(StridePhase+Speed*DeltaTime/(Speed>300.f?130.f:75.f)*2.f*PI,2.f*PI);
 const float Strength=bMove ? FMath::Clamp(Speed/250.f,0.f,1.35f)*(Player->bIsCrouched?.35f:1.f) : 0.f;
 const FVector Motion(-.25f*FMath::Sin(StridePhase*2.f),.5f*FMath::Cos(StridePhase),.55f*FMath::Sin(StridePhase*2.f));
 LocomotionOffset=FMath::VInterpTo(LocomotionOffset,Motion*Strength,DeltaTime,10.f);
 FVector ProjectorMotion=FVector::ZeroVector;
 if (ProjectorImpulseAge>=0.f)
 {
  ProjectorImpulseAge+=DeltaTime;
  const float Phase=FMath::Clamp(ProjectorImpulseAge/ProjectorImpulseDuration,0.f,1.f);
  const float Pulse=FMath::Square(FMath::Sin(Phase*PI));
  ProjectorMotion=FVector(3.2f*Pulse,0.f,-1.2f*Pulse);
  if (Phase>=1.f) ProjectorImpulseAge=-1.f;
 }
 if (Weight<=0.f)
 {
  LastTarget.Reset();Offset=LocomotionOffset+ProjectorMotion;
  Hands->SetRelativeLocation(Hands->GetRelativeLocation()+Offset);LastApplied=Hands->GetRelativeTransform();
  return;
 }
 const FVector Tip=Tool->GetComponentTransform().TransformPosition(CrowbarTip);
 const FVector Required=Tool->FirstPersonPrimitiveType==EFirstPersonPrimitiveType::FirstPerson ? ProjectPoint(Target,true) : Target;
 const FVector WorldOffset=(Required-Tip)*FMath::SmoothStep(0.f,1.f,Weight);
 const auto* Parent=Hands->GetAttachParent();
 const FVector Seating=(Parent ? Parent->GetComponentTransform().InverseTransformVector(WorldOffset) : WorldOffset).GetClampedToMaxSize(MaxSeatCm);
 Offset=Seating+LocomotionOffset*(1.f-Weight);
 Hands->SetRelativeLocation(Hands->GetRelativeLocation()+Offset);
 LastApplied=Hands->GetRelativeTransform();
}

void UTallowToolContactComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 RestoreRoot(); Super::EndPlay(Reason);
}
