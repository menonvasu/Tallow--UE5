#pragma once
#include "CoreMinimal.h"

/** Mim's rig (Tallow/SourceAssets/Creature/Final01), shared by the body component and the anim instance.
 *  Component space faces +Y, Z up, cm. Ten arms, two to a pair: arm01 at the chest (front support), arm02 beside the
 *  face (the gesture pair), arm03 on the body and arm04 on the belly (the reach arms, out to the sides), arm05 at
 *  the rump (rear support). */
namespace TallowMim
{
    constexpr int32 NumArms = 10;
    /** Arm k is arm(k/2+1), left for even k. */
    inline FName ArmBone(int32 Arm, const TCHAR* Part)
    {
        return FName(*FString::Printf(TEXT("arm%02d_%s_%s"), Arm / 2 + 1, Part, Arm % 2 == 0 ? TEXT("l") : TEXT("r")));
    }
    inline FName DigitBone(int32 Arm, int32 Digit, int32 Joint)
    {
        return FName(*FString::Printf(TEXT("arm%02d_d%d_%02d_%s"), Arm / 2 + 1, Digit, Joint, Arm % 2 == 0 ? TEXT("l") : TEXT("r")));
    }
    enum class ERole : uint8 { Support, Reach, Gesture };
    inline ERole Role(int32 Arm)
    {
        const int32 Pair = Arm / 2 + 1;
        return Pair == 2 ? ERole::Gesture : (Pair == 3 || Pair == 4) ? ERole::Reach : ERole::Support;
    }
    inline float Side(int32 Arm) { return Arm % 2 == 0 ? 1.f : -1.f; }
    static const TCHAR* const PlushBones[] = {TEXT("plush_hood"), TEXT("plush_neck"), TEXT("plush_chest"),
        TEXT("plush_body"), TEXT("plush_abd1"), TEXT("plush_abd2")};
    /** The spine bones the fit squeeze moves, parents before children. */
    static const TCHAR* const SpineBones[] = {TEXT("chest"), TEXT("neck"), TEXT("head"), TEXT("abdomen_01"), TEXT("abdomen_02")};

    /** The body as the fit layer sees it standing: from the rump to the front of the face, across the head, and from
     *  the underside to the top. Its middle sits 41 cm behind the root. */
    const FVector BodySize(300.f, 90.f, 90.f);
    constexpr float BodyUnderside = 95.f, BodyCenterBehindRoot = 41.f, BodyBoneHeight = 126.f;

    /** Per-bone collision capsules in the rest pose (export mim-capsules.json, converted to component space): the clip
     *  audit sweeps these against the world every frame. */
    struct FCapsule { const TCHAR* Bone; FVector Start, End; float Radius; };
    static const FCapsule Capsules[] = {
    {TEXT("arm01_upper_l"), FVector(30.0, -20.0, 133.0), FVector(112.1, 27.9, 115.0), 22.7},
    {TEXT("arm01_fore_l"), FVector(112.1, 27.9, 115.0), FVector(173.8, 107.7, 63.1), 9.2},
    {TEXT("arm01_hand_l"), FVector(173.8, 107.7, 63.1), FVector(182.6, 136.9, 36.7), 7.9},
    {TEXT("arm01_upper_r"), FVector(-30.0, -20.0, 133.0), FVector(-112.0, 28.0, 115.0), 22.8},
    {TEXT("arm01_fore_r"), FVector(-112.0, 28.0, 115.0), FVector(-173.6, 107.8, 63.1), 9.3},
    {TEXT("arm01_hand_r"), FVector(-173.6, 107.8, 63.1), FVector(-182.1, 138.1, 37.1), 7.9},
    {TEXT("arm02_upper_l"), FVector(26.0, -45.0, 150.0), FVector(78.4, 11.3, 228.3), 13.2},
    {TEXT("arm02_fore_l"), FVector(78.4, 11.3, 228.3), FVector(155.1, 72.8, 170.0), 10.7},
    {TEXT("arm02_hand_l"), FVector(155.1, 72.8, 170.0), FVector(172.2, 94.8, 153.7), 9.1},
    {TEXT("arm02_upper_r"), FVector(-26.0, -45.0, 150.0), FVector(-78.3, 11.4, 228.2), 13.2},
    {TEXT("arm02_fore_r"), FVector(-78.3, 11.4, 228.2), FVector(-155.1, 72.8, 170.0), 10.7},
    {TEXT("arm02_hand_r"), FVector(-155.1, 72.8, 170.0), FVector(-171.9, 94.9, 154.0), 9.2},
    {TEXT("arm03_upper_l"), FVector(29.0, -80.0, 133.0), FVector(78.1, -94.6, 171.8), 10.2},
    {TEXT("arm03_fore_l"), FVector(78.1, -94.6, 171.8), FVector(187.4, -64.7, 94.6), 13.4},
    {TEXT("arm03_hand_l"), FVector(187.4, -64.7, 94.6), FVector(196.3, -50.4, 73.2), 11.5},
    {TEXT("arm03_upper_r"), FVector(-29.0, -80.0, 133.0), FVector(-74.9, -97.3, 157.0), 10.5},
    {TEXT("arm03_fore_r"), FVector(-74.9, -97.3, 157.0), FVector(-185.1, -60.3, 93.6), 15.8},
    {TEXT("arm03_hand_r"), FVector(-185.1, -60.3, 93.6), FVector(-196.7, -45.6, 69.6), 13.5},
    {TEXT("arm04_upper_l"), FVector(27.0, -112.0, 140.0), FVector(89.3, -160.8, 183.4), 22.8},
    {TEXT("arm04_fore_l"), FVector(89.3, -160.8, 183.4), FVector(182.8, -155.7, 102.8), 22.6},
    {TEXT("arm04_hand_l"), FVector(182.8, -155.7, 102.8), FVector(195.8, -136.2, 79.8), 19.4},
    {TEXT("arm04_upper_r"), FVector(-27.0, -112.0, 140.0), FVector(-89.3, -160.8, 183.4), 22.9},
    {TEXT("arm04_fore_r"), FVector(-89.3, -160.8, 183.4), FVector(-182.8, -155.7, 102.8), 22.9},
    {TEXT("arm04_hand_r"), FVector(-182.8, -155.7, 102.8), FVector(-195.3, -136.5, 80.3), 19.6},
    {TEXT("arm05_upper_l"), FVector(27.0, -138.0, 120.0), FVector(96.4, -98.9, 98.3), 20.6},
    {TEXT("arm05_fore_l"), FVector(96.4, -98.9, 98.3), FVector(116.6, -178.6, 65.7), 8.8},
    {TEXT("arm05_hand_l"), FVector(116.6, -178.6, 65.7), FVector(119.9, -161.1, 45.0), 7.5},
    {TEXT("arm05_upper_r"), FVector(-27.0, -138.0, 120.0), FVector(-96.3, -99.1, 98.3), 20.5},
    {TEXT("arm05_fore_r"), FVector(-96.3, -99.1, 98.3), FVector(-116.6, -178.6, 65.7), 8.8},
    {TEXT("arm05_hand_r"), FVector(-116.6, -178.6, 65.7), FVector(-120.8, -160.3, 44.5), 7.5},
    {TEXT("head"), FVector(0.0, 15.0, 137.0), FVector(0.0, 65.0, 141.0), 42.0},
    {TEXT("chest"), FVector(0.0, -55.0, 127.0), FVector(0.0, -15.0, 130.0), 34.0},
    {TEXT("abdomen_01"), FVector(0.0, -88.0, 125.0), FVector(0.0, -122.0, 125.0), 30.0},
    {TEXT("abdomen_02"), FVector(0.0, -122.0, 125.0), FVector(0.0, -160.0, 128.0), 30.0},
    };
}
