#include "TallowProjector.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/AudioComponent.h"
#include "Components/DecalComponent.h"
#include "Components/TextRenderComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/TallowCharacter.h"
#include "Presentation/TallowPlayDirector.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "Tasks/TallowFuseBox.h"
#include "Sound/SoundBase.h"

ATallowProjector::ATallowProjector()
{
 PrimaryActorTick.bCanEverTick = true;
 PrimaryActorTick.bStartWithTickEnabled = false;
 ProjectorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectorMesh"));
 RootComponent = ProjectorMesh;
 ProjectorMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 ProjectorMesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 ProjectorMesh->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
 SpoolMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpoolMesh"));
 SpoolMesh->SetupAttachment(RootComponent);
 SpoolMesh->SetRelativeLocation(FVector(-8,15,48));
 SpoolMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 SpoolMesh->SetVisibility(false);
 TakeupSpoolMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TakeupSpoolMesh"));
 TakeupSpoolMesh->SetupAttachment(RootComponent);
 TakeupSpoolMesh->SetRelativeLocation(FVector(23,15,48));
 TakeupSpoolMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 InsertionReelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InsertionReelMesh"));
 InsertionReelMesh->SetupAttachment(RootComponent);
 InsertionReelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 InsertionReelMesh->SetVisibility(false);
 ProjectorLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("ProjectorLight"));
 ProjectorLight->SetupAttachment(RootComponent);
 ProjectorLight->SetRelativeLocation(FVector(35,0,34));
 ProjectorLight->SetIntensity(0.f);
 ProjectorLight->SetOuterConeAngle(15.f);
 ClatterAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("ClatterAudioComponent"));
 ProjectionAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("ProjectionAudioComponent"));
 ReelInsertAudioComponent=CreateDefaultSubobject<UAudioComponent>(TEXT("ReelInsertAudioComponent"));
 for (UAudioComponent* Audio : {ClatterAudioComponent.Get(),ProjectionAudioComponent.Get(),ReelInsertAudioComponent.Get()})
 {
  Audio->SetupAttachment(RootComponent);
  Audio->bAutoActivate = false;
  Audio->bAllowSpatialization = true;
  Audio->bOverrideAttenuation = true;
  Audio->AttenuationOverrides.bAttenuate = true;
  Audio->AttenuationOverrides.bSpatialize = true;
  Audio->AttenuationOverrides.AttenuationShapeExtents = FVector(140.f);
  Audio->AttenuationOverrides.FalloffDistance = 1800.f;
 }
 ProjectionDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("ProjectionDecal"));
 ProjectionDecal->SetupAttachment(RootComponent);
 ProjectionDecal->DecalSize = FVector(DecalDepth,50,50);
 ProjectionDecal->SetVisibility(false);
 ProjectedDigit = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ProjectedDigit"));
 ProjectedDigit->SetupAttachment(RootComponent);
 ProjectedDigit->SetHorizontalAlignment(EHTA_Center);
 ProjectedDigit->SetVerticalAlignment(EVRTA_TextCenter);
 ProjectedDigit->SetTextRenderColor(FColor(255,237,191));
 ProjectedDigit->SetWorldSize(55.f);
 ProjectedDigit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 ProjectedDigit->SetCastShadow(false);
 ProjectedDigit->SetVisibility(false);
}

void ATallowProjector::BeginPlay()
{
 Super::BeginPlay();
 SupplyRestRotation=SpoolMesh->GetRelativeRotation();
 TakeupRestRotation=TakeupSpoolMesh->GetRelativeRotation();
 auto MeshIfEmpty = [](UStaticMeshComponent* Component,const TCHAR* Path)
 { if (!Component->GetStaticMesh()) Component->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Path)); };
 MeshIfEmpty(ProjectorMesh,TEXT("/Game/Presentation/ProjectorFinal/SM_Projector.SM_Projector"));
 MeshIfEmpty(SpoolMesh,TEXT("/Game/Presentation/ProjectorFinal/SM_ProjectorReel.SM_ProjectorReel"));
 MeshIfEmpty(TakeupSpoolMesh,TEXT("/Game/Presentation/ProjectorFinal/SM_ProjectorReel.SM_ProjectorReel"));
 MeshIfEmpty(InsertionReelMesh,TEXT("/Game/Presentation/ProjectorFinal/SM_FilmReel.SM_FilmReel"));
 if (!ClatterLoopSound) ClatterLoopSound = LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/Projector/S_Projector_Thread_Loop.S_Projector_Thread_Loop"));
 if (!ProjectorLoopSound) ProjectorLoopSound = LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/Projector/S_Projector_Run_Loop.S_Projector_Run_Loop"));
 if (!ReelInsertSound) ReelInsertSound = LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/Projector/S_Projector_ReelInsert.S_Projector_ReelInsert"));
 ProjectionAudioComponent->SetSound(ProjectorLoopSound);
 ClatterAudioComponent->SetSound(ClatterLoopSound);
 ReelInsertAudioComponent->SetSound(ReelInsertSound);
 if (!DecalMaterialBase)
 {
  UMaterialInterface* Assigned=ProjectionDecal->GetDecalMaterial();
  if (Assigned && !Assigned->GetPathName().StartsWith(TEXT("/Engine/"))) DecalMaterialBase=Assigned;
  else DecalMaterialBase=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Tasks/Film_Reel/M_ProjectorFilm_Decal.M_ProjectorFilm_Decal"));
 }
 if (DecalMaterialBase)
 {
  DecalMID = UMaterialInstanceDynamic::Create(DecalMaterialBase,this);
  ProjectionDecal->SetDecalMaterial(DecalMID);
 }
 SetDigitVisible(false);
 PlaceDecal();
 // Authored/test codes remain intact. A fresh run generates its own only when unspecified.
 if (Codes.Num() != 5)
 {
  Codes.Reset();
  for (int32 Index=0;Index<5;++Index) Codes.Add(FMath::RandRange(0,9));
 }
 for (int32& Digit : Codes) Digit=FMath::Clamp(Digit,0,9);
 if (!LinkedFuseBox) LinkedFuseBox=Cast<ATallowFuseBox>(UGameplayStatics::GetActorOfClass(this,ATallowFuseBox::StaticClass()));
}

bool ATallowProjector::HasPower() const { return !bRequiresPower || (IsValid(LinkedFuseBox) && LinkedFuseBox->HasPower()); }

void ATallowProjector::SetDigitVisible(bool bVisible)
{
 if (DecalMID) DecalMID->SetScalarParameterValue(VisibleParam,bVisible?1.f:0.f);
 ProjectionDecal->SetVisibility(bVisible && DecalMID != nullptr);
 ProjectedDigit->SetVisibility(bVisible && DecalMID == nullptr);
}

void ATallowProjector::SetState(EProjectorState NewState)
{
 CurrentState=NewState;
 SetDigitVisible(false);
 ProjectorLight->SetIntensity(NewState==EProjectorState::Projecting?ProjectionIntensity:0.f);
 OnProjectorStateChanged.Broadcast(NewState);
 if (NewState==EProjectorState::Projecting)
 {
  PlaceDecal();
  NoiseCountdown=0.f;
  SetActorTickEnabled(true);
  if (ProjectorLoopSound) ProjectionAudioComponent->FadeIn(.15f,1.f);
  RevealNextCode();
 }
 else if (NewState==EProjectorState::Finished)
 {
  GetWorldTimerManager().SetTimer(EndTimerHandle,this,&ATallowProjector::ProjectorFinished,FMath::Max(.05f,InputLockoutAfterThreshold),false);
 }
}

void ATallowProjector::EmitOperationNoise(AActor* Instigator,float Loudness)
{
 if (auto* Noise=GetWorld()->GetSubsystem<UTallowNoiseSubsystem>())
  Noise->EmitNoise(ETallowNoiseType::Projector,GetActorLocation(),Instigator?Instigator:this,Loudness,true);
}

void ATallowProjector::OnInteract_Implementation(AActor* Interactor)
{
 if (!CanInteract_Implementation(Interactor)) return;
 auto* Player=Cast<ATallowCharacter>(Interactor);
 Operator=Player;
 if (CurrentState==EProjectorState::Empty)
 {
  if (!bReelLoaded)
  {
   Player->ClearCarriedItem();
   bReelLoaded=true;
   InsertAge=0.f;
   const auto* Camera=Player->GetFirstPersonCameraComponent();
   InsertStart=FTransform(Camera->GetComponentRotation(),Camera->GetComponentLocation()+Camera->GetForwardVector()*35.f-Camera->GetRightVector()*17.f-Camera->GetUpVector()*17.f);
   InsertionReelMesh->SetWorldTransform(InsertStart);
   InsertionReelMesh->SetVisibility(true);
   LockoutEndTime=GetWorld()->GetTimeSeconds()+FMath::Max(.1f,ReelInsertDuration);
   SetActorTickEnabled(true);
   if (ReelInsertSound) ReelInsertAudioComponent->Play();
   for (TActorIterator<ATallowPlayDirector> It(GetWorld());It;++It) It->PlayProjectorGesture(true);
   OnReelLoaded.Broadcast();
   return;
  }
  ThreadProgress=FMath::Min(1.f,ThreadProgress+FMath::Clamp(ProgressPerTap,.01f,1.f));
  TimeSinceLastTap=0.f;
  SetActorTickEnabled(true);
  if (ClatterLoopSound && !ClatterAudioComponent->IsPlaying()) ClatterAudioComponent->FadeIn(.06f,1.f);
  EmitOperationNoise(Player,TapLoudness);
  for (TActorIterator<ATallowPlayDirector> It(GetWorld());It;++It) It->PlayProjectorGesture(false);
  if (ThreadProgress>=1.f-KINDA_SMALL_NUMBER)
  {
   ThreadProgress=1.f;
   ClatterAudioComponent->Stop();
   LockoutEndTime=GetWorld()->GetTimeSeconds()+InputLockoutAfterThreshold;
   SetState(EProjectorState::Threaded);
   SetActorTickEnabled(false);
  }
 }
 else if (CurrentState==EProjectorState::Threaded)
 {
  CodeIndex=0;
  SetState(EProjectorState::Projecting);
 }
}

void ATallowProjector::Tick(float DeltaTime)
{
 Super::Tick(DeltaTime);
 if (InsertAge>=0.f)
 {
  InsertAge+=DeltaTime;
  const float T=FMath::SmoothStep(0.f,FMath::Max(ReelInsertDuration,.1f),InsertAge);
  const FTransform Target=SpoolMesh->GetComponentTransform();
  const FVector Arc=GetActorUpVector()*FMath::Sin(T*PI)*9.f;
  InsertionReelMesh->SetWorldLocationAndRotation(FMath::Lerp(InsertStart.GetLocation(),Target.GetLocation(),T)+Arc,FQuat::Slerp(InsertStart.GetRotation(),Target.GetRotation(),T));
  if (T>=1.f)
  {
   InsertAge=-1.f;
   InsertionReelMesh->SetVisibility(false);
   SpoolMesh->SetVisibility(true);
   if (ThreadProgress<=0.f) SetActorTickEnabled(false);
  }
  return;
 }
 if (CurrentState==EProjectorState::Projecting)
 {
  if (!HasPower()) { StopProjection(); return; }
  NoiseCountdown-=DeltaTime;
  if (NoiseCountdown<=0.f)
  {
   EmitOperationNoise(Operator.Get(),OperationLoudness);
   NoiseCountdown=FMath::Max(.1f,OperationNoiseInterval);
  }
 }
 else if (CurrentState==EProjectorState::Empty && bReelLoaded)
 {
  TimeSinceLastTap+=DeltaTime;
  if (TimeSinceLastTap>DecayDelay) ThreadProgress=FMath::Max(0.f,ThreadProgress-DecayPerSecond*DeltaTime);
  if (ThreadProgress<=0.f)
  {
   ClatterAudioComponent->Stop();
   SetActorTickEnabled(false);
   return;
  }
  ClatterAudioComponent->SetPitchMultiplier(FMath::Lerp(MinPitch,MaxPitch,ThreadProgress));
 }
 else return;
 const float Speed=CurrentState==EProjectorState::Projecting || TimeSinceLastTap<DecayDelay?RotationSpeedNormal:RotationSpeedSlow;
 const FRotator Turn(Speed*DeltaTime,0,0); // FRotator Pitch rotates around local Y.
 SpoolMesh->AddLocalRotation(Turn);
 TakeupSpoolMesh->AddLocalRotation(Turn);
}

bool ATallowProjector::CanInteract_Implementation(AActor* Interactor) const
{
 const auto* Player=Cast<ATallowCharacter>(Interactor);
 if (!Player || !Player->CanChangeInventory() || GetWorld()->GetTimeSeconds()<LockoutEndTime) return false;
 if (CurrentState==EProjectorState::Empty) return bReelLoaded || Player->HasCarriedItem(ECarriedItemType::FilmReel);
 return CurrentState==EProjectorState::Threaded && HasPower();
}

FText ATallowProjector::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
 const auto* Player=Cast<ATallowCharacter>(Interactor);
 if (!Player) return FText::GetEmpty();
 if (CurrentState==EProjectorState::Empty)
 {
  if (!bReelLoaded) return FText::FromString(Player->HasCarriedItem(ECarriedItemType::FilmReel)?TEXT("INSERT REEL"):Player->GetInventoryCount(ECarriedItemType::FilmReel)>0?TEXT("TAKE FILM REEL FROM POCKET"):TEXT("FILM REEL NEEDED"));
  return FText::FromString(InsertAge>=0.f?TEXT("SEATING THE REEL"):ThreadProgress<=0.f?TEXT("TAP E TO THREAD REEL"):TEXT("KEEP TAPPING E"));
 }
 if (CurrentState==EProjectorState::Threaded) return FText::FromString(HasPower()?bHasFinishedProjectingOnce?TEXT("WATCH CODE AGAIN"):TEXT("START PROJECTOR"):TEXT("THE PROJECTOR NEEDS POWER"));
 return FText::FromString(CurrentState==EProjectorState::Projecting?TEXT("WATCH THE WALL — FIVE DIGITS"):TEXT("FILM RUNNING OUT"));
}

void ATallowProjector::RevealNextCode()
{
 if (CurrentState!=EProjectorState::Projecting) return;
 if (!Codes.IsValidIndex(CodeIndex))
 {
  SetDigitVisible(false);
  ProjectionAudioComponent->Stop();
  SetActorTickEnabled(false);
  const bool bFirst=!bHasFinishedProjectingOnce;
  bHasFinishedProjectingOnce=true;
  SetState(EProjectorState::Finished);
  if (bFirst) OnProjectorFinished.Broadcast();
  UE_LOG(LogTemp,Display,TEXT("TALLOW_PROJECTOR complete %s"),*GetName());
  return;
 }
 const int32 Value=Codes[CodeIndex];
 if (DecalMID) DecalMID->SetScalarParameterValue(DigitParam,float(Value));
 ProjectedDigit->SetText(FText::AsNumber(Value));
 SetDigitVisible(true);
 OnProjectorCodeRevealed.Broadcast(Value,CodeIndex);
 UE_LOG(LogTemp,Display,TEXT("TALLOW_PROJECTOR digit %d index %d"),Value,CodeIndex);
 GetWorldTimerManager().SetTimer(CodeTimerHandle,this,&ATallowProjector::HideCode,FMath::Max(.1f,ShowDuration),false);
}

void ATallowProjector::HideCode()
{
 if (CurrentState!=EProjectorState::Projecting) return;
 SetDigitVisible(false);
 ++CodeIndex;
 GetWorldTimerManager().SetTimer(GapTimerHandle,this,&ATallowProjector::RevealNextCode,FMath::Max(.05f,GapDuration),false);
}

void ATallowProjector::StopProjection()
{
 GetWorldTimerManager().ClearTimer(CodeTimerHandle);
 GetWorldTimerManager().ClearTimer(GapTimerHandle);
 GetWorldTimerManager().ClearTimer(EndTimerHandle);
 ClatterAudioComponent->Stop();
 ProjectionAudioComponent->Stop();
 ReelInsertAudioComponent->Stop();
 InsertAge=-1.f;
 InsertionReelMesh->SetVisibility(false);
 SpoolMesh->SetVisibility(bReelLoaded);
 SetDigitVisible(false);
 ProjectorLight->SetIntensity(0.f);
 SetActorTickEnabled(false);
 CodeIndex=0;
 if (CurrentState!=EProjectorState::Empty) SetState(EProjectorState::Threaded);
}

void ATallowProjector::ResetProjector()
{
 StopProjection();
 bReelLoaded=false;
 bHasFinishedProjectingOnce=false;
 ThreadProgress=0.f;
 TimeSinceLastTap=0.f;
 InsertAge=-1.f;
 LockoutEndTime=0.f;
 Operator.Reset();
 InsertionReelMesh->SetVisibility(false);
 SpoolMesh->SetVisibility(false);
 SpoolMesh->SetRelativeRotation(SupplyRestRotation);
 TakeupSpoolMesh->SetRelativeRotation(TakeupRestRotation);
 SetState(EProjectorState::Empty);
}

void ATallowProjector::EndPlay(const EEndPlayReason::Type Reason)
{
 StopProjection();
 Super::EndPlay(Reason);
}
void ATallowProjector::ProjectorFinished() { SetState(EProjectorState::Threaded); }

void ATallowProjector::PlaceDecal()
{
 const FVector Start=ProjectorLight->GetComponentLocation();
 const FVector End=Start+ProjectorLight->GetForwardVector()*MaxProjectionDistance;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(ProjectorSurface),false,this);
 if (Operator.IsValid()) Query.AddIgnoredActor(Operator.Get());
 FHitResult Hit;
 if (!GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Query))
 {
  UE_LOG(LogTemp,Warning,TEXT("TALLOW_PROJECTOR %s: aim the lens at a wall within %.0f cm"),*GetName(),MaxProjectionDistance);
  return;
 }
 FRotator Rotation=FRotationMatrix::MakeFromXZ(-Hit.ImpactNormal,ProjectorLight->GetUpVector()).Rotator();
 Rotation.Roll+=DecalRollOffset;
 ProjectionDecal->SetWorldLocationAndRotation(Hit.ImpactPoint,Rotation);
 const float HalfWidth=Hit.Distance*FMath::Tan(FMath::DegreesToRadians(ProjectorLight->OuterConeAngle));
 ProjectionDecal->DecalSize=FVector(DecalDepth,HalfWidth,HalfWidth);
 ProjectedDigit->SetWorldLocationAndRotation(Hit.ImpactPoint+Hit.ImpactNormal*1.5f,Hit.ImpactNormal.Rotation());
}

float ATallowProjector::GetInteractionProgress_Implementation(AActor* Interactor) const
{
 return CurrentState==EProjectorState::Empty && bReelLoaded?ThreadProgress:-1.f;
}
