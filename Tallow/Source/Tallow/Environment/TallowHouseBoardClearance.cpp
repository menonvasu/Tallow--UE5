#include "Environment/TallowHouseBoardClearance.h"
#include "Tasks/TallowPlank.h"
#include "Components/StaticMeshComponent.h"

UTallowHouseBoardClearance::UTallowHouseBoardClearance()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = .1f;
}
void UTallowHouseBoardClearance::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Plank = Cast<ATallowPlank>(GetOwner()))
    {
        Board = Plank->FindComponentByClass<UStaticMeshComponent>();
        if (Board.IsValid())
        {
            OriginalPawnResponse = Board->GetCollisionResponseToChannel(ECC_Pawn);
            bOriginalNavigation = Board->CanEverAffectNavigation();
            Plank->OnPlankDestroyed.AddUniqueDynamic(this, &ThisClass::OnBoardBroken);
        }
    }
}
void UTallowHouseBoardClearance::OnBoardBroken(bool bBroken)
{
    if (!bBroken) { RestoreBlocking(); return; }
    if (!Board.IsValid()) return;
    bDebris = true;
    Board->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
    Board->SetCanEverAffectNavigation(false);
}
void UTallowHouseBoardClearance::RestoreBlocking()
{
    if (!Board.IsValid()) return;
    Board->SetCollisionResponseToChannel(ECC_Pawn,OriginalPawnResponse);
    Board->SetCanEverAffectNavigation(bOriginalNavigation);
    bDebris = false;
}
void UTallowHouseBoardClearance::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
    // ResetPlankState restores visibility collision when putting boards back.
    // The original class exposes no reset delegate; observe that actual state.
    if (bDebris && Board.IsValid() && Board->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block) RestoreBlocking();
}
