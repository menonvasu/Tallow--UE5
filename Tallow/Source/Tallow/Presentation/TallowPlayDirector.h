#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Candle/TallowCandleComponent.h"
#include "UI/Presentation/TallowMenuWidget.h"
#include "TallowPlayDirector.generated.h"

class ATallowCharacter;
class ATallowChairEscape;
class ATallowCreature;
class ATallowFuseBox;
class UTallowLessonWidget;
class UTallowRunWidget;
class UTallowPlayerPresentationComponent;
class UTallowFlameVisualComponent;
class ULevelSequence;
class ULevelSequencePlayer;
class ALevelSequenceActor;
class UAnimSequence;
class USkeletalMesh;
class UStaticMesh;
class UMaterialInterface;
class UStaticMeshComponent;
class USceneComponent;
class USoundBase;
class USoundAttenuation;
class UAudioComponent;
class UPointLightComponent;

UENUM(BlueprintType)
enum class ETallowPlayScene : uint8 { Game, MainMenu, Ending };

/** Integration owner for the art branch. Existing actors still own every minigame, candle rule and AI decision. */
UCLASS()
class TALLOW_API ATallowPlayDirector : public AActor
{
	GENERATED_BODY()
public:
	ATallowPlayDirector();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(EditAnywhere, Category="Flow") ETallowPlayScene Scene = ETallowPlayScene::Game;
	UPROPERTY(EditAnywhere, Category="Flow") TSoftObjectPtr<UWorld> GameMap;
	UPROPERTY(EditAnywhere, Category="Flow") TSoftObjectPtr<UWorld> MenuMap;
	UPROPERTY(EditAnywhere, Category="Flow") TSoftObjectPtr<UWorld> EndingMap;
	UPROPERTY(EditAnywhere, Category="Flow") TObjectPtr<ULevelSequence> MenuSequence;
	UPROPERTY(EditAnywhere, Category="Flow") TObjectPtr<ULevelSequence> IntroSequence;
	UPROPERTY(EditAnywhere, Category="Flow") TObjectPtr<ULevelSequence> OutroSequence;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<USkeletalMesh> PlayerHands;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UAnimSequence> HoldClip;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UAnimSequence> WalkClip;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UAnimSequence> SprintClip;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UAnimSequence> CrouchClip;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UAnimSequence> ToolHoldClip;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UAnimSequence> CrowbarPryClip;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UAnimSequence> FuseInsertClip;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UAnimSequence> CupClip;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UAnimSequence> RestClip;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UAnimSequence> RelightClip;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UAnimSequence> ReachClip;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UAnimSequence> PickupClip;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UAnimSequence> BoundClip;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UAnimSequence> EscapeClip;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UStaticMesh> CandleMesh;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UStaticMesh> FlameMesh;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UMaterialInterface> FlameMaterial;
	UPROPERTY(EditAnywhere, Category="Player") FVector WickPosition = FVector(.4f, 0.f, 23.f);
	UPROPERTY(EditAnywhere, Category="Player") FTransform CandleCameraTransform = FTransform(FVector(44.5f, 26.5f, -27.5f));
	UPROPERTY(EditAnywhere, Category="Player") FName CandleHandBone = TEXT("hand_r");
	UPROPERTY(EditAnywhere, Category="Player") float CandleIntensityLumens = 90.f;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UStaticMesh> CrowbarMesh;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UStaticMesh> FuseMesh;
	UPROPERTY(EditAnywhere, Category="Player") TObjectPtr<UStaticMesh> MatchMesh;
	UPROPERTY(EditAnywhere, Category="Player") FTransform MatchCameraTransform;
	UPROPERTY(EditAnywhere, Category="Player") float MatchCalibrationTime = 19.f / 30.f;
	UPROPERTY(EditAnywhere, Category="Player") FTransform CrowbarCameraTransform = FTransform(FRotator(0,90,0),FVector(44.5f,-22.5f,-35.f));
	UPROPERTY(EditAnywhere, Category="Player") FTransform FuseCameraTransform = FTransform(FRotator(0,90,0),FVector(44.5f,-22.5f,-27.f));
	/** Zero selects a fresh seed per new run. Capture keeps the same placed pickups. */
	UPROPERTY(EditAnywhere, Category="Route") int32 RunSeed = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Route") int32 ActiveRunSeed = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Route") bool bPowerRestored = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Route") bool bBoardsCleared = false;
	UFUNCTION(BlueprintPure, Category="Flow") bool CanEscape() const { return bPowerRestored && bBoardsCleared && !bLeaving && !bCaptureTracking; }
	UFUNCTION(BlueprintCallable, Category="Flow") void FinishRun();
	UFUNCTION(BlueprintPure, Category="Flow") FText GetObjective() const;
	UFUNCTION(BlueprintPure, Category="Flow") bool IsMenuOpen() const { return Menu && Menu->IsInViewport(); }
	UFUNCTION(BlueprintCallable, Category="Flow") void TogglePause();
	UFUNCTION(BlueprintCallable, Category="Flow") void SkipIntro();
	UFUNCTION() void OnMenuAction(ETallowMenuAction Action);
private:
	void InitializeScene();
	void InitializeGameplay();
	void BindGameplayEvents();
	void SetMenu(ETallowMenuMode Mode);
	void CloseMenu();
	void LoadDestination(const TSoftObjectPtr<UWorld>& Destination);
	bool PlaySequence(ULevelSequence* Sequence, bool bLoop);
	void RandomizePickups();
	void SetKeeperAwake(bool bAwake);
	void SetPowerVisuals(bool bEnabled);
	void UpdateLessons(float DeltaSeconds);
	void UpdatePlayerAnimation();
	void ApplyPickupVisual(AActor* Actor);
	void OnActorSpawned(AActor* Actor);
	UFUNCTION() void OnSequenceFinished();
	UFUNCTION() void OnChairEscaped();
	UFUNCTION() void OnCandleAcquired();
	UFUNCTION() void OnCandleReset();
	UFUNCTION() void OnCandleStateChanged(ECandleState State);
	UFUNCTION() void OnRelight();
	UFUNCTION() void OnPower();
	UFUNCTION() void OnPlankRemoved(bool bBroken);
	UFUNCTION() void OnStill(bool bStill);
	UFUNCTION() void OnCaught();
	UFUNCTION() void OnFuseInserted();
	// Interim CC0 cues (SourceAssets/Audio/CREDITS.md) until Ty's own sound design pass replaces them.
	USoundBase* Cue(const FString& Name);
	void PlayCue(const FString& Name, float Volume, const FVector* Where = nullptr);
	void AddFootsteps();
	void UpdateSound();
	UPROPERTY(Transient) TMap<FString, TObjectPtr<USoundBase>> Cues;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> CueAttenuation;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> PowerHum;
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Hinges;
	TArray<float> HingeYaw;
	TArray<bool> HingeMoving;
	ECandleState LastCandleState = ECandleState::Out;
	uint8 LastItem = 0;
	float NextSettle = 0.f;
	FTimerHandle CatchHandle;
	FTimerHandle BoardDropHandle;
	// Jumpscare staging on top of the game mode's capture (look lock, fade and reset stay owned there).
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> CaptureLight;
	float CaptureTime = 0.f;
	float CaptureBaseFOV = 0.f;
	float CaptureStrike = 0.f;
	FTimerHandle StingHandle;
	UPROPERTY(Transient) TObjectPtr<APlayerController> PC;
	UPROPERTY(Transient) TObjectPtr<ATallowCharacter> Player;
	UPROPERTY(Transient) TObjectPtr<ATallowCreature> Keeper;
	UPROPERTY(Transient) TObjectPtr<ATallowFuseBox> FuseBox;
	UPROPERTY(Transient) TObjectPtr<UTallowMenuWidget> Menu;
	UPROPERTY(Transient) TObjectPtr<UTallowLessonWidget> Lessons;
	UPROPERTY(Transient) TObjectPtr<UTallowRunWidget> RunUI;
	UPROPERTY(Transient) TObjectPtr<UTallowPlayerPresentationComponent> Presentation;
	UPROPERTY(Transient) TObjectPtr<UTallowFlameVisualComponent> Flame;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> HeldCandle;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> HeldTool;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> HeldMatch;
	UPROPERTY(Transient) TObjectPtr<UTallowFlameVisualComponent> MatchFlame;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> OriginalHeldTool;
	FTransform CrowbarHandTransform;
	FTransform FuseHandTransform;
	FDelegateHandle ActorSpawnedHandle;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Wick;
	UPROPERTY(Transient) TObjectPtr<ULevelSequencePlayer> SequencePlayer;
	UPROPERTY(Transient) TObjectPtr<ALevelSequenceActor> SequenceActor;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> ForcedClip;
	float ForcedUntil = 0.f;
	float DraftTime = 0.f;
	float CupTime = 0.f;
	float StillTime = 0.f;
	int32 TotalPlanks = 0;
	int32 RemovedPlanks = 0;
	bool bIntroPlaying = false;
	bool bLeaving = false;
	bool bInitialized = false;
	bool bKeeperAwake = false;
	bool bWasTied = false;
	bool bCaptureTracking = false;
	FTimerHandle InitializeHandle;
};
