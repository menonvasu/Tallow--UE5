#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowFuseBox.generated.h"

class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFuseInserted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFuseCurrentlyBeingInserted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNothingInserted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPowerRestored);

UCLASS()
class TALLOW_API ATallowFuseBox : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	/** Sets default values for this actor's properties */
	ATallowFuseBox();
	
	virtual float GetInteractionDuration_Implementation() const override;
	
	virtual void OnInteractStart_Implementation(AActor* Actor) override;

	virtual void OnInteractComplete_Implementation(AActor* Interactor) override;

	virtual void OnInteractCancel_Implementation(AActor* Interactor) override;
	
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	
	virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;
	
	/** Required fuses */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FuseBox|Tunables")
	int32 RequiredFuses = 3;
	
	/** Hold duration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FuseBox|Tunables")
	float InsertionHoldDuration = 1.5f;
	
	/** Fuse Insert Loudness */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FuseBox|Tunables")
	float FuseInsertLoudness = 1.0f;
	
	/** Current fuse count to toggle visibility */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FuseBox|State")
	int32 CurrentFuses = 0;
	
	bool bHasPower = false;
	
	/** Events */
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "FuseBox|Events")
	FOnFuseInserted OnFuseInserted;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "FuseBox|Events")
	FOnFuseInserted OnPowerRestored;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "FuseBox|Events")
	FOnNothingInserted OnNothingInserted;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "FuseBox|Events")
	FOnFuseCurrentlyBeingInserted OnFuseCurrentlyBeingInserted;
	
	UFUNCTION(BlueprintCallable, Category = "FuseBox|Reset")
	void ResetVisual();
	
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
