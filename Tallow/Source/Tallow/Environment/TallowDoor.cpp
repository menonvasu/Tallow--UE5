#include "TallowDoor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TimelineComponent.h"
#include "Player/TallowCharacter.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "Tasks/TallowPlank.h"

// Sets default values
ATallowDoor::ATallowDoor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	DoorFrameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorFrameMesh"));
	RootComponent = DoorFrameMesh;
	
	HingePivot = CreateDefaultSubobject<USceneComponent>(TEXT("HingePivot"));
	HingePivot->SetupAttachment(RootComponent);
	
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(HingePivot);
	
	DoorTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DoorTimeline"));
}

// Called when the game starts or when spawned
void ATallowDoor::BeginPlay()
{
	Super::BeginPlay();
	
	InitialPlanks = BarricadePlanks;
}

bool ATallowDoor::CanInteract_Implementation(AActor* Interactor) const
{
	// if door has planks 
	if (IsBarricaded()) { return false; }
	
	return true;
}

FText ATallowDoor::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
	if (IsBarricaded()) { return FText::FromString("BARRICADED"); }
	
	if (!bIsOpen) { return FText::FromString("OPEN DOOR"); }
	
	return FText::FromString("CLOSE DOOR");
}

void ATallowDoor::NotifyPlankRemoved(ATallowPlank* Plank)
{
	BarricadePlanks.Remove(Plank);
}

void ATallowDoor::OnInteract_Implementation(AActor* Interactor)
{
	if (!Interactor) { return; }
	ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) { return; }
	
	UTallowNoiseSubsystem* NoiseSubsystem = GetWorld()->GetSubsystem<UTallowNoiseSubsystem>();
	if (!NoiseSubsystem) { return; }
	
	// if door closed
	if (!bIsOpen)
	{
		if (Player->GetVelocity().Size() > Player->Walkspeed)
		{
			// slam door!
			NoiseSubsystem->EmitNoise(ETallowNoiseType::DoorSlam, GetActorLocation(), Player, DoorSlamLoudness, true);
		}
		else
		{
			// open door
			NoiseSubsystem->EmitNoise(ETallowNoiseType::DoorOpen, GetActorLocation(), Player, DoorOpenLoudness, false);
		}
	}
	// if open - close
	else
	{
		NoiseSubsystem->EmitNoise(ETallowNoiseType::DoorClose, GetActorLocation(), Player, DoorCloseLoudness, true);
	}
	
	ToggleDoor();
}

void ATallowDoor::UpdateDoorRotation(float OutputValue) 
{
	// Lerp between closed (0.0f) and open (OpenAngle) based on the timeline curve
	float CurrentYaw = FMath::Lerp(0.0f, OpenAngle, OutputValue);
	HingePivot->SetRelativeRotation(FRotator(0.0f, CurrentYaw, 0.0f));
}

void ATallowDoor::ToggleDoor()
{
	if (bIsOpen)
	{
		DoorTimeline->Reverse();
	}
	else
	{
		DoorTimeline->Play();
	}
	bIsOpen = !bIsOpen;
}

void ATallowDoor::ResetDoor()
{
	// reset meshes and states.
	DoorTimeline->Stop();
	DoorTimeline->SetPlaybackPosition(0.0f, false);
	bIsOpen = false;
	BarricadePlanks = InitialPlanks;
	
	for (ATallowPlank* Plank : InitialPlanks)
	{
		if (Plank)
		{
			Plank->ResetPlankState();
		}
	}
}




