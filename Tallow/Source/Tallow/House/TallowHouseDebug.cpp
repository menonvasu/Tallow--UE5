#include "House/TallowRoomSearchSubsystem.h"
#include "House/TallowHouseRooms.h"
#include "Scare/TallowScareDirector.h"
#include "Plush/TallowPlushMim.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#if !UE_BUILD_SHIPPING
namespace
{
void Print(const TArray<FString>& Lines) { for (const auto& Line : Lines) UE_LOG(LogTemp, Display, TEXT("%s"), *Line); }
FAutoConsoleCommandWithWorldAndArgs SearchDump(TEXT("Tallow.Search.Dump"), TEXT("Room visits and this round's search progress"), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
{
    if (const auto* S = UTallowRoomSearchSubsystem::Get(World)) for (const auto& R : TallowHouseRooms::All())
    {
        const auto State = S->GetRoomState(R.Id);
        UE_LOG(LogTemp, Display, TEXT("TALLOW_SEARCH room=%s visited=%d searched=%d here=%d progress=%.3f"), *R.Id.ToString(), State.bVisited, State.bSearched, State.bHere, State.Progress);
    }
}));
FAutoConsoleCommandWithWorldAndArgs SearchDraw(TEXT("Tallow.Search.Draw"), TEXT("Draw search state for ten seconds"), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World) { if (const auto* S = UTallowRoomSearchSubsystem::Get(World)) S->Draw(10.f); }));
FAutoConsoleCommandWithWorldAndArgs ScareDump(TEXT("Tallow.Scare.Dump"), TEXT("Scare count and cooldowns"), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World) { if (const auto* S = UTallowScareDirector::Get(World)) { TArray<FString> Lines; S->Describe(Lines); Print(Lines); } }));
FAutoConsoleCommandWithWorldAndArgs ScareFire(TEXT("Tallow.Scare.Fire"), TEXT("Try a named group while respecting phase, pacing and cooldowns"), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { if (auto* S = UTallowScareDirector::Get(World); S && !Args.IsEmpty()) S->TryFireGroup(FName(*Args[0])); }));
FAutoConsoleCommandWithWorldAndArgs PlushDump(TEXT("Tallow.Plush.Dump"), TEXT("Plush state and perch"), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World) { for (TActorIterator<ATallowPlushMim> It(World); It; ++It) { TArray<FString> Lines; It->Describe(Lines); Print(Lines); } }));
FAutoConsoleCommandWithWorldAndArgs PlushRelocate(TEXT("Tallow.Plush.Relocate"), TEXT("Attempt a safe unseen relocation, respecting its timer"), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World) { for (TActorIterator<ATallowPlushMim> It(World); It; ++It) It->TryRelocate(); }));
}
#endif
