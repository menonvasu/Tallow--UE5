#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TallowPatrolPoint.generated.h"

class UBillboardComponent;

UCLASS()
class TALLOW_API ATallowPatrolPoint : public AActor
{
	GENERATED_BODY()
	
public:	

	/** Sets default values for this actor's properties */
	ATallowPatrolPoint();

	/** Helper to return next patrol point*/
	UFUNCTION(BlueprintPure, Category = "Patrol")
	ATallowPatrolPoint* GetNextPoint() const;

protected:

	/** Billboard Point */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Comps")
	TObjectPtr<UBillboardComponent> Point;

	/** Array of patrol points */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Comps")
	TArray<TObjectPtr<ATallowPatrolPoint>> NextPoints;
};
