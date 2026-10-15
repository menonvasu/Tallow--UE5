#include "TallowDoor.h"

#include "AI/TallowCreature.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TimelineComponent.h"
#include "Player/TallowCharacter.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "Tasks/TallowPlank.h"
#include "Components/BoxComponent.h"
#include "Tasks/TallowLockpickGame.h"

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
	DoorMesh->SetCanEverAffectNavigation(false);
	
	AIProximityBox = CreateDefaultSubobject<UBoxComponent>(TEXT("AIProximityBox"));
	AIProximityBox->SetupAttachment(RootComponent);
	AIProximityBox->SetBoxExtent(FVector(80.0f, 70.0f, 120.0f));
	AIProximityBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AIProximityBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // only wake up for pawn
	AIProximityBox->SetCanEverAffectNavigation(false);
	
	DoorTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DoorTimeline"));
}

// Called when the game starts or when spawned
void ATallowDoor::BeginPlay()
{
	Super::BeginPlay();
	
	InitialPlanks = BarricadePlanks;
	
	if (DoorTimeline && DoorTimelineCurve)
	{
		TimelineProgress.BindDynamic(this, &ATallowDoor::UpdateDoorRotation);
		DoorTimeline->AddInterpFloat(DoorTimelineCurve, TimelineProgress);	
	}
	
	if (AIProximityBox)
	{
		AIProximityBox->OnComponentBeginOverlap.AddDynamic(this, &ATallowDoor::OnAIProximityBeginOverlap);
	}
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
	
	if (bIsLocked)
	{
		if (UTallowLockpickGame* Lockpick = Player->GetLockpickComponent())
		{
			Lockpick->StartLockpickSession(this, 3);
		}
		return;
	}
	
	UTallowNoiseSubsystem* NoiseSubsystem = GetWorld()->GetSubsystem<UTallowNoiseSubsystem>();
	 
	// if door closed
	if (!bIsOpen)
	{
		if (Player->GetVelocity().Size() > Player->Walkspeed)
		{
			// slam door!
			if (NoiseSubsystem)
			{
				NoiseSubsystem->EmitNoise(ETallowNoiseType::DoorSlam, GetActorLocation(), Player, DoorSlamLoudness, true);
			}
		}
		else
		{
			// open door
			if (NoiseSubsystem)
			{
				NoiseSubsystem->EmitNoise(ETallowNoiseType::DoorOpen, GetActorLocation(), Player, DoorOpenLoudness, false);
			}
		}
	}
	// if open - close
	else
	{
		if (NoiseSubsystem)
		{
			NoiseSubsystem->EmitNoise(ETallowNoiseType::DoorClose, GetActorLocation(), Player, DoorCloseLoudness, true);	
		}
	}
	
	ToggleDoor(Player);
}

void ATallowDoor::OnInteractComplete_Implementation(AActor* Interactor)
{
	if (!Interactor) { return; }
	
	bIsLocked = false;
	OnDoorUnlocked.Broadcast();
	
	OnInteract_Implementation(Interactor);
}

void ATallowDoor::UpdateDoorRotation(float OutputValue) 
{
	// Lerp between closed (0.0f) and open (OpenAngle) based on the timeline curve
	float CurrentYaw = FMath::Lerp(0.0f, OpenAngle, OutputValue);
	HingePivot->SetRelativeRotation(FRotator(0.0f, CurrentYaw, 0.0f));
}

void ATallowDoor::ToggleDoor(ATallowCharacter* Player)
{
	if (!Player || !DoorTimeline) return;
	
	if (!bIsOpen)
	{
		// Opening 
		const bool bIsSlamming = Player->GetVelocity().Size() > Player->Walkspeed;
		DoorTimeline->SetPlayRate(bIsSlamming ? DoorSlamPlayRate : DoorOpenPlayRate);
		DoorTimeline->Play();
	}
	else
	{
		// Closing
		DoorTimeline->SetPlayRate(DoorClosePlayRate);
		DoorTimeline->Reverse();
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

void ATallowDoor::OnAIProximityBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ATallowCreature* Creature = Cast<ATallowCreature>(OtherActor);
	if (!Creature || !DoorTimeline) { return; }
	
	if (IsBarricaded()) { return; } // only works with un-barricaded
	
	UTallowNoiseSubsystem* NoiseSubsystem = GetWorld()->GetSubsystem<UTallowNoiseSubsystem>();
	
	if (!bIsOpen)
	{
		
		if (const bool bIsChasing = (Creature->CurrentState == ETallowCreatureState::Chasing))
		{
			DoorTimeline->SetPlayRate(DoorSlamPlayRate);
			if (NoiseSubsystem)
			{
				NoiseSubsystem->EmitNoise(ETallowNoiseType::DoorSlam, GetActorLocation(), Creature, DoorSlamLoudness, false);
			}
		}
		else
		{
			DoorTimeline->SetPlayRate(DoorOpenPlayRate);
			if (NoiseSubsystem)
			{
				NoiseSubsystem->EmitNoise(ETallowNoiseType::DoorOpen, GetActorLocation(), Creature, DoorOpenLoudness, false);	
			}
		}
		
		DoorTimeline->Play();
		bIsOpen = true;
	}
}



