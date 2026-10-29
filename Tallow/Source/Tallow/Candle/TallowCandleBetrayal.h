#pragma once

#include "CoreMinimal.h"

/** Your candle betrays you (29 Oct). Lit, it lets her see and lets Mim see her. The closer Mim is, by the way she
 *  would have to come and not through the walls, the harder the flame gutters, dims and leans away from her; right on
 *  top of her, and coming, a gust can take it. Plain maths and data, so the rules can be tested without a world. */
namespace TallowCandleBetrayal
{
/** How far off Mim feels (cm). Her walk to Nell when the navmesh has one (never shorter than the straight line);
 *  otherwise the straight line stretched by the walls between them (Transmission: the fraction of sound that gets
 *  through, as the noise system measures it), so a wall still keeps the flame calmer than open air. */
inline float FeltDistance(float Straight, float PathLength, float Transmission)
{
	if (PathLength > 0.f) return FMath::Max(Straight, PathLength);
	return Straight / FMath::Clamp(Transmission, .2f, 1.f);
}

/** 0 calm, 1 fully guttering. Eased, so the first sign comes in softly and the last couple of metres climb hard. */
inline float GutterForDistance(float Felt, float StartCm, float FullCm)
{
	const float T = FMath::Clamp((StartCm - Felt) / FMath::Max(1.f, StartCm - FullCm), 0.f, 1.f);
	return T * T * (3.f - 2.f * T);
}
}

/** The gust that blows the candle out. One per encounter at most, a chance that rises the closer she comes (so not
 *  every time), only while she is coming, never into a cupped hand, never inside a protected moment or a scripted beat. */
struct FTallowCandleGust
{
	/** Inside this felt distance (cm) a gust can come. */
	float StartCm = 320.f;
	/** At or inside this the chance is at its height. */
	float SureCm = 110.f;
	/** The chance of a gust per second at its height (a hazard rate, so frame rate does not change the odds). */
	float MaxRatePerSecond = .9f;
	/** She has to be coming toward Nell at least this fast (cm/s); standing over her is not a gust. */
	float MinClosingSpeed = 25.f;
	/** The encounter starts once she is this close... */
	float EncounterCm = 700.f;
	/** ...and ends only once she has been this far off for EncounterEndSeconds. */
	float EncounterEndCm = 1100.f;
	float EncounterEndSeconds = 6.f;

	bool bInEncounter = false;
	/** This encounter has had its gust. */
	bool bSpent = false;
	float FarSeconds = 0.f;

	/** The chance of a gust in a step of Dt seconds, before the encounter and hold rules. */
	float Chance(float Felt, float Closing, float Dt) const
	{
		if (Closing < MinClosingSpeed || Felt >= StartCm || Dt <= 0.f) return 0.f;
		const float T = FMath::Clamp((StartCm - Felt) / FMath::Max(1.f, StartCm - SureCm), 0.f, 1.f);
		return 1.f - FMath::Exp(-MaxRatePerSecond * T * T * Dt);
	}

	/** Steps the encounter and returns true when a gust should take the flame now. bExposed: lit and open in her hand
	 *  (not cupped, not mid-strike, not out). bHeld: a protected moment, a scripted beat, a capture. Roll: 0 to 1. */
	bool Step(float Felt, float Closing, float Dt, bool bExposed, bool bHeld, float Roll)
	{
		if (Felt < EncounterCm) bInEncounter = true;
		FarSeconds = Felt > EncounterEndCm ? FarSeconds + FMath::Max(Dt, 0.f) : 0.f;
		if (bInEncounter && FarSeconds >= EncounterEndSeconds) { bInEncounter = false; bSpent = false; FarSeconds = 0.f; }
		if (!bInEncounter || bSpent || !bExposed || bHeld) return false;
		if (Roll >= Chance(Felt, Closing, Dt)) return false;
		bSpent = true;
		return true;
	}

	void Reset() { bInEncounter = bSpent = false; FarSeconds = 0.f; }
};
