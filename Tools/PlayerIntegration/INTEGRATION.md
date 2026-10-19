# Player interaction V05

Root integration owns all gameplay decisions. These assets and the presentation component do not decide chair escape, interaction success, candle state, damage, or timers.

## Import and validation

Run `import_interaction_unreal.py`, then `import_tools_unreal.py`, then `import_supplement_unreal.py` in the editor. Each refuses existing destination assets. Run `verify_import_fresh_unreal.py` in a fresh editor process to catch unit, animation, skeleton and material errors. Scripts are only for the `/Game/Presentation/PlayerInteractionV05`, `/Game/Presentation/CandleV02`, and `/Game/Presentation/ToolProps` namespaces. NormalGL textures have their green channel flipped for Unreal.

Generated full/partial meshes share the original 52 rest bones. Partial: 22,716 triangles; full: 57,368. Imported facing is -Y, lateral +X. Camera mesh placement is yaw +90, translation (20,0,-110) cm. Hands, candle, flame and held tools require the same first-person projection: 90 degrees, scale .6; world FOV remains 70. Owner-only visibility and expanded animated bounds remain enabled.

## Held calibration

Sample `AN_PlayerV05_Hold` at zero, then attach candle to `hand_r` with KeepWorld using camera-relative translation **(44.5,22.5,-27.5) cm**, yaw +90. Asset: `CandleV02/SM_CandleV02_Crooked`. Wick in candle asset local space: **(.4,0,25.5) cm**. Attach flame and light here; do not use the old candlestick bounds. The runtime `AttachHeldCandleToHand` helper records and restores the former parent.

Sample `AN_PlayerV05_ToolHold` at zero for left-tool calibration. All tools have local +Z length and their base at origin. KeepWorld attach to `hand_l`:

| Asset under ToolProps | Camera translation cm | Yaw |
|---|---|---|
| SM_ToolV02_Crowbar | (44.5,-22.5,-35) | +90 |
| SM_ToolV02_Fuse | (44.5,-22.5,-27) | +90 |

Exact bounds are in `SourceAssets/Player/InteractionV05/tool-match-contract.json`. For floor pickup placement, rotate pitch90 then compute the rotated bounds minimum Z and lift by its negative plus a small floor clearance. Crowbar has an asymmetric hook; a generic half-height offset is incorrect.

## Playback

`PlayPresentationClip` uses the shared native crossfade instance, with .18-second default transitions. One-shots pass loop=false; continuous poses/locomotion pass loop=true. `SetPresentationPose` is for calibration/review, not for per-frame playback. Keep all assets strongly referenced by the integration actor/settings so the cooker includes them.

Base loops: Hold/Cup/Rest/Crouch/ToolHold 2 seconds; Walk 1 second; Sprint .6 seconds; IntroBound 4 seconds. Loop endpoints duplicate exactly. Nonloops: Reach1, Relight1.2, Pickup1.5, ChairEscape1.5 seconds. Prefer supplemental **CupShield** to Cup: its hand is raised above the redesigned wick. Supplemental CrowbarPry2 and FuseInsert1.5 seconds preserve closed tool grip. Interaction timing remains gameplay-owned; adjust playback rate to the real interaction duration.

Walk/Sprint are first-person upper-body sway, not full-body leg locomotion. Do not expose a sliding full model or mismatched Manny shadow. Existing SeatedV04 is the validated menu-chair fit; IntroBound is an opening presentation pose, not a replacement chair gameplay animation.

## Evidence and limits

`contact-uv-validation.json`: zero overlapping candle UV triangle interiors at 2048 resolution; 0.5 mm minimum sampled right stem clearance; zero sampled vertices penetrating the stem by more than 1 mm across Hold/Cup/Walk/Relight/Pickup. `animation-validation.json`: normalized weights, maximum2 influences, 52 bones and exact loop closure. Numerical contact checks cover the stem, not every possible pose/prop triangle pair. FP renders are Blender previews; Unreal import, transition, camera, lighting and gameplay validation are required separately.

## Relight with match

Run `import_relight_unreal.py` for the final additional `AN_PlayerV05_RelightMatch` (1.2 seconds). Prefer it to the original Relight. It brings the candle closer and pinches the original match model between index and thumb. Sample at **19/30 seconds** for calibration, set `SM_ToolV02_Match` from the camera-relative location and XYZW quaternion in `tool-match-contract.json`, then KeepWorld attach to `hand_l`. Restore the desired live pose immediately after calibration. Keep the candle's original Hold calibration throughout. Show the match only during the relight presentation; interruption must hide it. This visual does not ignite the candle or alter the relight timer.

Final match bone-tip gap: 3.03 mm around a 2.4 mm shaft. Head-to-wick gap: 2.41 mm at peak. This is a static peak contact check, not a claim of continuous triangle collision validation. Complete editable scene: `05_interaction-relight.blend`; the original production scene remains available without supplemental actions.
