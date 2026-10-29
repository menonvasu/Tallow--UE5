#include "Chapter/TallowChapterTypes.h"
#include "Misc/Crc.h"
ETallowDoorState FTallowChapterLayout::GetDoorState(FName Edge, ETallowDoorState Fallback) const
{
    for (const auto& Door : Doors) if (Door.Edge == Edge) return Door.State;
    return Fallback;
}
int32 FTallowChapterLayout::DoorHash() const
{
    uint32 Hash = 0;
    TArray<FTallowDoorAssignment> Sorted = Doors;
    Sorted.Sort([](const FTallowDoorAssignment& A, const FTallowDoorAssignment& B) { return A.Edge.LexicalLess(B.Edge); });
    for (const auto& Door : Sorted)
        Hash = HashCombine(Hash, HashCombine(FCrc::StrCrc32(*Door.Edge.ToString()), GetTypeHash(static_cast<uint8>(Door.State))));
    return static_cast<int32>(Hash & 0x7fffffff);
}
