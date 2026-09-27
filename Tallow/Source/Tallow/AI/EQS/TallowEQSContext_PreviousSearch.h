#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "TallowEQSContext_PreviousSearch.generated.h"

UCLASS()
class TALLOW_API UTallowEQSContext_PreviousSearch : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	// Overriding the function to provide a list of actors
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};