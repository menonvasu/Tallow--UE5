#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowOptionalBoards.generated.h"

class UStaticMeshComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOptionalBoardsOpened, bool, bByMim);

/** Route variation deliberately separate from the required workshop plank counter. */
UCLASS()
class TALLOW_API ATallowOptionalBoards : public AActor, public IInteractable
{
    GENERATED_BODY()
public:
    ATallowOptionalBoards();
    virtual void BeginPlay() override;
    void SetChapterControlled(bool bControlled) { bChapterControlled = bControlled; }
    void ApplyChapterBlocked(bool bBlocked);
    virtual void Tick(float DeltaSeconds) override;
    virtual bool CanInteract_Implementation(AActor* Interactor) const override;
    virtual float GetInteractionDuration_Implementation() const override;
    virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;
    virtual void OnInteractTick_Implementation(AActor* Interactor, float DeltaSeconds, float Progress) override;
    virtual void OnInteractComplete_Implementation(AActor* Interactor) override;
    virtual void OnInteractCancel_Implementation(AActor* Interactor) override;

    /** Inventory adapters can override this without replacing the environment actor. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="House|Route") bool HasPryTool(AActor* Interactor) const;
    virtual bool HasPryTool_Implementation(AActor* Interactor) const;
    UFUNCTION(BlueprintPure, Category="House|Route") bool IsRouteBlocked() const { return bSelected && !bRemoved; }
    UFUNCTION(BlueprintPure, Category="House|Route") bool IsSelectedForRun() const { return bSelected; }
    /** Whether this variant is boarded on the run with this seed (the same roll Tick makes). */
    UFUNCTION(BlueprintPure, Category="House|Route") bool IsSelectedForSeed(int32 Seed) const;
    UPROPERTY(BlueprintAssignable, Category="House|Route") FOnOptionalBoardsOpened OnOptionalBoardsOpened;
    UPROPERTY(VisibleAnywhere, Category="House|Route") TObjectPtr<UStaticMeshComponent> Boards;
    UPROPERTY(EditAnywhere, Category="House|Route") FName RouteGroup = TEXT("StudyLoop");
    UPROPERTY(EditAnywhere, Category="House|Route") int32 VariantIndex = 0;
    UPROPERTY(EditAnywhere, Category="House|Route") int32 VariantCount = 2;
    UPROPERTY(EditAnywhere, Category="House|Route") float PrySeconds = 2.6f;
    /** Keep boarded doors blocked unless an authored breach is explicitly enabled. */
    UPROPERTY(EditAnywhere, Category="House|Route") bool bAllowMimBreach = false;
    UPROPERTY(EditAnywhere, Category="House|Route") float MimBreachSeconds = 5.f;
private:
    void RemoveBoards(bool bByMim, AActor* Responsible);
    void SetBoardVisibility();
    bool bChapterControlled = false;
    bool bInitialized = false;
    bool bSelected = false;
    bool bRemoved = false;
    bool bLastTied = true;
    float NoiseAge = 0.f;
    float BreachAge = 0.f;
    float ScanAge = 0.f;
    TWeakObjectPtr<AActor> NearbyMim;
};
