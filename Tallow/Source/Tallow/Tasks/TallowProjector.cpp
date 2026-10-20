#include "TallowProjector.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/AudioComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/TallowCharacter.h"
#include "Noise/TallowNoiseSubsystem.h"

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
	ProjectorLight->SetLightBrightness(0.0f);
	
	ClatterAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("ClatterAudioComponent"));
	ClatterAudioComponent->SetupAttachment(RootComponent);
	ClatterAudioComponent->bAutoActivate = false;
}

// Called when the game starts or when spawned
void ATallowProjector::BeginPlay()
{
	Super::BeginPlay();
	
	if (LightFunctionBase)
	{
		LightMID = UMaterialInstanceDynamic::Create(LightFunctionBase, this);
		ProjectorLight->SetLightFunctionMaterial(LightMID);	
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
		// only if not loaded
		if (!bReelLoaded)
		{
			if(!Player->HasCarriedItem(ECarriedItemType::FilmReel)) { return; }
			Player->ClearCarriedItem();
			bReelLoaded = true;
			SpoolMesh->SetVisibility(true);
			return;
		}
		
		// run threading 
		ThreadProgress = FMath::Min(1.0f, ThreadProgress + ProgressPerTap);
		TimeSinceLastTap = 0.0f;
		SetActorTickEnabled(true);
		if (UTallowNoiseSubsystem* NoiseSubsystem = GetWorld()->GetSubsystem<UTallowNoiseSubsystem>())
		{
			NoiseSubsystem->EmitNoise(ETallowNoiseType::Projector, GetActorLocation(), GetOwner(), TapLoudness, true);	
		}
		// on completion - change state
		if (ThreadProgress >= 1.0f)
		{
			LockoutEndTime = GetWorld()->GetTimeSeconds() + InputLockoutAfterThreshold;
			SetState(EProjectorState::Threaded);
			SetActorTickEnabled(false);
		}
		break;
		
	case EProjectorState::Threaded:
		// run projection if already threaded
		if (GetWorld()->GetTimeSeconds() < LockoutEndTime) { return; }
		SetState(EProjectorState::Projecting);
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
		// reset progress
		ThreadProgress = FMath::Max(0.0f, ThreadProgress - DecayPerSecond * DeltaTime);
	}

	// rotate film reel mesh
	const float CurrentRotationSpeed = (TimeSinceLastTap < DecayDelay) ? RotationSpeedNormal : RotationSpeedSlow;
	FRotator CurrentRotation = SpoolMesh->GetRelativeRotation();
	CurrentRotation.Pitch += CurrentRotationSpeed * DeltaTime;
	SpoolMesh->SetRelativeRotation(CurrentRotation);
	
	if (ThreadProgress <= 0.0f)
	{
		SetActorTickEnabled(false);
	}
}

