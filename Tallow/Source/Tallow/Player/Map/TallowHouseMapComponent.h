#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "House/TallowHouseRooms.h"
#include "TallowHouseMapComponent.generated.h"
class ATallowCharacter;
class ATallowDoor;
class UTallowHouseMapWidget;
using FTallowMapRoom = FTallowRoomDef;
UCLASS(ClassGroup=(Tallow))
class TALLOW_API UTallowHouseMapComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UTallowHouseMapComponent();
 virtual void BeginPlay()override;
 virtual void EndPlay(const EEndPlayReason::Type Reason)override;
 virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)override;
 UFUNCTION(BlueprintCallable,Category="Map")bool CollectMap();
 UFUNCTION(BlueprintCallable,Category="Map")void ToggleMap();
 UFUNCTION(BlueprintPure,Category="Map")bool HasMap()const{return bCollected;}
 void RestoreCollectedForChapter(bool bInCollected) { bCollected = bInCollected; }
 UFUNCTION(BlueprintPure,Category="Map")bool IsMapOpen()const;
 UFUNCTION(BlueprintPure,Category="Map")int32 GetDiscoveredRoomCount()const;
 UFUNCTION(BlueprintPure,Category="Map")int32 GetKnownDoorCount()const{return KnownDoors.Num();}
 UFUNCTION(BlueprintPure,Category="Map")int32 GetCurrentFloor()const;
 FName GetCurrentRoom()const;
 bool KnowsRoom(FName Id)const;
 FString GetClue()const;
 static const TArray<FTallowMapRoom>& Rooms();
 const TArray<TWeakObjectPtr<ATallowDoor>>& GetKnownDoors()const{return KnownDoors;}
 void PlayGesture(bool bPuttingAway);
private:
 UFUNCTION()void OnCaught();
 TWeakObjectPtr<ATallowCharacter> Player;
 UPROPERTY(Transient)TObjectPtr<UTallowHouseMapWidget> Widget;
 TSet<FName>Discovered;
 TArray<TWeakObjectPtr<ATallowDoor>>KnownDoors;
 bool bCollected=false;
};
