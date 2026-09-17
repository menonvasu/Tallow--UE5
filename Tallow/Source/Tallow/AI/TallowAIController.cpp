#include "AI/TallowAIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "TallowCreature.h"
#include "Perception/AIPerceptionTypes.h"

ATallowAIController::ATallowAIController()
{
	// Createa and set components
	AIPerceptionComponent = this->CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	SetPerceptionComponent(*AIPerceptionComponent);

	StateTreeAIComponent = this->CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTree"));
	StateTreeAIComponent->SetStartLogicAutomatically(false);

	// Hearing Params
	HearingConfig = this->CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	if (HearingConfig)
	{
		HearingConfig->HearingRange = 3000.0f;
		HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
		HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
		HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	
		AIPerceptionComponent->ConfigureSense(*HearingConfig);
		AIPerceptionComponent->SetDominantSense(HearingConfig->GetSenseImplementation());
	}
}

// Dynamic reflection on target perception 
void ATallowAIController::BeginPlay()
{
	Super::BeginPlay();
	AIPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &ATallowAIController::HandleTargetPerceptionUpdated);
}

// Set starting patrol point
void ATallowAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (ATallowCreature* Creature = Cast<ATallowCreature>(InPawn))
	{
		CurrentPatrolPoint = Creature->StartingPatrolPoint;
	}

	if (StateTreeAIComponent)
	{
		StateTreeAIComponent->StartLogic();
	}
}

// Call event on noise heard
void ATallowAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (Stimulus.WasSuccessfullySensed() && Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
	{
		LastHeardNoiseLocation = Stimulus.StimulusLocation;
		StateTreeAIComponent->SendStateTreeEvent(FStateTreeEvent(NoiseHeardTag));
	}
}
