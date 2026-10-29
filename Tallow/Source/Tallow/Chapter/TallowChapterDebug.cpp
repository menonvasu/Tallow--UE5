#include "Chapter/TallowChapterSubsystem.h"
#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
namespace
{
    UTallowChapterSubsystem* Chapters(UWorld* World) { return UTallowChapterSubsystem::GetActive(World); }
    FAutoConsoleCommandWithWorldAndArgs Jump(TEXT("Tallow.Chapter.Jump"),TEXT("Jump to a chapter index and optional phase."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
    {
        if (auto* C=Chapters(World); C && Args.Num())
        {
            auto Phase=ETallowChapterPhase::None;
            if (Args.Num()>1) { const int64 Value=StaticEnum<ETallowChapterPhase>()->GetValueByNameString(Args[1]); if (Value!=INDEX_NONE) Phase=ETallowChapterPhase(Value); }
            C->JumpToChapter(FCString::Atoi(*Args[0]),Phase);
        }
    }));
    FAutoConsoleCommandWithWorldAndArgs Simulate(TEXT("Tallow.Chapter.Simulate"),TEXT("Send story, clue, escape, battery, fuse, caught or final."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
    {
        if (auto* C=Chapters(World); C && Args.Num())
        {
            const TMap<FString,ETallowChapterEvent> Events={{TEXT("story"),ETallowChapterEvent::StoryStarted},{TEXT("clue"),ETallowChapterEvent::StoryClueDone},{TEXT("escape"),ETallowChapterEvent::ChairEscaped},{TEXT("battery"),ETallowChapterEvent::BatteryCollected},{TEXT("fuse"),ETallowChapterEvent::FuseActivated},{TEXT("caught"),ETallowChapterEvent::Caught},{TEXT("final"),ETallowChapterEvent::FinalReached}};
            if (const auto* Event=Events.Find(Args[0].ToLower())) C->NotifyEvent(*Event);
        }
    }));
    FAutoConsoleCommandWithWorldAndArgs Restart(TEXT("Tallow.Chapter.Restart"),TEXT("Rebuild the chapter from its snapshot."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&,UWorld* World){if(auto* C=Chapters(World))C->RestartChapter(ETallowRestartReason::Debug);}));
    FAutoConsoleCommandWithWorldAndArgs Dump(TEXT("Tallow.Chapter.Dump"),TEXT("Describe chapter, resources and pacing."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&,UWorld* World){if(auto* C=Chapters(World))UE_LOG(LogTemp,Display,TEXT("%s"),*C->DescribeState());}));
    FAutoConsoleCommandWithWorldAndArgs Skip(TEXT("Tallow.Chapter.SkipBeat"),TEXT("Finish the current beat at its cut."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&,UWorld* World){if(auto* C=Chapters(World))C->SkipBeat();}));
    FAutoConsoleCommandWithWorldAndArgs MimDump(TEXT("Tallow.Mim.Dump"),TEXT("Describe Mim form, hunt and walking distance."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&,UWorld* World){if(auto* C=Chapters(World))UE_LOG(LogTemp,Display,TEXT("%s"),*C->DescribeState());}));
}
#endif
