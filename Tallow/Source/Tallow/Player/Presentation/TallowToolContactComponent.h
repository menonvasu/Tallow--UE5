#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowToolContactComponent.generated.h"

class ATallowCharacter;
class UTallowPlayerPresentationComponent;
class UStaticMeshComponent;
class UTallowInteractionComponent;

/** Keeps the animated grip intact while seating the iron's hooked tip on the board. */
UCLASS(ClassGroup=(Tallow))
class TALLOW_API UTallowToolContactComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UTallowToolContactComponent();
 void Setup(ATallowCharacter* Player, UTallowPlayerPresentationComponent* Presentation, UStaticMeshComponent* Tool);
 virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
 void PlayProjectorImpulse(bool bInserting) { ProjectorImpulseAge=0.f; ProjectorImpulseDuration=bInserting?.55f:.32f; }
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UFUNCTION(BlueprintPure, Category="Player|Contact") FVector GetRenderedTipLocation() const;
 UFUNCTION(BlueprintPure, Category="Player|Contact") float GetContactWeight() const { return Weight; }
 /** Hook endpoint from the tool's authored mesh, in imported centimetres. */
 UPROPERTY(EditAnywhere, Category="Player|Contact") FVector CrowbarTip = FVector(7.7f,0.f,36.5f);
 /** The hook is only seated on a board this close to where it is drawn; a farther pry plays in place. */
 UPROPERTY(EditAnywhere, Category="Player|Contact") float ContactReachCm = 25.f;
 /** Largest camera-space shift of the hands rig while seating. The rig is a first-person primitive, so pushing it
  *  out to the real board would draw it at FirstPersonScale: a miniature pair of arms at the board. */
 UPROPERTY(EditAnywhere, Category="Player|Contact") float MaxSeatCm = 8.f;
private:
 FVector ProjectPoint(const FVector& Point, bool bInverse) const;
 void RestoreRoot();
 TWeakObjectPtr<ATallowCharacter> Player;
 TWeakObjectPtr<UTallowPlayerPresentationComponent> Presentation;
 TWeakObjectPtr<UStaticMeshComponent> Tool;
 TWeakObjectPtr<UTallowInteractionComponent> Interaction;
 TWeakObjectPtr<AActor> LastTarget;
 TWeakObjectPtr<AActor> ReachTarget;
 bool bReachJudged = false;
 bool bInReach = false;
 FTransform LastApplied;
 FVector Offset = FVector::ZeroVector;
 FVector Target = FVector::ZeroVector;
 float Weight = 0.f;
 float ProjectorImpulseAge=-1.f, ProjectorImpulseDuration=.32f;
 float StridePhase = 0.f;
 FVector LocomotionOffset = FVector::ZeroVector;
};
