#include "AI/TallowAIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AIPerceptionTypes.h"
#include "TallowCreature.h"
#include "Navigation/TallowNavFilter_PhaseOne.h"

ATallowAIController::ATallowAIController()
{
	// Createa and set components
	AIPerceptionComponent = this->CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	SetPerceptionComponent(*AIPerceptionComponent);

	StateTreeAIComponent = this->CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTree"));
	StateTreeAIComponent->SetStartLogicAutomatically(false);

	// Phase one stays out of the child gaps, phase two has no restriction
	PhaseOneNavigationFilter = UTallowNavFilter_PhaseOne::StaticClass();

	// Hearing Params
	HearingConfig = this->CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	if (HearingConfig)
	{
		HearingConfig->HearingRange = 3000.0f;
		HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
		HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
		HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	
		AIPerceptionComponent->ConfigureSense(*HearingConfig);
	}

	// Sight Params
	SightConfig = this->CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	if (SightConfig)
	{
		SightConfig->SightRadius = 2000.0f;
		SightConfig->LoseSightRadius = 2500.0f;
		SightConfig->PeripheralVisionAngleDegrees = 60.0f;
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

		AIPerceptionComponent->ConfigureSense(*SightConfig);
		AIPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
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
		ApplyPhaseNavigationFilter(Creature->GetPhase());
	}

	if (StateTreeAIComponent)
	{
		StateTreeAIComponent->StartLogic();
	}
}

// Move To tasks fall back to DefaultNavigationFilterClass, so this is all the
// State Tree needs to respect the phase
void ATallowAIController::ApplyPhaseNavigationFilter(ETallowCreaturePhase Phase)
{
	DefaultNavigationFilterClass = (Phase == ETallowCreaturePhase::PhaseOne)
		? PhaseOneNavigationFilter
		: PhaseTwoNavigationFilter;
}

// Call event on noise heard or sight seen
void ATallowAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	const FAISenseID HearingSenseID = UAISense::GetSenseID<UAISense_Hearing>();
	const FAISenseID SightSenseID = UAISense::GetSenseID<UAISense_Sight>();

	if (Stimulus.WasSuccessfullySensed() && Stimulus.Type == HearingSenseID)
	{
		LastHeardNoiseLocation = Stimulus.StimulusLocation;
		StateTreeAIComponent->SendStateTreeEvent(FStateTreeEvent(NoiseHeardTag));
	}

	else if (Stimulus.Type == SightSenseID)
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			TargetActor = Actor;
			StateTreeAIComponent->SendStateTreeEvent(FStateTreeEvent(PlayerSpottedTag));
		}
		else
		{
			LastHeardNoiseLocation = Stimulus.StimulusLocation;
			TargetActor = nullptr;
			StateTreeAIComponent->SendStateTreeEvent(FStateTreeEvent(PlayerLostTag));
		}
	}
}
