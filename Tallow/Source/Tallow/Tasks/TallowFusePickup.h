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
	
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	
	virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "FuseBox|Events")
	FOnFusePickedUp OnFusePickedUp;
	
	UFUNCTION(BlueprintCallable, Category = "FuseBox|Reset")
	void ResetPickup();
	void SetDormant(bool bDormant);
	void SetChapterNoun(const FText& Noun) { ChapterNoun = Noun; }
	
protected:
	bool bChapterDormant = false;
	FText ChapterNoun;
	/** The fuse mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FuseBoxPickup")
	TObjectPtr<UStaticMeshComponent> FuseMesh;
};
