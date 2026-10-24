#include "TallowProjector.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Tasks/TallowFuseBox.h"
#include "Player/TallowCharacter.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "Components/DecalComponent.h"

// Sets default values
ATallowProjector::ATallowProjector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	
	ProjectorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectorMesh"));
	RootComponent = ProjectorMesh;
	ProjectorMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ProjectorMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECR_Block);
	
	SpoolMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpoolMesh"));
	SpoolMesh->SetupAttachment(ProjectorMesh);
	SpoolMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpoolMesh->SetVisibility(false);
	
	ProjectorLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("ProjectorLight")); 
	ProjectorLight->SetupAttachment(ProjectorMesh);
	ProjectorLight->SetIntensity(0.0f);
	
	ClatterAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("ClatterAudioComponent"));
	ClatterAudioComponent->SetupAttachment(RootComponent);
	ClatterAudioComponent->bAutoActivate = false;
	
	ProjectionAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("ProjectionAudioComponent"));
	ProjectionAudioComponent->SetupAttachment(RootComponent);
	ProjectionAudioComponent->bAutoActivate = false;
	
	ProjectionDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("ProjectionDecal"));
	ProjectionDecal->SetupAttachment(ProjectorMesh);
	ProjectionDecal->DecalSize = FVector(DecalDepth, 50.0f, 50.0f);
	ProjectionDecal->SetVisibility(false); 
}

// Called when the game starts or when spawned
void ATallowProjector::BeginPlay()
{
	Super::BeginPlay();
	
	if (DecalMaterialBase)
	{
		DecalMID = UMaterialInstanceDynamic::Create(DecalMaterialBase, this);
		DecalMID->SetScalarParameterValue(VisibleParam, 0.0f);
		ProjectionDecal->SetDecalMaterial(DecalMID);	
	}
	
	PlaceDecal();
	
	if (ProjectionAudioComponent && ProjectorLoopSound)
	{
		ProjectionAudioComponent->SetSound(ProjectorLoopSound);
	}
	
	if (ClatterAudioComponent && ClatterLoopSound)
	{
		ClatterAudioComponent->SetSound(ClatterLoopSound);
	}
	
	// populate code array
	Codes.Reset();	
	for (int32 i = 0; i < 5; i++)
	{
		Codes.Add(FMath::RandRange(0, 9));
	}
	
	// get fuse box
	AActor* FoundActor = UGameplayStatics::GetActorOfClass(GetWorld(), ATallowFuseBox::StaticClass());
	if (FoundActor)
	{
		LinkedFuseBox = Cast<ATallowFuseBox>(FoundActor);
	}
}

void ATallowProjector::SetState(EProjectorState NewState)
{
	CurrentState = NewState;
		
	switch (CurrentState)
	{
	case EProjectorState::Empty:
		OnProjectorStateChanged.Broadcast(EProjectorState::Empty);
		ThreadProgress = 0.0f;
		if (ProjectorLight)
		{
			ProjectorLight->SetIntensity(0.0f);
		}
		if (DecalMID)
		{
			DecalMID->SetScalarParameterValue(VisibleParam, 0.0f);
		}
		
		break;
		
	case EProjectorState::Threaded:
		OnProjectorStateChanged.Broadcast(EProjectorState::Threaded);
		if (ProjectorLight)
		{
			ProjectorLight->SetIntensity(0.0f);
		}
		if (DecalMID)
		{
			DecalMID->SetScalarParameterValue(VisibleParam, 0.0f);
		}
		
		break;
		
	case EProjectorState::Projecting:
		OnProjectorStateChanged.Broadcast(EProjectorState::Projecting);
		if (ProjectorLight)
		{
			ProjectorLight->SetIntensity(ProjectionIntensity);	
		}
		if (ProjectionAudioComponent && !ProjectionAudioComponent->IsPlaying())
		{
			ProjectionAudioComponent->FadeIn(0.5f, 1.0f);
		}
		RevealNextCode();
		break;
		
	case EProjectorState::Finished:
		OnProjectorStateChanged.Broadcast(EProjectorState::Finished);
		if (ProjectorLight)
		{
			ProjectorLight->SetIntensity(0.0f);	
		}
		if (DecalMID)
		{
			DecalMID->SetScalarParameterValue(VisibleParam, 0.0f);
		}
		
		GetWorld()->GetTimerManager().SetTimer(
			EndTimerHandle,	
			this,
			&ATallowProjector::ProjectorFinished,
			InputLockoutAfterThreshold,
			false
		);
		break;
		
	default:
		break;
	}
}


void ATallowProjector::OnInteract_Implementation(AActor* Interactor)
{
	if (!Interactor) { return; }
	ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) { return; }
	
	switch (CurrentState)
	{
	case EProjectorState::Empty:
		{
			// only if not loaded
			if (!bReelLoaded)
			{
				if(!Player->HasCarriedItem(ECarriedItemType::FilmReel)) { return; }
				Player->ClearCarriedItem();
				bReelLoaded = true;
				OnReelLoaded.Broadcast();
				SpoolMesh->SetVisibility(true);
				return;
			}
		
			// run threading 
			ThreadProgress = FMath::Min(1.0f, ThreadProgress + ProgressPerTap);
			TimeSinceLastTap = 0.0f;
			if (!IsActorTickEnabled())
			{
				SetActorTickEnabled(true);	
			}
		
			// play audio
			UTallowNoiseSubsystem* NoiseSubsystem = GetWorld()->GetSubsystem<UTallowNoiseSubsystem>();
			if (ClatterAudioComponent && !ClatterAudioComponent->IsPlaying())
			{
				ClatterAudioComponent->FadeIn(0.5f, 1.0f);
			}
			if (NoiseSubsystem)
			{
				NoiseSubsystem->EmitNoise(ETallowNoiseType::Projector, GetActorLocation(), Player, TapLoudness, true);
			}
		
			// on completion - change state
			if (ThreadProgress >= 1.0f)
			{
				if (ClatterAudioComponent && ClatterAudioComponent->IsPlaying())
				{
					ClatterAudioComponent->FadeOut(0.3f, 0.0f);
				}
				LockoutEndTime = GetWorld()->GetTimeSeconds() + InputLockoutAfterThreshold;
				SetState(EProjectorState::Threaded);
				SetActorTickEnabled(false);
			}
			break;
		}
	case EProjectorState::Threaded:
		if (LinkedFuseBox && LinkedFuseBox->HasPower())
		{
			// run projection if already threaded
			if (GetWorld()->GetTimeSeconds() < LockoutEndTime) { return; }
			CodeIndex = 0;
			SetState(EProjectorState::Projecting);
		}
		break;
		
	default:
		break;
	}
}

void ATallowProjector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	TimeSinceLastTap += DeltaTime;
	if (TimeSinceLastTap > DecayDelay)
	{
		// reduce progress
		ThreadProgress = FMath::Max(0.0f, ThreadProgress - DecayPerSecond * DeltaTime);
	}

	// rotate film reel mesh
	const float CurrentRotationSpeed = (TimeSinceLastTap < DecayDelay) ? RotationSpeedNormal : RotationSpeedSlow;
	SpoolMesh->AddLocalRotation(FRotator(CurrentRotationSpeed * DeltaTime, 0.0f, 0.0f));
	
	// play clatter audio 
	if (ClatterAudioComponent)
	{
		if (ThreadProgress > 0.0f)
		{
			// play if not
			if (!ClatterAudioComponent->IsPlaying())
			{
				ClatterAudioComponent->FadeIn(0.5f, 1.0f);
			}
			
			const float TargetPitch = FMath::Lerp(MinPitch, MaxPitch, ThreadProgress);
			ClatterAudioComponent->SetPitchMultiplier(TargetPitch);
		}
		else
		{
			// stop audio
			if (ClatterAudioComponent->IsPlaying())
			{
				ClatterAudioComponent->FadeOut(0.3f, 0.0f);
			}
		}
	}
	
	if (ThreadProgress <= 0.0f)
	{
		SetActorTickEnabled(false);
	}
}

bool ATallowProjector::CanInteract_Implementation(AActor* Interactor) const
{
	if (!Interactor) { return false; }
	const ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) { return false; }
	
	if (CurrentState == EProjectorState::Empty)
	{
		if (!bReelLoaded)
		{
			if (!Player->HasCarriedItem(ECarriedItemType::FilmReel))
			{
				return false;
			}
		}
	}
	if (CurrentState == EProjectorState::Threaded)
	{
		// if has no power, cannot interact
		if (LinkedFuseBox && LinkedFuseBox->HasPower())
		{
			return true;
		}
		return false;
	}
	// no interaction while projecting or finished
	if (CurrentState == EProjectorState::Projecting || CurrentState == EProjectorState::Finished)
	{
		return false;
	}
	
	return true;
}

FText ATallowProjector::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
	if (!Interactor) { return FText::GetEmpty(); }
	ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) { return FText::GetEmpty(); }
	
	switch (CurrentState)
	{
	case EProjectorState::Empty:
		if (!bReelLoaded)
		{
			// doesn't have reel
			if (!Player->HasCarriedItem(ECarriedItemType::FilmReel))
			{
				return FText::FromString("FILM REEL NEEDED");
			}	
			return FText::FromString("INSERT REEL");
		}
		if (ThreadProgress <= 0.0f)
		{
			return FText::FromString("TAP TO THREAD REEL");
		}
		return FText::FromString("KEEP MASHING");
	
	case EProjectorState::Threaded:
		if (LinkedFuseBox && LinkedFuseBox->HasPower())
		{
			return FText::FromString("START PROJECTOR");
		}
		return FText::FromString("NO POWER");
	
	default:
		return FText::GetEmpty();
	}
}

void ATallowProjector::RevealNextCode()
{
	if (!DecalMID)
	{
		return;
	}
	
	// finished sequence
	if (!Codes.IsValidIndex(CodeIndex))
	{
		if (!bHasFinishedProjectingOnce)
		{
			OnProjectorFinished.Broadcast();	
		}
		if (ProjectionAudioComponent && ProjectionAudioComponent->IsPlaying())
		{
			ProjectionAudioComponent->FadeOut(0.3f, 0.0f);
		}
		bHasFinishedProjectingOnce = true;
		SetState(EProjectorState::Finished);
		
		
		return;
	}
	
	// show code
	const int32 CodeValue = Codes[CodeIndex];
	DecalMID->SetScalarParameterValue(VisibleParam, 1.0f);
	DecalMID->SetScalarParameterValue(DigitParam, static_cast<float>(CodeValue));
	OnProjectorCodeRevealed.Broadcast(CodeValue, CodeIndex);
	
	// start timer - call hide code on finish
	GetWorld()->GetTimerManager().SetTimer(
		CodeTimerHandle,
		this,
		&ATallowProjector::HideCode,
		ShowDuration,
		false
		);
}

void ATallowProjector::HideCode()
{
	if (!DecalMID) { return; }
	
	// hide light
	DecalMID->SetScalarParameterValue(VisibleParam, 0.0f);
	
	// start timer - call show on finish
	GetWorld()->GetTimerManager().SetTimer(
		GapTimerHandle,
		this,
		&ATallowProjector::RevealNextCode,
		GapDuration,
		false
		);
	CodeIndex++;
}

void ATallowProjector::ResetProjector()
{
	CodeIndex = 0;
	SetActorTickEnabled(false);
	if (ClatterAudioComponent && ClatterAudioComponent->IsPlaying())
	{
		ClatterAudioComponent->FadeOut(0.2f, 0.0f);
	}
	if (ProjectionAudioComponent && ProjectionAudioComponent->IsPlaying())
	{
		ProjectionAudioComponent->FadeOut(0.2f, 0.0f);
	}
	GetWorld()->GetTimerManager().ClearTimer(CodeTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(GapTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(EndTimerHandle);
	if (CurrentState != EProjectorState::Empty)
	{
		SetState(EProjectorState::Threaded);
	}
	else
	{
		SetState(EProjectorState::Empty);
	}
}

void ATallowProjector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CodeTimerHandle);
		World->GetTimerManager().ClearTimer(GapTimerHandle);
		World->GetTimerManager().ClearTimer(EndTimerHandle);
	}
	
	if (ClatterAudioComponent && ClatterAudioComponent->IsPlaying())
	{
		ClatterAudioComponent->Stop();
	}
	if (ProjectionAudioComponent && ProjectionAudioComponent->IsPlaying())
	{
		ProjectionAudioComponent->Stop();
	}
	
	Super::EndPlay(EndPlayReason);
}

// helper for end timer 
void ATallowProjector::ProjectorFinished()
{
	
	SetState(EProjectorState::Threaded);
}
 
void ATallowProjector::PlaceDecal()
{
	if (!ProjectionDecal || !ProjectorLight) { return; }

	const FVector Start = ProjectorLight->GetComponentLocation();
	const FVector End = Start + ProjectorLight->GetForwardVector() * MaxProjectionDistance;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	// if not reaching surface
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: projector isn't aimed at a surface"), *GetName());
		ProjectionDecal->SetVisibility(false);
		return;
	}

	// face the decal into the wall
	FRotator Rot = FRotationMatrix::MakeFromXZ(-Hit.ImpactNormal, ProjectorLight->GetUpVector()).Rotator();
	Rot.Roll += DecalRollOffset;
	ProjectionDecal->SetWorldLocationAndRotation(Hit.ImpactPoint, Rot);

	// match the beam footprint
	const float HalfWidth = Hit.Distance * FMath::Tan(FMath::DegreesToRadians(ProjectorLight->OuterConeAngle));
	ProjectionDecal->DecalSize = FVector(DecalDepth, HalfWidth, HalfWidth);
	ProjectionDecal->MarkRenderStateDirty();
	ProjectionDecal->SetVisibility(true);
}

float ATallowProjector::GetInteractionProgress_Implementation(AActor* Interactor) const
{
	// only when mashing
	if (CurrentState == EProjectorState::Empty && bReelLoaded)
	{
		return ThreadProgress;
	}
	return -1.0f;
}

