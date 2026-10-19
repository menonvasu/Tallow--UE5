#pragma once

#include "CoreMinimal.h"

/** Deterministic presentation only. No candle timers, matches or light state are modified. */
namespace TallowFlameVisual
{
struct FShape
{
	float Height = 1.f;
	float Width = 1.f;
	float BendX = 0.f;
	float BendY = 0.f;
	float Emission = 1.f;
};

inline FShape Evaluate(float Time, float Exposure, bool bCupped, float Ignition)
{
	const float Gust = FMath::Clamp(Exposure, 0.f, 1.f) * (bCupped ? .12f : 1.f);
	const float Flutter = FMath::Sin(Time * 23.f) * FMath::Sin(Time * 11.7f + 1.2f);
	const float Collapse = FMath::Pow(FMath::Max(0.f, FMath::Sin(Time * 13.f)), 6.f) * Gust;
	FShape Shape;
	Shape.Height = FMath::Clamp((bCupped ? .82f : 1.f) + .045f * FMath::Sin(Time * 6.1f)
		- .38f * Gust - .36f * Collapse + .28f * FMath::Clamp(Ignition, 0.f, 1.f), .18f, 1.4f);
	Shape.Width = 1.f + .14f * Gust + .18f * Collapse;
	Shape.BendX = .035f * FMath::Sin(Time * 7.3f) + Gust * (.52f + .22f * Flutter);
	Shape.BendY = .025f * FMath::Sin(Time * 8.7f + 2.f) + Gust * .17f * FMath::Sin(Time * 17.f);
	Shape.Emission = FMath::Max(.25f, 1.f - .4f * Collapse + .5f * FMath::Clamp(Ignition, 0.f, 1.f));
	return Shape;
}

/** Mirrors the material's WPO contract. Unit mesh: tip Z=1, root (0,0,0), max radius=.5. */
inline FVector Deform(const FVector& Position, float HeightCm, float WidthCm, const FShape& Shape)
{
	const float T = FMath::Clamp(static_cast<float>(Position.Z), 0.f, 1.f);
	return FVector(Position.X * WidthCm * Shape.Width + Shape.BendX * HeightCm * T * T,
		Position.Y * WidthCm * Shape.Width + Shape.BendY * HeightCm * T * T,
		T * HeightCm * Shape.Height);
}
}
