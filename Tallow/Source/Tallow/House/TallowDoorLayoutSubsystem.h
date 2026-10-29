#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "House/TallowLayoutSolver.h"
#include "TallowDoorLayoutSubsystem.generated.h"

UCLASS()
class TALLOW_API UTallowDoorLayoutSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	static UTallowDoorLayoutSubsystem* Get(const UObject* WorldContext);
	void GatherSolverInput(FName StartRoom, FTallowSolverInput& Out) const;
	void ApplyLayout(const FTallowChapterLayout& Layout);
	void SetChapterControl(bool bControlled);
	bool IsChapterControlled() const { return bChapterControl; }
	ETallowDoorState GetLiveState(FName EdgeId) const;
	void DescribeLive(TArray<FString>& OutLines) const;
	void DrawConnectivity(float Seconds) const;   // body empty in shipping
protected:
	bool bChapterControl = false;
	FTallowChapterLayout AppliedLayout;
};


