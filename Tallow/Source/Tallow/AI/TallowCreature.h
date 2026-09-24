#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TallowCreature.generated.h"

class ATallowPatrolPoint;

/** The creature's two forms. Phase two is the spider form (#25) */
UENUM(BlueprintType)
enum class ETallowCreaturePhase : uint8
{
	PhaseOne	UMETA(DisplayName = "Phase One"),
	PhaseTwo	UMETA(DisplayName = "Phase Two")
};

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

	/** Switch form. Phase two can follow the player through child gaps (#23) */
	UFUNCTION(BlueprintCallable, Category = "AI|Phase")
	void SetPhase(ETallowCreaturePhase NewPhase);

	/** Get current form */
	UFUNCTION(BlueprintPure, Category = "AI|Phase")
	ETallowCreaturePhase GetPhase() const { return Phase; }

protected:

	/** Current form. Set per instance to start a creature in phase two */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AI|Phase")
	ETallowCreaturePhase Phase = ETallowCreaturePhase::PhaseOne;
};
