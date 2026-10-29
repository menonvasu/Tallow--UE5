#include "House/TallowDoorLayoutSubsystem.h"
#include "Chapter/TallowChapterDefinition.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs LayoutDraw(TEXT("Tallow.Layout.Draw"),TEXT("Draw measured room connectivity for <seconds>."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A,UWorld* W){if(auto* S=UTallowDoorLayoutSubsystem::Get(W)) S->DrawConnectivity(A.IsEmpty()?20.f:FCString::Atof(*A[0]));}));
static FAutoConsoleCommandWithWorldAndArgs LayoutDump(TEXT("Tallow.Layout.Dump"),TEXT("Log live door states."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&,UWorld* W){if(auto* S=UTallowDoorLayoutSubsystem::Get(W)){TArray<FString> Lines;S->DescribeLive(Lines);for(const auto& L:Lines) UE_LOG(LogTemp,Display,TEXT("TALLOW_LAYOUT %s"),*L);}}));
static FAutoConsoleCommandWithWorldAndArgs LayoutSolve(TEXT("Tallow.Layout.Solve"),TEXT("Log a layout for <seed> <chapter>, without applying it."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A,UWorld* W)
{
    auto* S=UTallowDoorLayoutSubsystem::Get(W);if(!S) return;
    const int32 Seed=A.Num()?FCString::Atoi(*A[0]):1,Index=A.Num()>1?FCString::Atoi(*A[1]):0;
    auto* Campaign=UTallowCampaignDefinition::LoadOrBuildDefault(W);if(!Campaign||!Campaign->Chapters.IsValidIndex(Index)) return;
    FTallowSolverInput I;S->GatherSolverInput(TEXT("wake"),I);auto Rules=Campaign->Chapters[Index]->MakeRules(Index);auto L=FTallowLayoutSolver::Solve(I,Rules,Seed,{});TArray<FString> Problems;
    UE_LOG(LogTemp,Display,TEXT("TALLOW_LAYOUT solve seed=%d chapter=%d valid=%d attempt=%d battery=%s chair=%s"),Seed,Index,FTallowLayoutSolver::Validate(I,Rules,L,{},Problems),L.Attempt,*L.BatterySpot.ToString(),*L.ChairSpot.ToString());
    for(const auto& P:Problems) UE_LOG(LogTemp,Display,TEXT("TALLOW_LAYOUT %s"),*P);
}));
#endif
