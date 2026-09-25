#include "AI/TallowCreature.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SphereComponent.h"
#include "TallowAIController.h"

ATallowCreature::ATallowCreature()
{
	// Sets tick to false
	PrimaryActorTick.bCanEverTick = false;

	// Set size for collision capsule (Default)
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// Set AI controller class
	AIControllerClass = ATallowAIController::StaticClass();

	// Set possession
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	// Setup grab sphere
	GrabSphere = CreateDefaultSubobject<USphereComponent>(TEXT("Grab Sphere"));
	GrabSphere->SetupAttachment(GetCapsuleComponent());
	GrabSphere->SetSphereRadius(85.0f);
	GrabSphere->SetCollisionProfileName(TEXT("Trigger"));
}

void ATallowCreature::BeginPlay()
{
	Super::BeginPlay();

	if (AutoPossessAI != EAutoPossessAI::Disabled && Controller == nullptr)
	{
		SpawnDefaultController();
	}

	InitialSpawnTransform = GetActorTransform();
	
	if (GrabSphere)
	{
		GrabSphere->OnComponentBeginOverlap.AddDynamic(this, &ATallowCreature::OnOverlapBegin);
	}
}

void ATallowCreature::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ATallowAIController* AICon = GetController<ATallowAIController>())
	{
		AICon->NotifyPlayerTouched(OtherActor);
	}
}

void ATallowCreature::ResetToSpawn()
{
	SetActorTransform(InitialSpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}

	if (ATallowAIController* AICon = GetController<ATallowAIController>())
	{
		AICon->ResetBrain();
	}
}



void ATallowCreature::SetPhase(ETallowCreaturePhase NewPhase)
{
	Phase = NewPhase;

	// Swap which routes the controller is allowed to path through
	if (ATallowAIController* AIController = Cast<ATallowAIController>(GetController()))
	{
		AIController->ApplyPhaseNavigationFilter(Phase);
	}
}
