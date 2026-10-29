#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TallowMenuScene.generated.h"

class ACameraActor;
class ALight;
class ATallowPlayDirector;
class UAnimSequence;
class UAudioComponent;
class USkeletalMeshComponent;
class USoundBase;

/** One sight of her face while the candle flares: where one of the two Mim puppets is, and in which pose. */
USTRUCT(BlueprintType)
struct FTallowMenuFlashFrame
{
	GENERATED_BODY()
	/** 0 or 1: each frame has its own puppet, so the second is posed before the first is shown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") int32 Puppet = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") FTransform Transform;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") TObjectPtr<UAnimSequence> Pose;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") float PoseTime = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") float Seconds = .35f;
};

/** One appearance: the sights of her between the candle's dips, its sting, and what is wrong when it is calm again. */
USTRUCT(BlueprintType)
struct FTallowMenuApparition
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") TArray<FTallowMenuFlashFrame> Frames;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") TObjectPtr<USoundBase> Sting;
	/** Back on the calm shot the chair stands empty this long; then the candle gutters and she is there again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") float EmptyChairSeconds = 0.f;
};

/** A light of the calm shot: scaled for the close-up while the menu is up, put back as it was for the intro. */
USTRUCT(BlueprintType)
struct FTallowMenuLight
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light") TObjectPtr<ALight> Light;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light") float MenuScale = 1.f;
	/** The candle's own light: it flickers, and it gutters when she comes back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light") bool bCandle = false;
};

/** The main menu's picture (28 Oct: "a close-up of the girl in the chair, and then it flashes black and white
 *  and Mim pops out"; 29 Oct, after playing it: "the light flickers, Mim's face is there, it flickers again, she's
 *  gone"). Nell sits tied in the candlelight while the waltz plays. After a while on the menu, never on a key press
 *  and never the same moment twice, the candle dips nearly out, flares, and Mim's face is there in the dark beyond
 *  her, half caught by the flame and nothing else; it dips again and flares on nothing. Only her face is drawn. Owns only this
 *  presentation: the director still owns the menu, its input and the start of the game, and once a run starts
 *  everything here is put back as it was. */
UCLASS()
class TALLOW_API ATallowMenuScene : public AActor
{
	GENERATED_BODY()
public:
	ATallowMenuScene();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene") TObjectPtr<USkeletalMeshComponent> Mim;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene") TObjectPtr<USkeletalMeshComponent> MimClose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene") TObjectPtr<ACameraActor> Camera;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene") TObjectPtr<AActor> Nell;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene") TArray<FTallowMenuLight> Lights;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene") float FlickerAmount = .12f;
	/** Flicker steps like everything else that moves on the menu (tallow.OnTwos). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene") float FlickerRate = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound") TObjectPtr<USoundBase> Music;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound") float MusicFadeIn = 3.f;
	/** The sting rings on long after the picture is calm; it is faded from here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound") float StingSeconds = 4.f;
	/** Volume of the flash sting on top of the asset's own gain: a startle a few dB over the waltz, not a blast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound") float StingVolume = .25f;
	/** The waltz drops away for the flicker and this long after, then creeps back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound") float MusicReturnDelay = .8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") TArray<FTallowMenuApparition> Apparitions;
	/** Seconds of quiet on the menu before the first appearance, then between later ones (random within). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") FVector2D FirstDelay = FVector2D(20.f, 40.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") FVector2D LaterDelay = FVector2D(70.f, 140.f);
	/** Never within this long of a key press or click, so it never lands on the player's own input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") float QuietSeconds = 1.5f;
	/** Puppets are posed this long before they are shown (stepped animation and texture streaming catch up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") float StageLead = .4f;
	/** Each sight of her is on screen for at least this many rendered frames, even after a hitch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") int32 MinFrames = 3;
	/** The candle's dip: how long, and how much of the light is left (the candle and the room's other lights). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") float DipSeconds = .15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") float DipLevel = .05f;
	/** The candle's flare after each dip, and how long the last one takes to settle with her gone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") float FlareLevel = 1.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") float SettleSeconds = .35f;
	/** Her eyes' own glow (EyeGlowGain, 32 in the house). Here they are black, and the flame's glint is all they show. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") float EyeGlow = 0.f;
	/** Her material slots that are drawn; the rest of her (fur, hands) is never seen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparition") TArray<FName> FaceSlots = {
		TEXT("M_Mim_Skin"), TEXT("M_Mim_Eye"), TEXT("M_Mim_EyeBack"), TEXT("M_Mim_Teeth"), TEXT("M_Mim_Gums"),
		TEXT("M_Mim_Mouth"), TEXT("M_Mim_HoodBase")};

	/** Starts appearance Index now (-1: the next one), as the menu would. */
	UFUNCTION(BlueprintCallable, Category = "Apparition") void Appear(int32 Index = -1);
	/** Review: hold appearance Index on one step: -1 calm, 0.. a sight of her, then (frame count) the dip, (+1) the
	 *  flare she is gone from, (+2) the empty chair. */
	UFUNCTION(BlueprintCallable, Category = "Apparition|Review") void Hold(int32 Index, int32 Step);
	UFUNCTION(BlueprintCallable, Category = "Apparition|Review") void Release();
	UFUNCTION(BlueprintPure, Category = "Apparition|Review") bool IsAppearing() const { return Step != EStep::Calm; }
	UFUNCTION(BlueprintPure, Category = "Apparition|Review") FString GetStepName() const;
	UFUNCTION(BlueprintPure, Category = "Apparition|Review") float GetSecondsUntilNext() const { return FMath::Max(0.f, NextAt - Waited); }
	UFUNCTION(BlueprintPure, Category = "Apparition|Review") int32 GetAppearanceCount() const { return Shown; }

private:
	enum class EStep : uint8 { Calm, Staged, Dip, Frame, Settle, Empty, Gutter };
	struct FLightState { float Original = 0.f; };

	void UpdateMenuState(float DeltaSeconds);
	/** The candle dips; afterwards her face (frame Next) or, past the last, the flare she is gone from. */
	void EnterDip(int32 Next);
	void EnterFrame(int32 Index);
	void BeginFlicker();
	/** Back to the calm shot; unless bQuiet, with whatever this appearance leaves wrong afterwards. */
	void EndFlicker(bool bQuiet = false);
	void Pose(const FTallowMenuFlashFrame& Frame);
	void ShowPuppet(int32 Puppet);
	/** Only FaceSlots are drawn, on every LOD, and her eyes keep no glow of their own. */
	void PrepareFace(USkeletalMeshComponent* Puppet) const;
	void SetLights(float RoomScale);
	void PutBack();
	/** No temporal history across the cut, so the flash and the return are hard cuts, not a smear. */
	void CutThisFrame() const;
	void Schedule(const FVector2D& Range);
	USkeletalMeshComponent* Puppet(int32 Index) const;

	UPROPERTY(Transient) TObjectPtr<ATallowPlayDirector> Director;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> StingVoice;
	TArray<FLightState> LightStates;
	EStep Step = EStep::Calm;
	int32 Current = 0;
	int32 FrameIndex = 0;
	int32 DipNext = 0;
	int32 FramesShown = 0;
	int32 Shown = 0;
	TArray<int32> Order;
	float StepLeft = 0.f;
	float Waited = 0.f;
	float NextAt = 0.f;
	float StingAge = 0.f;
	bool bStingFading = false;
	float FlickerClock = 0.f;
	float Flicker = 1.f;
	float Gutter = 1.f;
	bool bHolding = false;
	bool bMenuSeen = false;
	bool bDone = false;
	bool bFlickering = false;
};
