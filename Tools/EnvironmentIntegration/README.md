# HouseIntegrationV01 — playable integration work in progress

Owned assets and scripts only. Original map and gameplay classes are untouched by these tools. Root executes Unreal scripts against the already-created `/Game/Presentation/Playable/Lvl_Tallow_Playable` copy.

## Unreal execution order

1. `import_house.py` — all architecture and three CC0 furniture meshes/materials.
2. `refresh_upper_floor_unreal.py` — targeted explicit-cell upper floor repair (already included in current full import, safe to repeat).
3. `import_finish_joinery.py` — targeted finish batch import, places only finish actors. Requires playable copy loaded.
4. `dress_playable_house.py` — replaces owned tagged dressing, disables exact source architecture/prototype actor collisions, adds room furnishings and readability fills. Does not create gameplay tasks. Gate leaves and splintered board are excluded from auto-placement.
5. `refresh_house_surfaces.py` — assigns private limewash/wallpaper to owned mesh slots. Run after any full import.
6. `import_task_props.py` — visual switchboard/latch/window only. Root assigns existing native components using `TaskProps/manifest.json` socket coordinates.

All assets live under `/Game/Presentation/Environment/HouseIntegrationV01`. The scripts assert playable map ownership before level edits. A failed import should be repaired before dressing, rather than spawning missing meshes. Full import is safe to repeat but slower than the targeted refreshes.

## Source and placement contracts

`Tallow/SourceAssets/Production/HouseIntegrationV01` contains:

- `HouseArchitecture.blend`, `Architecture/manifest.json`: own-UV architectural source and exports.
- `HouseStagedReview.blend`, `Review/*.png`: inspected source views; UE runtime lighting, character presence and visibility differ.
- `dressing.json`: curated furniture transforms, exact prototype actor replacement names, baseline light locations.
- `route-plan.json`: random item candidates, gate hinge transforms, fuse panel, final room/escape route, hiding and draft candidate contracts. Root owns eligibility, randomization, triggers and gameplay validation. Hiding candidates require actual capsule/line-of-sight checks before activation.
- `source-clearance.json`: 57 vertical tread-center rays have no obstacle within 2.4m; five sampled collection/upper landing positions hit floor at 3.9m. This is not swept-capsule or navigation proof.
- `BlenderKitRaw/attribution.json`: original CC0 Poly Haven furniture via BlenderKit, original hashes and source links. Licensed author UVs retained; all newly authored house/task geometry has ProductionUV.
- `TaskProps/manifest.json`: switchboard backplate-center pivot, local Unreal +Y front, three socket centers; source and standalone measured review.

## Build and review

Headless Blender only; never attach to the user's open scene. Geometry scripts use `--background --factory-startup --threads 4 --python-exit-code 1`. Expensive render commands additionally use `flock /tmp/tallow-heavy-render.lock`.

Source rebuild sequence: `audit_house_source.py`, `audit_wall_parts.py`, `build_house_kit.py`, `redesign_house.py`, `make_surface_maps.py`, `refresh_source_surfaces.py`, `repair_upper_floor.py`, `finish_joinery.py`. Then `measure_house_clearance.py` and `render_house_review.py`. The explicit upper-floor repair is essential: a Boolean on the disconnected imported source shell was rejected during visual review. Furniture preparation and task props are independent (`prepare_furniture.py`, `build_task_props.py`, `review_task_props.py`). `make_route_plan.py` is the initial proposal generator; the delivered route-plan JSON includes subsequent integration contracts, so do not overwrite it casually.

## Remaining gates

Root must validate map alignment, collision, complete randomized route, dynamic gates, actual hiding/drafts, player and 2.4m Keeper traversal, and lit/cupped/out candle readability in Unreal. Source renders are improved WIP, not final house art approval. Window exterior treatment, story dressing, final interaction placement and gameplay camera composition may need another pass after actual playthrough evidence.
