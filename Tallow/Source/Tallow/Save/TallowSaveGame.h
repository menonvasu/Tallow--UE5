#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Chapter/TallowChapterTypes.h"
#include "TallowSaveGame.generated.h"

UCLASS()
class TALLOW_API UTallowSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() int32 SaveVersion = 1;
	UPROPERTY() FString BuildId;
	UPROPERTY() FDateTime SavedAt;
	UPROPERTY() FTallowCampaignState Campaign;
};


