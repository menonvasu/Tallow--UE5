#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowFuseBox.generated.h"

class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFuseInserted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNothingInserted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPowerRestored);

UCLASS()
class TALLOW_API ATallowFuseBox : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	/** Sets default values for this actor's properties */
	ATallowFuseBox();
	
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	
	/** Required fuses */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FuseBox|Tunables")
	int32 RequiredFuses = 3;
	
	/** Fuse Insert Loudness */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FuseBox|Tunables")
	float FuseInsertLoudness = 1.0f;
	
	/** Current fuse count to toggle visibility */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FuseBox|State")
	int32 CurrentFuses = 0;
	
	/** Events */
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "FuseBox|Events")
	FOnFuseInserted OnFuseInserted;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "FuseBox|Events")
	FOnFuseInserted OnPowerRestored;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "FuseBox|Events")
	FOnNothingInserted OnNothingInserted;
	
protected:
	/** Called when the game starts */
	virtual void BeginPlay() override;
	
	/** Box mesh with fuse slots */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FuseBox")
	TObjectPtr<UStaticMeshComponent> BoxMesh;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FuseBox")
	TObjectPtr<UStaticMeshComponent> FuseMesh1;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FuseBox")
	TObjectPtr<UStaticMeshComponent> FuseMesh2;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FuseBox")
	TObjectPtr<UStaticMeshComponent> FuseMesh3;
};
