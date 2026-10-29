#include "Chapter/TallowChapterFlow.h"

bool FTallowChapterFlow::CanTransition(ETallowChapterKind Kind, ETallowChapterPhase From, ETallowChapterPhase To)
{
    using P = ETallowChapterPhase;
    if (From == To || To == P::None || To == P::Paused) return false;
    if (Kind == ETallowChapterKind::Final)
        return ((From == P::None || From == P::Caught || From == P::ChapterTransition) && To == P::ActiveHunt)
            || (From == P::ActiveHunt && (To == P::FinalSequence || To == P::Caught))
            || (From == P::BatteryAcquired && To == P::Caught);
    switch (From)
    {
    case P::None: case P::Caught: case P::ChapterTransition: return To == P::InitialRestraint || To == P::Exploration;
    case P::InitialRestraint: return To == P::Exploration;
    case P::Exploration: return To == P::StoryInteraction || To == P::Transformation;
    case P::StoryInteraction: return To == P::Transformation;
    case P::Transformation: return To == P::Restraint;
    case P::Restraint: return To == P::HuntPreparation;
    case P::HuntPreparation: return To == P::ActiveHunt;
    case P::ActiveHunt: return To == P::BatteryAcquired || To == P::FuseSequence || To == P::Caught;
    case P::BatteryAcquired: return To == P::ActiveHunt || To == P::FuseSequence || To == P::Caught;
    case P::FuseSequence: return To == P::ChapterTransition;
    default: return false;
    }
}
ETallowChapterPhase FTallowChapterFlow::OnEvent(ETallowChapterKind Kind, ETallowChapterPhase Current, ETallowChapterEvent Event, bool bPlayerInChairRoom)
{
    using P = ETallowChapterPhase; using E = ETallowChapterEvent;
    if (Event == E::Caught && (Current == P::ActiveHunt || Current == P::BatteryAcquired)) return P::Caught;
    if (Kind == ETallowChapterKind::Final) return Current == P::ActiveHunt && Event == E::FinalReached ? P::FinalSequence : Current;
    switch (Event)
    {
    case E::ChairEscaped: if (Current == P::InitialRestraint) return P::Exploration; if (Current == P::HuntPreparation) return P::ActiveHunt; break;
    case E::StoryStarted: if (Current == P::Exploration) return P::StoryInteraction; break;
    case E::StoryClueDone: if ((Current == P::Exploration || Current == P::StoryInteraction) && bPlayerInChairRoom) return P::Transformation; break;
    case E::BatteryCollected: if (Current == P::ActiveHunt) return P::BatteryAcquired; break;
    case E::BatteryDropped: case E::BatteryLost: if (Current == P::BatteryAcquired) return P::ActiveHunt; break;
    case E::FuseActivated: if (Current == P::ActiveHunt || Current == P::BatteryAcquired) return P::FuseSequence; break;
    default: break;
    }
    return Current;
}
int32 FTallowChapterFlow::ChapterSeed(int32 CampaignSeed, int32 Index)
{
    return (HashCombine(GetTypeHash(CampaignSeed), GetTypeHash(Index * 7919 + 17)) & 0x7fffffff) | 1;
}
