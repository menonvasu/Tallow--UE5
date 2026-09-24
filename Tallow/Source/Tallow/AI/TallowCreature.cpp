#include "AI/TallowCreature.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
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
}

void ATallowCreature::BeginPlay()
{
	Super::BeginPlay();

	if (AutoPossessAI != EAutoPossessAI::Disabled && Controller == nullptr)
	{
		SpawnDefaultController();
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
