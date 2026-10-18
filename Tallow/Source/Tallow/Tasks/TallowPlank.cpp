#include "TallowPlank.h"
#include "Components/StaticMeshComponent.h"
#include "Player/TallowCharacter.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "Environment/TallowDoor.h"

ATallowPlank::ATallowPlank()
{
	PrimaryActorTick.bCanEverTick = false;
	
	// Sets default values
	PlankMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlankMesh"));
	RootComponent = PlankMesh;
	PlankMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PlankMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECR_Block);
}

// Called when the game starts or when spawned
void ATallowPlank::BeginPlay()
{
	Super::BeginPlay();

	InitialPlankTransform = GetActorTransform();
}

float ATallowPlank::GetInteractionDuration_Implementation() const
{
	if (bBroken) { return 0.0f; }
	
	return PryHoldDuration;
}

bool ATallowPlank::CanInteract_Implementation(AActor* Interactor) const
{
	if (!Interactor) { return false; }
	const ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) { return false; }
	
	if (bBroken) return false;
	
	// can interact if holding crowbar
	if (Player->GetCarriedItem() == ECarriedItemType::Crowbar)
	{
		return true;
	}
	return false;
}

void ATallowPlank::OnInteractStart_Implementation(AActor* Interactor) 
{
	// broadcast pry anim & sound
	OnPryPlank.Broadcast();
	
	TimeSinceLastNoise = 0.0f;
}

void ATallowPlank::OnInteractComplete_Implementation(AActor* Interactor)
{
	// broadcast broken anim & sound
	bBroken = true;
	OnPlankDestroyed.Broadcast(bBroken);
	
	// detach and clatter on floorboard
	if (PlankMesh)
	{
		PlankMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PlankMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECR_Ignore);
		PlankMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		PlankMesh->SetSimulatePhysics(true);
		PlankMesh->AddImpulse(GetActorForwardVector() * 300.0f, NAME_None, true);
	}
	
	if (TargetDoor)
	{
		TargetDoor->NotifyPlankRemoved(this);
	}
}

void ATallowPlank::OnInteractTick_Implementation(AActor* Interactor, float DeltaTime, float Progress)
{
	if (!Interactor) { return; }
	ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) { return; }
	
	// if already played noise, return early
	TimeSinceLastNoise += DeltaTime;
	if (TimeSinceLastNoise < NoisePerTime)
	{
		return;
	}
	
	// broadcast currently being pry-ed sound
	OnPlankBeingBroken.Broadcast();
	
	// emit noise continually for duration
	if (UTallowNoiseSubsystem* NoiseSubsystem = GetWorld()->GetSubsystem<UTallowNoiseSubsystem>())
	{
		NoiseSubsystem->EmitNoise(ETallowNoiseType::Pry, GetActorLocation(), Player, PlankPryLoudness, true);
	}
	// reset timer
	TimeSinceLastNoise = 0.0f;
}

void ATallowPlank::OnInteractCancel_Implementation(AActor* Interactor)
{
	ResetPlankState();
	
	TimeSinceLastNoise = 0.0f;
}

void ATallowPlank::ResetPlankState()
{
	if (!PlankMesh) { return; }
	
	PlankMesh->SetSimulatePhysics(false);
	PlankMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PlankMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECR_Block);
	SetActorTransform(InitialPlankTransform);
	
	bBroken = false;
	TimeSinceLastNoise = 0.0f;
}

FText ATallowPlank::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
	if (!Interactor) return FText::GetEmpty();
	const ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) return FText::GetEmpty();
	
	// if not holding crowbar
	if (!Player->HasCarriedItem(ECarriedItemType::Crowbar))
	{
		return FText::FromString("REQUIRES CROWBAR");
	}
	return FText::FromString("PRY PLANK");
}



