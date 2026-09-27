#include "TallowEQSContext_PreviousSearch.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "AI/TallowAIController.h"
#include "GameFramework/Actor.h"

void UTallowEQSContext_PreviousSearch::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();
	if (!QueryOwner) return;

	const ATallowAIController* AICon = Cast<ATallowAIController>(QueryOwner);
	if (!AICon)
	{
		if (const APawn* Pawn = Cast<APawn>(QueryOwner))
		{
			AICon = Pawn->GetController<ATallowAIController>();
		}
	}

	if (!AICon) return;

	UEnvQueryItemType_Point::SetContextHelper(ContextData, AICon->LastSearchLocation);
}