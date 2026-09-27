#include "AI/Traversal/TallowSurfaceTraversalComponent.h"
#include "AI/TallowCreature.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GameMode/TallowGameMode.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UTallowSurfaceTraversalComponent::UTallowSurfaceTraversalComponent()
{
 PrimaryComponentTick.bCanEverTick = true;
 PrimaryComponentTick.TickGroup = TG_PrePhysics;
}
void UTallowSurfaceTraversalComponent::BeginPlay()
{
 Super::BeginPlay(); Character = Cast<ACharacter>(GetOwner());
 CooldownUntil = GetWorld()->GetTimeSeconds() + 5.f; NextProbeTime=CooldownUntil;
 // Editor guides never participate in the cooked game. Exclude their PIE duplicates too.
 if (Character.IsValid()) for (TActorIterator<AActor> It(GetWorld()); It; ++It)
  if (It->IsEditorOnly()) Character->GetCapsuleComponent()->IgnoreActorWhenMoving(*It,true);
 if (auto* Mode=Cast<ATallowGameMode>(GetWorld()->GetAuthGameMode())) Mode->OnCaught.AddUniqueDynamic(this,&UTallowSurfaceTraversalComponent::FreezeForCapture);
}
void UTallowSurfaceTraversalComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
 if (auto* Mode=Cast<ATallowGameMode>(GetWorld()->GetAuthGameMode())) Mode->OnCaught.RemoveDynamic(this,&UTallowSurfaceTraversalComponent::FreezeForCapture);
 Super::EndPlay(EndPlayReason);
}
void UTallowSurfaceTraversalComponent::FreezeForCapture()
{
 bCaptureFrozen=true; ActualSpeed=0.f;
 if (Character.IsValid()) Character->GetCharacterMovement()->StopMovementImmediately();
}
bool UTallowSurfaceTraversalComponent::IsCurrentPoseClear() const
{
 return Character.IsValid() && PoseClear(Character->GetActorLocation(),Character->GetActorQuat());
}
FString UTallowSurfaceTraversalComponent::TagValue(const AActor* Actor, const FString& Prefix)
{
 for (const FName& Tag : Actor->Tags) { const FString Text = Tag.ToString(); if (Text.StartsWith(Prefix)) return Text.RightChop(Prefix.Len()); }
 return FString();
}
bool UTallowSurfaceTraversalComponent::PoseClear(const FVector& Position, const FQuat& Rotation) const
{
 if (!Character.IsValid()) return false;
 const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
 FCollisionQueryParams Params(SCENE_QUERY_STAT(KeeperTraversal), false, Character.Get());
 TArray<AActor*> Attached; Character->GetAttachedActors(Attached); Params.AddIgnoredActors(Attached);
 Params.AddIgnoredActors(Capsule->GetMoveIgnoreActors());
 const FCollisionShape Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight());
 const bool bBlocked=GetWorld()->OverlapBlockingTestByProfile(Position,Rotation,Capsule->GetCollisionProfileName(),Shape,Params);
 if (bBlocked)
 {
  TArray<FOverlapResult> Hits; GetWorld()->OverlapMultiByProfile(Hits,Position,Rotation,Capsule->GetCollisionProfileName(),Shape,Params);
  for (const auto& Hit:Hits) if(Hit.bBlockingHit) UE_LOG(LogTemp,Verbose,TEXT("Keeper pose %s blocked by %s"),*Position.ToString(),*(Hit.GetActor()?Hit.GetActor()->GetActorNameOrLabel():TEXT("None")));
 }
 return !bBlocked;
}
bool UTallowSurfaceTraversalComponent::BeginTraversal(TArray<FWaypoint>&& Points, bool bIsSqueeze)
{
 if (bCaptureFrozen || bActive || !Character.IsValid() || Points.IsEmpty()) return false;
 for (const FWaypoint& Point : Points) if (!PoseClear(Point.Position, Point.Rotation))
 {
  TArray<FOverlapResult> Hits;
  const auto* Capsule=Character->GetCapsuleComponent();
  FCollisionQueryParams Params(SCENE_QUERY_STAT(KeeperRouteValidation),false,Character.Get());
  GetWorld()->OverlapMultiByProfile(Hits,Point.Position,Point.Rotation,Capsule->GetCollisionProfileName(),FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Params);
  for (const auto& Hit:Hits) if(Hit.bBlockingHit) UE_LOG(LogTemp,Warning,TEXT("Keeper traversal endpoint %s blocked by %s (%s)"),*Point.Position.ToString(),*(Hit.GetActor()?Hit.GetActor()->GetActorNameOrLabel():TEXT("None")),*GetNameSafe(Hit.GetComponent()));
  return false;
 }
 auto* Movement = Character->GetCharacterMovement();
 SavedGravity = Movement->GravityScale; bSavedOrient = Movement->bOrientRotationToMovement;
 bSavedControllerYaw = Character->bUseControllerRotationYaw; bSavedControllerDesired = Movement->bUseControllerDesiredRotation;
 if (AAIController* AI = Cast<AAIController>(Character->GetController()))
 {
  AI->StopMovement();
  if (UBrainComponent* Brain = AI->GetBrainComponent()) { Brain->PauseLogic(TEXT("Keeper surface traversal")); bPausedBrain = true; }
 }
 Movement->StopMovementImmediately(); Movement->GravityScale = 0.f;
 Movement->bOrientRotationToMovement = false; Movement->bUseControllerDesiredRotation = false; Character->bUseControllerRotationYaw = false;
 Movement->SetMovementMode(MOVE_Flying);
 Waypoints = MoveTemp(Points); WaypointIndex = 0; bActive = true; bSqueeze = bIsSqueeze; bReturning = false;
 SafeHistory.Reset(); SafeHistory.Add({Character->GetActorLocation(), Character->GetActorQuat(), FVector::UpVector});
 return true;
}
bool UTallowSurfaceTraversalComponent::StartSurfaceRoute()
{
 if (!Character.IsValid()) return false;
 TArray<AActor*> Anchors;
 for (TActorIterator<AActor> It(GetWorld()); It; ++It) if (It->ActorHasTag(TEXT("Tallow.Traversal.Anchor"))) Anchors.Add(*It);
 // Stable packaged-game IDs. Labels are editor-only and deliberately not used.
 Anchors.Sort([](const AActor& A, const AActor& B) { return TagValue(&A,TEXT("Next=")) < TagValue(&B,TEXT("Next=")); });
 if (Anchors.Num() < 2) return false;
 // Existing route has B07 -> B00; sorting by Next puts B07 first. Rotate to B00 -> B01.
 const int32 Start = Anchors.IndexOfByPredicate([](const AActor* A){ return TagValue(A,TEXT("Next=")) == TEXT("B01"); });
 if (Start == INDEX_NONE) return false;
 TArray<FWaypoint> Points;
 const float Offset = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 10.f;
 for (int32 I = 0; I < Anchors.Num(); ++I)
 {
  AActor* Anchor = Anchors[(Start + I) % Anchors.Num()];
  TArray<FString> Numbers; TagValue(Anchor,TEXT("Normal=")).ParseIntoArray(Numbers,TEXT(","));
  if (Numbers.Num() != 3) return false;
  const FVector Normal = FVector(FCString::Atof(*Numbers[0]),FCString::Atof(*Numbers[1]),FCString::Atof(*Numbers[2])).GetSafeNormal();
  if (Normal.IsNearlyZero()) return false;
  // Forward stays along corridor while dorsal/foot axis follows support surface.
  const FQuat Rotation = FRotationMatrix::MakeFromXZ(FVector(0,1,0), Normal).ToQuat();
  Points.Add({Anchor->GetActorLocation() + Normal * Offset, Rotation, Normal});
 }
 if (FVector::Dist(Character->GetActorLocation(), Points[0].Position) > 250.f) return false;
 return BeginTraversal(MoveTemp(Points), false);
}
bool UTallowSurfaceTraversalComponent::StartSqueeze(AActor* Entry, AActor* Exit)
{
 if (!Character.IsValid() || !IsValid(Entry) || !IsValid(Exit)) return false;
 FVector Direction = Exit->GetActorLocation() - Entry->GetActorLocation(); Direction.Z = 0;
 if (Direction.IsNearlyZero()) return false;
 const FQuat Rotation = Direction.Rotation().Quaternion();
 const FVector Offset(0,0,Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3.f);
 TArray<FWaypoint> Points; Points.Add({Entry->GetActorLocation()+Offset,Rotation,FVector::UpVector});
 Points.Add({Exit->GetActorLocation()+Offset,Rotation,FVector::UpVector});
 if (FVector::Dist(Character->GetActorLocation(),Points[0].Position)>150.f) return false;
 return BeginTraversal(MoveTemp(Points), true);
}
bool UTallowSurfaceTraversalComponent::SweepStep(const FWaypoint& Target, float DeltaTime)
{
 const FVector Before = Character->GetActorLocation(); const FQuat BeforeRotation = Character->GetActorQuat();
 const float Speed=bSqueeze?SqueezeSpeed:(Target.Normal.Z < -.5f ? CeilingSpeed : CrawlSpeed);
 const float Distance=FVector::Dist(Before,Target.Position);
 const float Angle=BeforeRotation.AngularDistance(Target.Rotation);
 // Translation and rotation share one progress parameter. Otherwise short corner segments
 // arrive while still upside down and the unchanged full capsule strikes the ceiling.
 const float Alpha=FMath::Min(1.f,FMath::Min(Distance>SMALL_NUMBER?Speed*DeltaTime/Distance:1.f,Angle>SMALL_NUMBER?1.6f*DeltaTime/Angle:1.f));
 const FVector Position=FMath::Lerp(Before,Target.Position,Alpha);
 const FQuat Rotation=FQuat::Slerp(BeforeRotation,Target.Rotation,Alpha).GetNormalized();
 // UE's translation sweep does not sweep rotational volume. Test intermediate rotations too.
 for (int32 I=1; I<=4; ++I)
  if (!PoseClear(FMath::Lerp(Before,Position,I/4.f),FQuat::Slerp(BeforeRotation,Rotation,I/4.f))) { UE_LOG(LogTemp,Warning,TEXT("Keeper rotation blocked near %s toward %s"),*Before.ToString(),*Target.Position.ToString()); return false; }
 FHitResult Hit;
 Character->GetCapsuleComponent()->MoveComponent(Position-Before,Rotation,true,&Hit);
 ActualSpeed = FVector::Dist(Before,Character->GetActorLocation()) / FMath::Max(DeltaTime,SMALL_NUMBER);
 SurfaceNormal = Character->GetActorUpVector();
 if (Hit.bBlockingHit) UE_LOG(LogTemp,Warning,TEXT("Keeper sweep blocked by %s near %s"),*(Hit.GetActor()?Hit.GetActor()->GetActorNameOrLabel():TEXT("None")),*Before.ToString());
 return !Hit.bBlockingHit;
}
void UTallowSurfaceTraversalComponent::RequestGroundExit()
{
 if (!bActive || bReturning) return;
 bReturning = true; Waypoints.Reset();
 for (int32 I=SafeHistory.Num()-1; I>=0; --I) Waypoints.Add(SafeHistory[I]);
 WaypointIndex = 0;
}
void UTallowSurfaceTraversalComponent::FinishTraversal()
{
 if (!Character.IsValid()) return;
 auto* Movement=Character->GetCharacterMovement();
 Movement->StopMovementImmediately(); Movement->GravityScale=SavedGravity; Movement->bOrientRotationToMovement=bSavedOrient;
 Character->bUseControllerRotationYaw=bSavedControllerYaw; Movement->bUseControllerDesiredRotation=bSavedControllerDesired;
 Movement->SetMovementMode(MOVE_Falling);
 if (bPausedBrain) if (AAIController* AI=Cast<AAIController>(Character->GetController()))
  if (UBrainComponent* Brain=AI->GetBrainComponent()) Brain->ResumeLogic(TEXT("Keeper returned to ground"));
 bPausedBrain=false; bActive=false; bSqueeze=false; ActualSpeed=0; SurfaceNormal=FVector::UpVector;
 CooldownUntil=GetWorld()->GetTimeSeconds()+RouteCooldown;
}
void UTallowSurfaceTraversalComponent::ResetTraversal()
{
 bCaptureFrozen=false;
 if (bActive) FinishTraversal();
 Waypoints.Reset(); SafeHistory.Reset(); SqueezeAlpha=0;
}
void UTallowSurfaceTraversalComponent::TryAutomaticTraversal()
{
 ATallowCreature* Creature=Cast<ATallowCreature>(Character.Get());
 if (!Creature) return;
 if (Creature->CurrentState==ETallowCreatureState::Patrol && GetWorld()->GetTimeSeconds()>CooldownUntil)
 {
  if (StartSurfaceRoute()) return;
  CooldownUntil=GetWorld()->GetTimeSeconds()+2.f;
 }
 for (TActorIterator<AActor> It(GetWorld()); It; ++It)
 {
  AActor* Entry=*It;
  if (!Entry->ActorHasTag(TEXT("Tallow.Traversal.Squeeze"))) continue;
  const bool bEntry=Entry->ActorHasTag(TEXT("Entry"));
  if (FVector::Dist2D(Character->GetActorLocation(),Entry->GetActorLocation())>110.f || FMath::Abs(Character->GetActorLocation().Z-Entry->GetActorLocation().Z-Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight())>50.f) continue;
  for (TActorIterator<AActor> Other(GetWorld()); Other; ++Other)
  {
   if (*Other==Entry || !Other->ActorHasTag(TEXT("Tallow.Traversal.Squeeze")) || Other->ActorHasTag(TEXT("Entry"))==bEntry) continue;
   for (FName Tag:Entry->Tags) if (Tag!=TEXT("Entry") && Tag!=TEXT("Exit") && Tag!=TEXT("Tallow.Traversal.Squeeze") && Other->ActorHasTag(Tag))
   {
    const FVector Direction=(Other->GetActorLocation()-Entry->GetActorLocation()).GetSafeNormal();
    if (FVector::DotProduct(Character->GetVelocity(),Direction)>10.f && StartSqueeze(Entry,*Other)) return;
   }
  }
 }
}
void UTallowSurfaceTraversalComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
 Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
 if (!Character.IsValid() || bCaptureFrozen) return;
 SqueezeAlpha=FMath::FInterpTo(SqueezeAlpha,IsSqueezing()?1.f:0.f,DeltaTime,5.f);
 if (!bActive) { if(bAutomaticTraversal && GetWorld()->GetTimeSeconds()>NextProbeTime) { NextProbeTime=GetWorld()->GetTimeSeconds()+.15f; TryAutomaticTraversal(); } return; }
 if (const ATallowCreature* Creature=Cast<ATallowCreature>(Character.Get()))
  if(Creature->CurrentState!=ETallowCreatureState::Patrol && !bSqueeze) RequestGroundExit();
 Character->GetCharacterMovement()->StopMovementImmediately();
 if (!Waypoints.IsValidIndex(WaypointIndex)) { FinishTraversal(); return; }
 const FWaypoint& Target=Waypoints[WaypointIndex];
 if (!SweepStep(Target,FMath::Min(DeltaTime,.05f))) { RequestGroundExit(); return; }
 if (!bReturning && (SafeHistory.IsEmpty() || FVector::Dist(SafeHistory.Last().Position,Character->GetActorLocation())>10.f))
  SafeHistory.Add({Character->GetActorLocation(),Character->GetActorQuat(),SurfaceNormal});
 if (FVector::Dist(Character->GetActorLocation(),Target.Position)<1.f && Character->GetActorQuat().AngularDistance(Target.Rotation)<.02f) ++WaypointIndex;
}
