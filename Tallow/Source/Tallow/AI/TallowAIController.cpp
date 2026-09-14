#include "AI/TallowAIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Components/StateTreeComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AIPerceptionTypes.h"

ATallowAIController::ATallowAIController()
{
	// Createa and set components
	AIPerceptionComponent = this->CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	SetPerceptionComponent(*AIPerceptionComponent);

	StateTreeComponent = this->CreateDefaultSubobject<UStateTreeComponent>(TEXT("StateTree"));

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

void ATallowAIController::BeginPlay()
{
	Super::BeginPlay();
	AIPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &ATallowAIController::HandleTargetPerceptionUpdated);
}

void ATallowAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (Stimulus.WasSuccessfullySensed() && Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
	{
		LastHeardNoiseLocation = Stimulus.StimulusLocation;

		StateTreeComponent->SendStateTreeEvent(FStateTreeEvent(NoiseHeardTag));
	
	}

}
