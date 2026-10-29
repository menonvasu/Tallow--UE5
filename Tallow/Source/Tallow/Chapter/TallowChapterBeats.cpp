#include "Chapter/TallowChapterBeats.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
bool FTallowChapterBeatState::Claim(ETallowChapterBeat Beat, int32 Serial, uint64 Frame)
{
    if (!Matches(Beat, Serial) || Frame != RequestedFrame || FallbackAt >= 0.0) return false;
    bClaimed = true; return true;
}
void TallowChapterBeats::Fade(const UObject* Context, bool bOut, float Seconds)
{
    if (auto* PC = UGameplayStatics::GetPlayerController(Context, 0); PC && PC->PlayerCameraManager)
        PC->PlayerCameraManager->StartCameraFade(bOut ? 0.f : 1.f, bOut ? 1.f : 0.f, Seconds, FColor::Black, false, bOut);
}
