#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TallowCreature.generated.h"

class ATallowPatrolPoint;

UCLASS()
class TALLOW_API ATallowCreature : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this AI's properties
	ATallowCreature();

	/** Pawn BeginPlay implementation */
	virtual void BeginPlay() override;

	/** Set starting patrol point */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "AI|Patrol")
	TObjectPtr<ATallowPatrolPoint> StartingPatrolPoint;
};
