# TALLOW integrated playtest

The art branch now has a playable house at `/Game/Presentation/Playable/Lvl_Tallow_Playable`, reached from `/Game/Presentation/Cinematics/Maps/Lvl_MenuPresentation`. The original shared greybox map remains available. This is a local editor playtest; final audio, the pending lockpick MR and Windows package acceptance remain outstanding.

## Route and ownership

Chair escape → candle/cupping lesson → crowbar → three boards → three fuses (one per floor) → switchboard → attic collection → release latch → ending/replay. Crowbar and fuse positions roll within measured candidate pools at the start of a new run. Capture retains that roll. Power turns on two weak practical lights and releases all three collection entrances, including its crawl shortcut. The candle remains useful.

`ATallowPlayDirector` connects presentation to the existing chair, candle, plank, inventory, fuse, AI and capture callbacks. Existing actors still own those mechanics. Vasu's lockpick has not been replaced or fabricated. The optional V07 animation path leaves legacy playback available. The only candle-rule correction cancels an outstanding relight on reset, preventing an invisible candle from relighting after capture.

## Current assets

| Asset | Unreal destination | Editable source |
| --- | --- | --- |
| Keeper V07, shared rig, 20 authored clips, centre-opening hood/mask and mouth | `/Game/Presentation/KeeperRiggedV07` | `Tallow/SourceAssets/Keeper/RiggedV07/keeper-v07-animated.blend` |
| Player V05 partial/full meshes and 16 presentation clips | `/Game/Presentation/PlayerInteractionV05` | `Tallow/SourceAssets/Player/InteractionV05/05_interaction-relight.blend` |
| Crooked candle and brass holder | `/Game/Presentation/CandleV02` | `Tallow/SourceAssets/Production/CandleV02/CandleV02_Production.blend` |
| Crowbar, fuse and match | `/Game/Presentation/ToolProps` | `Tallow/SourceAssets/Production/ToolProps/ToolProps_Production.blend` |
| Rebuilt house, stairs, joinery, surface materials, furnishings and collection | `/Game/Presentation/Environment/HouseIntegrationV01` | `Tallow/SourceAssets/Production/HouseIntegrationV01` |
| Main menu, eight-second intro and ten-second outro | `/Game/Presentation/Cinematics` | `Tools/Presentation` and `Tools/PlayIntegration/update_cinematic_props.py` |

The new character/prop production meshes have authored UVs. Furniture from BlenderKit/Poly Haven is CC0 and retains its licensed author UVs; source links and hashes are in `HouseIntegrationV01/BlenderKitRaw/attribution.json`.

The first-person hand, candle, tool and match contacts use their measured bone attachments. The original template body and candle are hidden in this map. Walk/sprint clips supply first-person arm motion; they are not full-body leg animation. The menu retains the inspected seated V04 body clip. The Keeper uses authored in-place motion with speed matching and pose blends; character movement and existing AI still own translation and decisions.

The wick carries the gameplay light. Nearby visibility is maintained while guttering, and cupping reduces intensity and the existing gameplay radius. The expressive flame deforms separately. The capture camera follows the moving head; the original game mode owns locking, fade and reset timing.

## Reproducing and reviewing

Use `Tools/PlayIntegration/configure_playable.py` after production imports. It recreates only tagged integration additions and refuses missing final assets. Fresh-only import scripts should not be rerun over existing namespaces. Environment import/dressing order is recorded in `Tools/EnvironmentIntegration/README.md`.

Run the finite Unreal Python checks through `-ExecCmds="py /absolute/script.py"` so their tick callbacks can finish:

- `test_flow_runtime.py`: menu/intro, actual chair input, candle/draft/cup/relight, inventory, progression callbacks, capture/reset, ending/replay and relight-reset regression.
- `test_route_runtime.py`: actual CharacterMovement stair runs for both capsule sizes, gate sweeps, hiding exits and navigation queries. Puzzle completion callbacks supply setup; these do not replace manual timed-interaction playtesting.
- `review_playable.py`: gameplay-camera images, hand/prop contacts, lit/cupped/out comparisons, new Keeper capture and actual menu/intro/outro.

Latest results are in `Tallow/Saved/Integration`. `delivery-status.json` records the reviewed outcome and remaining limits. Source-only rig/UV/contact validation is separate from runtime evidence.

Open `/home/tyron/Projects/TALLOW-Review` in the file manager. `START_HERE.html` is the visual gallery; folders21–26 contain the current integrated work. The earlier numbered folders retain previous sources and previews.

## Final passes

Final audio follows settled action timing. `Record Sounds.desktop` in the review folder opens the local recorder; recording begins only when the user presses Record. Takes and notes go under `Tallow/SourceAssets/Audio/RecordedTakes`. Actual microphone recording/playback still needs a user check.

Remaining acceptance includes Vasu's merged lockpick, fresh-player tutorial/pursuit balancing, the final audio pass and a Windows packaged run. Fine sleeve deformation, close-up mouth shading and exterior/story dressing remain art-polish items; do not label this a finished release.
