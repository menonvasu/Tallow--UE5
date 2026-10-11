#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Player/TallowCharacter.h"
#include "TallowItemDefinition.generated.h"

enum class ECarriedItemType : uint8;
class UStaticMesh;
class AActor;

UCLASS()
class TALLOW_API UTallowItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType ItemDataType;
	
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(ItemDataType, GetFName());
	}
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data" )
	FText ItemDisplayName;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data" )
	ECarriedItemType ItemType;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data" )
	TSubclassOf<AActor> PickupClass;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data" )
	TObjectPtr<UStaticMesh> HeldMesh;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data" )
	FTransform HeldRelativeTransform;
};
