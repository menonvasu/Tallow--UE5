#pragma once

#include "CoreMinimal.h"

class UTallowSaveGame;

/** The checkpoint files. A directory and optional move function keep file tests isolated. */
struct TALLOW_API FTallowSaveStore
{
	using FMoveFile = TFunction<bool(const FString& Destination, const FString& Source)>;
	explicit FTallowSaveStore(FString InDirectory, FMoveFile InMove = {});
	static FString DefaultDirectory();
	FString Filename() const;
	bool Write(UTallowSaveGame* Save, int32& OutBytes, FString& OutReason) const;
	UTallowSaveGame* Load(FString& OutReason) const;
	bool Delete() const;
	static constexpr uint32 FormatVersion = 1;
	static constexpr int32 SaveVersion = 1;
	static constexpr int32 HeaderBytes = 16;

private:
	FString Directory;
	FMoveFile MoveFile;
	bool Move(const FString& Destination, const FString& Source) const;
	UTallowSaveGame* ReadFile(const FString& Path, FString& OutReason) const;
};
