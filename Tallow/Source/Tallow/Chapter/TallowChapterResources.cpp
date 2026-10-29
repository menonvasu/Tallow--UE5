#include "Chapter/TallowChapterResources.h"
#include "Candle/TallowCandleComponent.h"
#include "Player/TallowCharacter.h"
#include "Player/Map/TallowHouseMapComponent.h"
FTallowResourceSnapshot FTallowChapterResources::Capture(const ATallowCharacter* Player)
{
    FTallowResourceSnapshot Out;
    if (!Player) return Out;
    if (auto* Candle = Player->GetCandleComponent())
    {
        Out.Matches = Candle->GetMatchCount(); Out.Wax = Candle->GetWaxFraction(); Out.bHasCandle = Candle->IsAcquired();
    }
    if (auto* Map = Player->FindComponentByClass<UTallowHouseMapComponent>()) Out.bHasMap = Map->HasMap();
    Out.bHasCrowbar = Player->GetInventoryCount(ECarriedItemType::Crowbar) > 0;
    return Out;
}
FTallowResourceSnapshot FTallowChapterResources::ForNewChapter(const FTallowResourceSnapshot& Carried, const FTallowResourceRules& Rules)
{
    auto Out = Carried;
    const int32 Max = FMath::Max(0, Rules.MaxCarriedMatches);
    Out.Matches = FMath::Clamp(Carried.Matches, FMath::Clamp(Rules.MinMatchesAtStart, 0, Max), Max);
    Out.Wax = FMath::Clamp(FMath::Max(Carried.Wax, Rules.MinWaxAtStart), 0.f, 1.f);
    Out.bHasCrowbar = Rules.bKeepCrowbar && Carried.bHasCrowbar;
    return Out;
}
void FTallowChapterResources::Restore(ATallowCharacter* Player, const FTallowResourceSnapshot& Snapshot)
{
    if (!Player) return;
    Player->ResetInventory();
    if (Snapshot.bHasCrowbar) { Player->SetCarriedItem(ECarriedItemType::Crowbar); Player->StowCarriedItem(); }
    if (auto* Candle = Player->GetCandleComponent()) Candle->RestoreForChapter(Snapshot.bHasCandle, Snapshot.Wax, Snapshot.Matches, ECandleState::Out);
    if (auto* Map = Player->FindComponentByClass<UTallowHouseMapComponent>()) Map->RestoreCollectedForChapter(Snapshot.bHasMap);
}
