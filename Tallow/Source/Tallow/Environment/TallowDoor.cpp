#include "TallowDoor.h"

#include "AI/TallowCreature.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TimelineComponent.h"
#include "Player/TallowCharacter.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "Presentation/TallowNellNotes.h"
#include "Presentation/TallowPlayDirector.h"
#include "Kismet/GameplayStatics.h"
#include "Tasks/TallowPlank.h"
#include "Components/BoxComponent.h"
#include "Tasks/TallowLockpickGame.h"
#include "Navigation/TallowDoorNavModifier.h"

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
	NavigationBarrier = CreateDefaultSubobject<UTallowDoorNavModifier>(TEXT("NavigationBarrier"));
	
	DoorTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DoorTimeline"));
}

// Called when the game starts or when spawned
void ATallowDoor::BeginPlay()
{
	Super::BeginPlay();
	
	InitialPlanks = BarricadePlanks;
	bInitiallyLocked = bIsLocked;
	UpdateNavigationBarrier();
	UpdateLeafNavigation(bIsOpen);
	
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
	// A sealed door still answers the player (it rattles); only planks refuse outright.
	if (!bChapterSealed && IsBarricaded()) { return false; }
	
	return true;
}

FText ATallowDoor::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
	if (bChapterSealed) return FText::FromString(TEXT("BOLTED"));
	if (IsBarricaded()) { return FText::FromString("BARRICADED"); }
	
	if (bIsLocked && bOpensFromInside && IsInside(Interactor)) { return FText::FromString("UNBOLT"); }
	if (bIsLocked) { return FText::FromString("DOOR LOCKED: UNLOCK"); }
	
	if (!bIsOpen) { return FText::FromString("OPEN DOOR"); }
	
	return FText::FromString("CLOSE DOOR");
}

void ATallowDoor::NotifyPlankRemoved(ATallowPlank* Plank)
{
	BarricadePlanks.Remove(Plank);
	UpdateNavigationBarrier();
}

void ATallowDoor::UpdateNavigationBarrier()
{
	if (NavigationBarrier)
		NavigationBarrier->SetNavigationRelevancy(bChapterSealed || bIsLocked || IsBarricaded() || (DoorMesh && DoorMesh->CanEverAffectNavigation()));
}

void ATallowDoor::UpdateLeafNavigation(bool bAtRestOpen)
{
	// A shut leaf stays out of the navmesh (AI opens doors by walking up to them). An open leaf at rest is a
	// real wall in the room: as navigation geometry the navmesh is eroded round it like any wall, so routes
	// pass its free end instead of running through it (the study and bedroom leaves rest across the room).
	if (DoorMesh && DoorMesh->CanEverAffectNavigation() != bAtRestOpen)
	{
		DoorMesh->SetCanEverAffectNavigation(bAtRestOpen);
		if (NavigationBarrier) NavigationBarrier->RefreshLeafBounds();
		UpdateNavigationBarrier();
	}
}

void ATallowDoor::SetLocked(bool bLocked)
{
	bIsLocked = bLocked;
	UpdateNavigationBarrier();
}

void ATallowDoor::OnInteract_Implementation(AActor* Interactor)
{
	if (!Interactor) { return; }
	ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) { return; }
	if (bChapterSealed)
	{
		RattleBolted(Player);
		return;
	}
	
	if (bIsLocked && bOpensFromInside && IsInside(Player))
	{
		// From the room side it is only a bolt.
		bIsLocked = false;
		UpdateNavigationBarrier();
		OnDoorUnlocked.Broadcast();
	}
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

bool ATallowDoor::IsInside(const AActor* Actor) const
{
	if (!Actor) return false;
	const FVector Inward = GetActorTransform().TransformVectorNoScale(InsideDirection.GetSafeNormal());
	return FVector::DotProduct(Actor->GetActorLocation() - GetActorLocation(), Inward) > 0.f;
}

void ATallowDoor::RattleBolted(ATallowCharacter* Player)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastRattleTime < 1.0) { return; }
	LastRattleTime = Now;
	
	const FVector Where = GetActorLocation();
	if (ATallowPlayDirector* Director = Cast<ATallowPlayDirector>(UGameplayStatics::GetActorOfClass(this, ATallowPlayDirector::StaticClass())))
	{
		Director->PlayChapterCue(TEXT("P_Latch"), 0.25f, &Where);
	}
	if (UTallowNoiseSubsystem* NoiseSubsystem = GetWorld()->GetSubsystem<UTallowNoiseSubsystem>())
	{
		NoiseSubsystem->EmitNoise(ETallowNoiseType::DoorClose, Where, Player, BoltedRattleLoudness, true);
	}
	if (UTallowNellNotesComponent* Notes = UTallowNellNotesComponent::Find(this))
	{
		Notes->NoticeBolted();
	}
}

void ATallowDoor::OnInteractComplete_Implementation(AActor* Interactor)
{
	if (!Interactor || bChapterSealed) { return; }
	
	bIsLocked = false;
	UpdateNavigationBarrier();
	OnDoorUnlocked.Broadcast();
	
	OnInteract_Implementation(Interactor);
}

void ATallowDoor::UpdateDoorRotation(float OutputValue) 
{
	// Lerp between closed (0.0f) and open (OpenAngle) based on the timeline curve
	float CurrentYaw = FMath::Lerp(0.0f, OpenAngle, OutputValue);
	HingePivot->SetRelativeRotation(FRotator(0.0f, CurrentYaw, 0.0f));
	UpdateLeafNavigation(bIsOpen && OutputValue >= .98f);
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
	if (!bIsOpen) UpdateLeafNavigation(false);
}

void ATallowDoor::ResetDoor()
{
	// reset meshes and states.
	bChapterSealed = false;
	if (DoorTimeline)
	{
		DoorTimeline->Stop();
		DoorTimeline->SetPlaybackPosition(0.0f, false);
	}
	bIsOpen = false;
	UpdateLeafNavigation(false);
	BarricadePlanks.Empty();
	bIsLocked = bInitiallyLocked;
	
	for (ATallowPlank* Plank : InitialPlanks)
	{
		if (IsValid(Plank))
		{
			BarricadePlanks.Add(Plank);
			Plank->ResetPlankState();
		}
	}
	UpdateNavigationBarrier();
}

void ATallowDoor::OnAIProximityBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ATallowCreature* Creature = Cast<ATallowCreature>(OtherActor);
	if (!Creature || !DoorTimeline) { return; }
	
	if (bChapterSealed || IsBarricaded() || bIsLocked) { return; } // only works with un-barricaded, unlocked
	
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




void ATallowDoor::ApplyChapterState(ETallowDoorState State)
{
    bChapterSealed = State == ETallowDoorState::StoryGated;
    bIsLocked = State == ETallowDoorState::Lockpick;
    bIsOpen = State == ETallowDoorState::Open;
    BarricadePlanks.Reset();
    for (ATallowPlank* Plank : InitialPlanks)
    {
        if (!IsValid(Plank)) continue;
        Plank->SetChapterRetired(State != ETallowDoorState::Crowbar);
        if (State == ETallowDoorState::Crowbar) BarricadePlanks.Add(Plank);
    }
    if (DoorTimeline)
    {
        DoorTimeline->Stop();
        DoorTimeline->SetPlaybackPosition(bIsOpen ? DoorTimeline->GetTimelineLength() : 0.f, false);
    }
    UpdateDoorRotation(bIsOpen ? 1.f : 0.f);
    UpdateLeafNavigation(bIsOpen);
    UpdateNavigationBarrier();
}
