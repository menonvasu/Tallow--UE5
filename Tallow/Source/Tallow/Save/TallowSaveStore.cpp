#include "Save/TallowSaveStore.h"
#include "Save/TallowSaveGame.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
	constexpr int32 MaxPayloadBytes = 16 * 1024 * 1024;
	void AppendWord(TArray<uint8>& Bytes, uint32 Word)
	{
		for (int32 Shift = 0; Shift < 32; Shift += 8) Bytes.Add(static_cast<uint8>(Word >> Shift));
	}
	uint32 ReadWord(const TArray<uint8>& Bytes, int32 Offset)
	{
		uint32 Word = 0;
		for (int32 I = 0; I < 4; ++I) Word |= uint32(Bytes[Offset + I]) << (8 * I);
		return Word;
	}
}

FTallowSaveStore::FTallowSaveStore(FString InDirectory, FMoveFile InMove)
	: Directory(MoveTemp(InDirectory)), MoveFile(MoveTemp(InMove)) {}

FString FTallowSaveStore::DefaultDirectory()
{
	FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
#if !UE_BUILD_SHIPPING
	FParse::Value(FCommandLine::Get(), TEXT("TallowSaveDir="), Path);
#endif
	return FPaths::ConvertRelativePathToFull(Path);
}

FString FTallowSaveStore::Filename() const { return FPaths::Combine(Directory, TEXT("TallowCampaign.sav")); }

bool FTallowSaveStore::Move(const FString& Destination, const FString& Source) const
{
	return MoveFile ? MoveFile(Destination, Source) : IFileManager::Get().Move(*Destination, *Source, true, false, false, true);
}

UTallowSaveGame* FTallowSaveStore::ReadFile(const FString& Path, FString& OutReason) const
{
	const int64 Size = IFileManager::Get().FileSize(*Path);
	if (Size < 0) { OutReason = TEXT("missing"); return nullptr; }
	if (Size < HeaderBytes || Size > MaxPayloadBytes + HeaderBytes) { OutReason = TEXT("length"); return nullptr; }
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path)) { OutReason = TEXT("read"); return nullptr; }
	if (Bytes.Num() < HeaderBytes) { OutReason = TEXT("length"); return nullptr; }
	if (FMemory::Memcmp(Bytes.GetData(), "TLW2", 4) != 0) { OutReason = TEXT("magic"); return nullptr; }
	if (ReadWord(Bytes, 4) != FormatVersion) { OutReason = TEXT("format version"); return nullptr; }
	if (ReadWord(Bytes, 8) != uint32(Bytes.Num() - HeaderBytes)) { OutReason = TEXT("length"); return nullptr; }
	const uint32 Crc = FCrc::MemCrc32(Bytes.GetData() + HeaderBytes, Bytes.Num() - HeaderBytes);
	if (ReadWord(Bytes, 12) != Crc) { OutReason = TEXT("CRC"); return nullptr; }
	TArray<uint8> Payload;
	Payload.Append(Bytes.GetData() + HeaderBytes, Bytes.Num() - HeaderBytes);
	auto* Save = Cast<UTallowSaveGame>(UGameplayStatics::LoadGameFromMemory(Payload));
	if (!Save) { OutReason = TEXT("payload"); return nullptr; }
	if (Save->SaveVersion != SaveVersion || Save->Campaign.Version != 1) { OutReason = TEXT("save version"); return nullptr; }
	OutReason = TEXT("valid");
	return Save;
}

UTallowSaveGame* FTallowSaveStore::Load(FString& OutReason) const
{
	// An interrupted write is never a checkpoint, even when both committed files are missing.
	IFileManager::Get().Delete(*(Filename() + TEXT(".tmp")), false, false, true);
	FString PrimaryReason;
	if (auto* Save = ReadFile(Filename(), PrimaryReason)) { OutReason = TEXT("primary"); return Save; }
	FString BackupReason;
	if (auto* Save = ReadFile(Filename() + TEXT(".bak"), BackupReason))
	{
		OutReason = FString::Printf(TEXT("backup (primary rejected: %s)"), *PrimaryReason);
		return Save;
	}
	OutReason = FString::Printf(TEXT("primary %s; backup %s"), *PrimaryReason, *BackupReason);
	return nullptr;
}

bool FTallowSaveStore::Write(UTallowSaveGame* Save, int32& OutBytes, FString& OutReason) const
{
	OutBytes = 0;
	TArray<uint8> Payload;
	if (!Save || !UGameplayStatics::SaveGameToMemory(Save, Payload) || Payload.Num() > MaxPayloadBytes)
	{
		OutReason = TEXT("serialize"); return false;
	}
	TArray<uint8> Bytes;
	Bytes.Append(reinterpret_cast<const uint8*>("TLW2"), 4);
	AppendWord(Bytes, FormatVersion);
	AppendWord(Bytes, Payload.Num());
	AppendWord(Bytes, FCrc::MemCrc32(Payload.GetData(), Payload.Num()));
	Bytes.Append(Payload);
	const FString Primary = Filename(), Temp = Primary + TEXT(".tmp"), Backup = Primary + TEXT(".bak");
	auto& Files = IFileManager::Get();
	if (!Files.MakeDirectory(*Directory, true) || !FFileHelper::SaveArrayToFile(Bytes, *Temp))
	{
		Files.Delete(*Temp, false, false, true);
		OutReason = TEXT("temporary write"); return false;
	}
	const bool bHadPrimary = Files.FileExists(*Primary);
	FString Reason;
	// Never rotate a damaged primary over the only good backup.
	const bool bKeepBackup = bHadPrimary && !ReadFile(Primary, Reason) && ReadFile(Backup, Reason);
	const FString Previous = bKeepBackup ? Primary + TEXT(".old") : Backup;
	if (bHadPrimary && !Move(Previous, Primary))
	{
		Files.Delete(*Temp, false, false, true);
		OutReason = TEXT("backup move"); return false;
	}
	if (!Move(Primary, Temp))
	{
		// The previous file remains recoverable if the filesystem also refuses the rollback.
		const bool bRestored = !bHadPrimary || Move(Primary, Previous);
		Files.Delete(*Temp, false, false, true);
		OutReason = bRestored ? TEXT("commit move; previous restored") : TEXT("commit move; previous retained in backup");
		return false;
	}
	if (bKeepBackup) Files.Delete(*Previous, false, false, true);
	OutBytes = Bytes.Num();
	OutReason = TEXT("written");
	return true;
}

bool FTallowSaveStore::Delete() const
{
	bool bDeleted = true;
	for (const TCHAR* Suffix : { TEXT(""), TEXT(".bak"), TEXT(".tmp"), TEXT(".old") })
		bDeleted = IFileManager::Get().Delete(*(Filename() + Suffix), false, false, true) && bDeleted;
	return bDeleted;
}
