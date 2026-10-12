#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TallowDoor.generated.h"

UCLASS()
class TALLOW_API ATallowDoor : public AActor
{
	GENERATED_BODY()

public:
	ATallowDoor();

protected:
	virtual void BeginPlay() override;
	
};
