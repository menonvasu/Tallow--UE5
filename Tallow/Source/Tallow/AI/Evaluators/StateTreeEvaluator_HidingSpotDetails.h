#pragma once

#include "CoreMinimal.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeLinker.h"
#include "StateTreeEvaluator_HidingSpotDetails.generated.h"

enum class ETallowHidingSpotType : uint8;

USTRUCT()
struct FStateTreeEvaluator_HidingSpotDetailsInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta = (TypeRefFilter = "Actor"))
	TObjectPtr<AActor> HideActor = nullptr;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LastEvaluatedActor = nullptr;

	UPROPERTY(EditAnywhere, Category = "Output")
	FVector ApproachLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Output")
	ETallowHidingSpotType SpotType = (ETallowHidingSpotType)0;
};

USTRUCT(meta = (DisplayName = "AI Hiding Spot Data Evaluator"))
struct FStateTreeEvaluator_HidingSpotDetails : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	virtual const UStruct* GetInstanceDataType() const override { return FStateTreeEvaluator_HidingSpotDetailsInstanceData::StaticStruct(); }
	
	virtual bool Link(FStateTreeLinker& Linker) override { return true; }

	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};