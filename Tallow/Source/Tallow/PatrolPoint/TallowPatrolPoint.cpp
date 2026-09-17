#include "PatrolPoint/TallowPatrolPoint.h"
#include "Components/BillboardComponent.h"

// Sets default values
ATallowPatrolPoint::ATallowPatrolPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	Point = CreateDefaultSubobject<UBillboardComponent>(TEXT("BillBoard"));
	RootComponent = Point;
}

// Returns the next available point in the array
ATallowPatrolPoint* ATallowPatrolPoint::GetNextPoint() const
{
	if (NextPoints.IsEmpty())
	{
		return nullptr;
	}

	// if one point, return it - otherwise pick at random 
	int32 randPoint = FMath::RandHelper(NextPoints.Num());
	if (NextPoints[randPoint] == nullptr) return nullptr;
	return NextPoints[randPoint];
}	

