# M3 house greybox

The editor and game now open `Content/Maps/Lvl_Tallow_M3_Greybox` in Unreal 5.8. This branch builds on MR !11 (`refine/ai`). The original FirstPerson test map remains available.

The map assembles the existing house, adds furniture and climb-test ledges, places the existing chair/candle/AI blueprints, and attaches a 230 cm rough Keeper visual to the AI actor. The Keeper is a static prototype visual, not the final skeletal animation integration. House collision uses dedicated `Environment/M3House` copies with complex-as-simple collision; the earlier imports had no simple collision.

Start in the basement chair room. The main route crosses the basement workshop/toy corridor and laundry, rises via the basement stairs, then uses the ground-floor hall and main/back stairs to reach the upper floor. The furniture adds sightline breaks and crawl-under spaces. Three child-gap navigation volumes cover the existing narrow wall openings.

## Climbing handoff

`CLIMB_Workshop_Ledge`, `CLIMB_Study_Ledge` and `CLIMB_ToyRoom_Ledge` have tops at 55, 70 and 95 cm above their room floors. Their approach and landing coordinates are recorded in `m3-layout.json` (Unreal centimetres). The `TEST_STEP_*` blocks allow early traversal testing; hide/remove these once the climb implementation works. Climbing does not gate the main route. The climb mechanic itself is not implemented by this change.

`TODO_Task_*` are editor-only location markers for the gameplay owner, not implemented tasks.

## Rebuilding and checking

`build_m3_greybox.py` creates/replaces only this generated map. Do not run it after hand-editing the map without saving a separate copy. Enable PythonScriptPlugin and EditorScriptingUtilities for the script invocation.

Use UnrealEditor-Cmd with `-run=pythonscript -script=<absolute path to build_m3_greybox.py> -nullrhi -unattended` to rebuild. The Keeper FBX is included in SourceAssets; its source .blend remains in the separate Keeper art project.

Use normal UnrealEditor with `-ExecCmds="py <absolute path to validate_m3_greybox.py>" -nullrhi -RenderOffscreen -unattended` to run the Play in Editor checks. Do not use `-ExecutePythonScript` for this test: it exits before the deferred PIE checks finish. The script waits for asset compilation, builds navigation, saves, starts PIE, writes `m3-validation.json`, then exits.

The Blender layout previews are source-space previews, not Unreal screenshots. Lighting, final animated movement and a human playthrough still need review in the rendering editor.

## Wall, ceiling and squeeze handoff

`add_traversal_routes.py` adds eight `HANDOFF_Surface_*` anchors through the basement toy corridor and three paired `HANDOFF_Squeeze_*` door approaches. The details are in `traversal-routes.json`. Contact anchors sit on the surface; the movement implementation must offset the actor centre by its collision shape along the supplied outward normal. The thin connection guides are editor-only and have no collision.

These anchors do **not** make ground NavMesh/StateTree MoveTo climb walls. The gameplay owner needs to implement surface movement, corner transitions, capsule sweeps, interruption/reset and the folded doorway pose. The standing visual stays 230 cm tall. Proposed squeeze visual limits are 90 cm wide and 215 cm tall for the 110 × 240 cm openings. Extended legs still need individual planting/clearance checks.

After a fresh map build, run `refresh_house_collision.py` in normal UnrealEditor via `-ExecCmds="py <script>"` with the same non-rendering flags. This regenerates the collision-derived navigation cache on the copied house assets, then runs validation. Merely changing the collision mode did not refresh that imported cache.

Navigation uses a 1,024-tile pool because the house has several vertically stacked surfaces. The initial small pool silently left holes in otherwise generated routes. Validation also checks the stair connections, capsule sweeps through the three marked doors, and surface-anchor contact traces.

The basement stairs now have two 19.5 cm side approach steps. The navigation mesh uses 10 cm horizontal cells, 5 cm vertical cells and a 45 cm step limit; this avoids losing the narrow door connections and stair entrance to coarse rasterization. `validate_stair_movement.py` checks the existing Keeper capsule walking up the basement and main stairs in PIE, with decision logic paused only for the test.

Rebuild order: `build_m3_greybox.py`, `add_traversal_routes.py`, then `refresh_house_collision.py` (which also configures navigation and validates the map). The route script does not quit the editor; for unattended commandlet use, run it with `-run=pythonscript -script=...`.

## Submission boundary

The repository README lists M3 on 28 September: three AI states, complete noise, at least three tasks, hiding, stillness, phase-one creature and reflection. This branch supplies the greybox/rough creature and traversal handoff. Puzzle tasks and the reflection remain with their owners; location markers do not satisfy those requirements. The official submission time and rubric have not been verified. MR !11 is included as the base of this branch, not merged into main by this work.

Latest validation: nine floor traces, five patrol links, both floor connections, three doorway capsule sweeps and eight surface contacts passed. The existing Keeper physically reached the basement and upper stair destinations in PIE (45 cm step height, normal collision enabled). `m3-stair-movement.json` records those movement checks.
