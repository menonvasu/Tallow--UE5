#pragma once
#include "CoreMinimal.h"
#include "House/TallowHouseGraph.h"
#include "TallowLayoutSolver.generated.h"

USTRUCT(BlueprintType)
struct TALLOW_API FTallowSpotCandidate
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Spot;
	UPROPERTY(BlueprintReadOnly) FName Room;
	UPROPERTY(BlueprintReadOnly) ETallowSpotKind Kind = ETallowSpotKind::Battery;
	UPROPERTY(BlueprintReadOnly) int32 ChapterIndex = -1;
	UPROPERTY(BlueprintReadOnly) FTransform Transform;
};
USTRUCT(BlueprintType)
struct TALLOW_API FTallowSolverInput
{
	GENERATED_BODY()
	UPROPERTY() FTallowHouseGraph Graph;
	UPROPERTY() TArray<FTallowSpotCandidate> Spots;
	UPROPERTY() FName StartRoom;
	UPROPERTY() FName FuseBoxRoom;
	UPROPERTY() bool bPowerRestored = false;
};
struct TALLOW_API FTallowLayoutSolver
{
	static constexpr int32 MaxAttempts = 64;
	static FTallowChapterLayout Solve(const FTallowSolverInput& Input, const FTallowChapterRules& Rules, int32 Seed, const FTallowLayoutHistory& History);
	static bool Validate(const FTallowSolverInput& Input, const FTallowChapterRules& Rules, const FTallowChapterLayout& Layout, const FTallowLayoutHistory& History, TArray<FString>& OutProblems);
	static TSet<FName> Reachable(const FTallowHouseGraph& Graph, const FTallowChapterLayout& Layout, FName From, bool bHasCrowbar, bool bPowerRestored);
	static bool Retarget(const FTallowSolverInput& Input, const FTallowChapterRules& Rules, FName NewChairRoom, int32 Seed, const FTallowLayoutHistory& History, FTallowChapterLayout& InOutLayout);
	static FTallowChapterLayout SafeFallback(const FTallowSolverInput& Input, const FTallowChapterRules& Rules);
};
