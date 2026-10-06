#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowFusePickup.generated.h"

class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFusePickedUp);

UCLASS()
class TALLOW_API ATallowFusePickup : public AActor, public IInteractable
{
	GENERATED_BODY()
	
public:
	ATallowFusePickup();
	
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "FuseBox|Events")
	FOnFusePickedUp OnFusePickedUp;
	
	UFUNCTION(BlueprintCallable, Category = "FuseBox|Reset")
	void ResetPickup();
	
protected:
	/** The fuse mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FuseBoxPickup")
	TObjectPtr<UStaticMeshComponent> FuseMesh;
};
