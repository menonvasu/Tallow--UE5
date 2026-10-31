#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TallowHunterDirector.generated.h"

class ATallowCreature;
class ATallowCharacter;

/** The shape of an evening in the house (25 Oct: "I already know he's gonna come to the door every time"):
 *  long quiet stretches where it is far off, a build where it drifts closer and is glimpsed passing, a peak of
 *  real encounters, then a breather after a chase or a near miss before it starts again. */
UENUM(BlueprintType)
enum class ETallowTensionPhase : uint8
{
    Quiet, Build, Peak, Relax
};

/** Where the player is on the route through the house. The arc, not the clock, decides how hard the
 *  evening is allowed to press: each act raises the aggression ceiling, shortens the quiet, and unlocks
 *  a new kind of scare, so minute five and minute fifty do not feel the same (pacing pass, 27 Oct). */
UENUM(BlueprintType)
enum class ETallowStoryAct : uint8
{
    Dormant,      // chair and candle: Mim is not awake yet, the house only breathes
    FirstContact, // first board pried loose: teach the glimpse, then the first real hunt
    FuseHunt,     // boards cleared: the jump-out joins the repertoire
    HouseWakes,   // power restored: the heap disguise appears for the first time
    Reel,         // film loaded: a protected screening, then the surge it earns
    EndRun        // code verified: no more tricks or red herrings, only the honest run to the latch
};

/** Tuning for one act (one row of the evening). Plain data, so automation tests can check the escalation shape
 *  without a world, and so a chapter can hand the director its own row later (SetActTuning). */
USTRUCT(BlueprintType)
struct FTallowArcTuning
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float QuietMin = 30.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float QuietMax = 50.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float BuildSeconds = 90.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float PeakSeconds = 75.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float RelaxMin = 18.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float RelaxMax = 28.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float AggressionFloor = .15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float AggressionCap = .6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float AggressionRate = .001f;
    /** Closeness alone cannot end the dread early; being seen can. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float BuildDwellBeforePeak = 20.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float ScareCooldownMin = 120.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float ScareCooldownMax = 180.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float DisguiseCooldownMin = 150.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float DisguiseCooldownMax = 240.f;
    /** On rising into the act, the heap is held back this long first (0: whenever its cooldown allows). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float DisguiseDebutSeconds = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float PassByMin = 50.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float PassByMax = 90.f;
    /** The stalk: how soon it may stand off and watch again. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float WatchMin = 22.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float WatchMax = 34.f;
    /** And how many times in one build. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") int32 WatchesPerBuild = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") float LieGapScale = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") bool bAllowScare = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") bool bAllowDisguise = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") bool bAllowHouseEvents = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") bool bAllowLies = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") bool bAllowPassBy = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") bool bAllowWatch = true;
    /** The close call: it may be sent to search the room she keeps still in (once a hunt, never two hunts running). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pacing") bool bAllowCloseCall = false;
};

/** Chooses areas and encounter pacing. It never supplies a target actor to the hunter. */
UCLASS()
class TALLOW_API UTallowHunterDirector : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void UpdatePressure(float DeltaTime, ATallowCreature* Keeper, ATallowCharacter* Player, bool bSeen);
    bool TakeAreaHint(ATallowCreature* Keeper, ATallowCharacter* Player, FVector& OutArea, bool& bOutRetreat);
    void ResetPressure();
    UFUNCTION(BlueprintPure, Category="Hunter") float GetWalkingDistance() const { return WalkingDistance; }
    UFUNCTION(BlueprintPure, Category="Hunter") int32 GetAreaHintCount() const { return HintNumber; }
    bool IsReturnOverdue() const;
    /** The overseer never moves the body. It watches the player and decides what the hunter
     *  is allowed to be: faster, sharper-eared, quicker to come back. The player is never told. */
    void NotePursuitLost();
    void NoteCaught();
    /** It came close and went away without finding her: a near miss ends the peak. */
    void NoteNearMiss();
    UFUNCTION(BlueprintPure, Category="Hunter") ETallowTensionPhase GetPhase() const { return Phase; }
    /** Seconds into the current phase. */
    float GetPhaseAge() const;
    /** Now and then while it is building and unaware of her: walk a route that crosses her view at a distance,
     *  so she glimpses it go by ("when it's dark you should sometimes be able to see it come past"). */
    bool WantsPassBy() const;
    /** The jump-out is rare: only while building or peaking, minutes apart. */
    bool CanScare() const;
    void NoteScare();
    /** The disguise (the still, harmless-looking heap) is rarer again: building or peaking, minutes apart, and never
     *  hard on the heels of another scare. */
    bool CanDisguise() const;
    void NoteDisguise();
    bool TakePassByHint(ATallowCreature* Keeper, ATallowCharacter* Player, FVector& OutPoint);
    /** The stalk: while the evening builds it may stand off in the dark and watch her, a few times a build,
     *  each time a little closer. Never inside a protected moment, never once the peak has begun. */
    bool WantsWatch() const;
    /** How close the next watch may stand (0 far, 2 close), from the watches already held this build. */
    int32 GetWatchStage() const { return FMath::Min(WatchesThisBuild, 2); }
    /** A sound answered by watching: allowed inside protection, still bounded by the build's watches. */
    bool CanWatchFromNoise() const;
    void NoteWatch(bool bSlipped);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") int32 Watches = 0;
    void NoteHiding();
    UFUNCTION(BlueprintPure, Category="Hunter") float GetAggression() const { return Aggression; }
    /** The close call (29 Oct): she keeps dark (candle out or shielded), still and holding her breath, and it is
     *  sent to search the room she is in, for real. The director chooses only the room, the way it chooses areas; it
     *  never says where in the room she is, and the hunter's own senses decide what it finds. At most once a hunt, from
     *  the fuse hunt (chapter two) on, never in two hunts running, never inside a protected moment, never at the peak. */
    bool WantsCloseCall(const ATallowCharacter* Player) const;
    bool TakeCloseCallRoom(const ATallowCharacter* Player, FName& OutRoom) const;
    void NoteCloseCall();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") int32 CloseCalls = 0;
    /** Each rise out of the dormant house (a chapter's hunt, or its retry after a capture) is a new hunt. */
    UFUNCTION(BlueprintPure, Category="Hunter|Pacing") int32 GetHuntNumber() const { return HuntNumber; }
    /** Reviews only (the close-call check): forget the last close call, so the next hunt may have one. */
    UFUNCTION(BlueprintCallable, Category="Hunter|Review") void ResetCloseCallsForReview() { LastCloseCallHunt = -10; }

    // --- The evening's arc (pacing pass) -------------------------------------------------------------
    static FTallowArcTuning TuningForAct(ETallowStoryAct InAct);
    /** The current act's row: a chapter's own row if one was handed over, else the built-in table. */
    FTallowArcTuning Tuning() const { const FTallowArcTuning* Row = ActTuning.Find(Act); return Row ? *Row : TuningForAct(Act); }
    /** Data-driven rows (pass 2 chapters key the evening by act): replace one act's row, or clear it to fall back. */
    UFUNCTION(BlueprintCallable, Category="Hunter|Pacing") void SetActTuning(ETallowStoryAct ForAct, const FTallowArcTuning& Row) { ActTuning.Add(ForAct, Row); }
    UFUNCTION(BlueprintCallable, Category="Hunter|Pacing") void ClearActTuning(ETallowStoryAct ForAct) { ActTuning.Remove(ForAct); }
    UFUNCTION(BlueprintPure, Category="Hunter|Pacing") FTallowArcTuning GetActTuning(ETallowStoryAct ForAct) const { const FTallowArcTuning* Row = ActTuning.Find(ForAct); return Row ? *Row : TuningForAct(ForAct); }
    UFUNCTION(BlueprintPure, Category="Hunter|Pacing") ETallowStoryAct GetAct() const { return Act; }
    /** The play director derives the act from real route progress. Rising into a new act fires its
     *  authored beat; falling back (a capture reset the house) is silent. */
    void SetStoryAct(ETallowStoryAct NewAct);
    /** A solved step the player should get to feel: pressure lets go for a breath (first fuse, etc.). */
    void NotePuzzlePayoff();
    /** The film has been watched end to end. The projector was loud; the surge now is honest. */
    void NoteProjectionComplete();
    /** Space to think: reading, threading, watching the film. Closeness cannot start a peak and no
     *  ambient scare may fire, but walking into Mim is still walking into Mim. */
    void BeginProtectedWindow(float Seconds);
    bool IsProtected() const;
    /** Minor house scares are build texture only: never over a chase, never inside earned relief.
     *  A major event (the telephone) also waits until the house has woken. */
    bool AllowsHouseEvent(bool bMajorEvent) const;
    /** Red herrings and whispers belong to dread, not to relief, dormancy or the final run. */
    bool AllowsAmbientLie() const;
    float GetLieGapScale() const { return Tuning().LieGapScale; }

    /** Reviews only (the Keeper Lab, chase pace probes): pin how hunted the house is. */
    UFUNCTION(BlueprintCallable, Category="Hunter|Review") void SetAggressionForReview(float Value) { Aggression = FMath::Clamp(Value, 0.f, 1.f); bReviewPinned = true; }
    UFUNCTION(BlueprintCallable, Category="Hunter|Review") void SetPhaseForReview(ETallowTensionPhase NewPhase) { SetPhase(NewPhase, GetWorld()->GetTimeSeconds()); }
    /** Reviews only (the long soak): rise into an act now, with its authored beat, and hold it there so route
     *  progress cannot move it. Dormant releases the pin. */
    UFUNCTION(BlueprintCallable, Category="Hunter|Review") void SetActForReview(ETallowStoryAct NewAct);
    float GetSpeedMultiplier() const { return 1.f + .55f * Aggression; }
    float GetHearingMultiplier() const { return 1.f + .9f * Aggression; }
    float GetSightMultiplier() const { return 1.f + .35f * Aggression; }
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") float Aggression = .15f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") int32 Escapes = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") int32 Hides = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") float Menace = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") bool bBreather = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter") float MaxAbsenceSeconds = 20.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") ETallowTensionPhase Phase = ETallowTensionPhase::Quiet;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Pacing") ETallowStoryAct Act = ETallowStoryAct::Dormant;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") int32 PassBys = 0;
private:
    void SetPhase(ETallowTensionPhase NewPhase, float Now);
    float ClampAggression(float Value) const;
    /** The guard (31 Oct): now and then it walks through the room that holds what she needs next, then moves on. */
    bool TakeGuardHint(ATallowCreature* Keeper, ATallowCharacter* Player, FVector& OutArea);
    float NextGuardAt = 0.f;
    float PhaseStartedAt = 0.f;
    float PhaseUntil = 0.f;
    float PendingRelaxAt = -1.f;
    float CloseSince = -1.f;
    bool bWasClose = false;
    float NextPassBy = 0.f;
    float NextWatch = 0.f;
    int32 WatchesThisBuild = 0;
    float LastSeenBy = -1000.f;
    float NextScare = 90.f;
    float NextDisguise = 100.f;
    float NextHint = 0.f;
    float BreatherUntil = 0.f;
    float LastNearby = 0.f;
    float NextDistanceQuery = 0.f;
    float WalkingDistance = 100000.f;
    int32 HintNumber = 0;
    int32 HuntNumber = 0, LastCloseCallHunt = -10;
    float ProtectedUntil = 0.f;
    bool bReviewPinned = false, bActPinned = false;
    UPROPERTY() TMap<ETallowStoryAct, FTallowArcTuning> ActTuning;
};
