#pragma once
#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/Hunter/TallowMimMovementContract.h"
#include "TallowMimCharacterMovementComponent.generated.h"

/** CharacterMovement is the physical executor. It measures ordinary moves through the
 *  body contract (LastContractResult) but never vetoes them; see the travel policy in
 *  TallowMimMovementContract.h. ContractRejections counts torso/head brush steps
 *  (kept under its old name for test/report compatibility -- it is diagnostic only). */
UCLASS()
class TALLOW_API UTallowMimCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mim|Movement") FTallowMimTransitionResult LastContractResult;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mim|Movement") int32 ContractChecks = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mim|Movement") int32 ContractRejections = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mim|Movement") bool LastActualCapsuleBlocked = false;
protected:
    virtual bool MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation,
        bool bSweep, FHitResult* OutHit = nullptr, ETeleportType Teleport = ETeleportType::None) override;
};
