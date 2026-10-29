#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TallowScareProxy.generated.h"

class UStaticMeshComponent;

/** A movable stand-in for a house prop during a sight scare: it copies the prop's look while the prop itself is hidden,
 *  moves where a static prop cannot, and stays where it ends up until the chapter resets (the director removes it). */
UCLASS(NotPlaceable, Transient)
class TALLOW_API ATallowScareProxy : public AActor
{
    GENERATED_BODY()
public:
    ATallowScareProxy();
    virtual void Tick(float DeltaSeconds) override;
    void CopyLook(const UStaticMeshComponent* Source);
    /** Swing on a nail: about Axis through Nail, Degrees at first, dying away until it hangs SettleDegrees off true. */
    void Swing(const FVector& Nail, const FVector& Axis, float Degrees, float SettleDegrees, float Seconds);
    /** Run along the floor from A to B: away quickly, then slowing to a stop, the wheels kept on the boards. */
    void Run(const FVector& From, const FVector& To, float Seconds, float FloorToPivot);
    bool IsMoving() const { return Age < Seconds; }
    UPROPERTY(VisibleAnywhere, Category="Scare") TObjectPtr<USceneComponent> Pivot;
    UPROPERTY(VisibleAnywhere, Category="Scare") TObjectPtr<UStaticMeshComponent> Mesh;
private:
    enum class EMotion : uint8 { None, Swing, Run };
    EMotion Motion = EMotion::None;
    FVector Axis = FVector::UpVector, From = FVector::ZeroVector, To = FVector::ZeroVector;
    FQuat Rest = FQuat::Identity;
    float Degrees = 0.f, Settle = 0.f, Seconds = 0.f, Age = 0.f, Lift = 0.f, FloorZ = 0.f;
};
