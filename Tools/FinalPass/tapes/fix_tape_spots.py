"""Every tape in the house, measured and made to rest on a flat top: the chapter tape machines (ATallowStoryObject,
the spots each chapter can pick from DA_TallowCampaign) and the small lore tapes (ATallowSoundBite, placed where the
papers were). Re-runnable. PIE for every measurement (full collision and the runtime's own hidden state, like the
pickup audit), the editor world for the moves. Saves Lvl_Tallow_Playable only when a tape moved.

    UnrealEditor <project>/Tallow/Tallow.uproject -unattended -nosound -nullrhi -ExecCmds="py /abs/path/fix_tape_spots.py"
    TALLOW_TAPES_MODE=audit  measures only, never saves (the replay check)

1. PIE: each tape measured on the drawn geometry (Tools/FinalPass/pickups/spotlib.py: props without collision get
   query collision on the Camera channel only, so they count):
   - support: downward traces from just above the base, under every bottom vertex and the outline of the body and
     reels; every one lands on the same top (normal z >= .9), the base within 0.5 cm of it and never below, no part
     of the outline over a drop, and the outline pushed out 1.5 cm still on that top (not a shelf edge);
   - clear: nothing drawn inside its box raised 0.3 cm, no pickup place under or beside it;
   - reach: a navmesh path from the chair (300, -420, -330), boards off and doors open, to a standing place from
     which her eye line meets the tape within reach with nothing in the way;
   - clear of every other tape (they are all placed, whichever the round shows), and no higher than her eye
     (a tape on top of a tall cabinet is out of sight);
   - a chapter machine on its chapter's floor, in a story room. A lore tape hung on a wall by its nail fails:
     Ty (29 Oct) wants every tape on a flat surface.
   The base gap allowed is 1 cm (the brief), not the pickups' 0.5. The collection's power gate is opened for the
   walk, as it is by the time she can go in there.
2. A tape that fails moves to the nearest place that passes, its base flush on the top (+0.05 cm): a machine over
   the furniture the story pass measured it on, a lore tape down onto what is under it or over the top it was on,
   or else (and a wall tape) onto the nearest flat top in its room that has no tape yet, or else onto the floor at
   the foot of the wall, or else onto a top that has a tape already (a wall tape becomes the small
   recorder the surface tapes are). Machines keep their WritingAnchor's
   world place. One that cannot be fixed is reported (and has to be dropped).
3. Saves, then measures everything again in a fresh PIE. Run map/rebuild_nav.py after it, then
   beats/place_writing.py (a machine may have moved, so the words' wall is chosen again).

Results: <project>/Review/Results/tape-spots.json (every tape, pass/fail, gaps) and TAPE_SPOT lines in the log.
"""
import json, math, os, sys
from pathlib import Path
import unreal as u

PICKUPS = Path(__file__).resolve().parent.parent / 'pickups'
sys.path.insert(0, str(PICKUPS))
import importlib, spotlib; importlib.reload(spotlib)
from spotlib import FLOORS, W, approach, augment, describe, freeze, label, main_top, obb, open_house, owner, roaming, room_of, seat_height, support, trace, xyz

LEVEL = '/Game/Presentation/Playable/Lvl_Tallow_Playable'
CAMPAIGN = '/Game/FinalPass/Pass2/DA_TallowCampaign'
OUT = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).parent / 'Review' / 'Results'
MODE = os.environ.get('TALLOW_TAPES_MODE', 'fix')
ED = u.get_editor_subsystem(u.LevelEditorSubsystem); ACT = u.get_editor_subsystem(u.EditorActorSubsystem)
# The furniture each machine was measured on by Tallow.Chapters.Story.Measure (Saved/Pass2/story-recorders.json).
AUTHORED_TOP = {'Pass2_Story_Ch1_A': 'EV05_B2_WorkBench', 'Pass2_Story_Ch1_B': 'EV04_B2_SideCabinet_00',
                'Pass2_Story_Ch2_A': 'EV05_G2_DiningTable', 'Pass2_Story_Ch3_A': 'EV04_U5_Commode_02'}
FLOOR_OF_CHAPTER = {0: 'Basement', 1: 'Ground', 2: 'Upper'}
FLUSH = .05            # base above the top, as Story.Measure placed the machines
EYE_TOP = 115.0        # the highest a tape's base may sit above her floor (her eye is 123.5 cm)
SLIDE = 46             # how far over its top a tape may slide to find a place that passes (cm)
TOP_REACH = 320.0      # a wall tape looks for a flat top within this of its nail (cm)
SURFACE_SCALE = .36    # a lore tape on a surface: the story recorder at this scale (soundbites/place_soundbites.py)
spotlib.GAP = 1.0      # base to top, cm
TAKEN = {}             # every tape's plan rectangle and height span where it stands now: tapes never meet
R = {'mode': MODE, 'errors': [], 'before': [], 'fixes': [], 'after': [], 'dropped': [], 'saved': False}
MESHES = {}


def log(*a): print('TAPE_SPOT', *a)


def chapter_lists():
    lists = {}
    campaign = u.load_asset(CAMPAIGN)
    for i, ch in enumerate(campaign.get_editor_property('chapters') if campaign else []):
        for name in ch.get_editor_property('story_object_candidates'):
            lists.setdefault(str(name), []).append(i)
    return lists


def mesh_verts(mesh):
    if mesh.get_path_name() not in MESHES:
        verts = []
        for s in range(mesh.get_num_sections(0)):
            verts += [(v.x, v.y, v.z) for v in u.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, s)[0]]
        MESHES[mesh.get_path_name()] = sorted(set((round(x, 2), round(y, 2), round(z, 2)) for x, y, z in verts))
    return MESHES[mesh.get_path_name()]


def is_wall_tape(a):
    nail = a.get_editor_property('nail') if isinstance(a, u.TallowSoundBite) else None
    return bool(nail and nail.static_mesh)


def body(a):
    """The tape's drawn vertices in its yaw frame: offsets from its location, turned back by its yaw (cm)."""
    at = a.get_actor_location(); yaw = math.radians(a.get_actor_rotation().yaw); cy, sy = math.cos(yaw), math.sin(yaw); out = []
    for c in a.get_components_by_class(u.StaticMeshComponent):
        if not c.static_mesh or not c.is_visible() or c.get_name() == 'Nail': continue
        ct = c.get_world_transform(); verts = mesh_verts(c.static_mesh)
        for v in verts[::max(1, len(verts) // 400)]:
            p = ct.transform_location(u.Vector(*v)); dx, dy = p.x - at.x, p.y - at.y
            out.append((dx * cy + dy * sy, -dx * sy + dy * cy, p.z - at.z))
    return out


class Shape:
    """A tape's body turned to a yaw: the spotlib description (offsets from its plan position, bottom at 0)."""
    def __init__(self, local, yaw):
        r = math.radians(yaw); cy, sy = math.cos(r), math.sin(r)
        self.yaw = yaw; self.low = min(v[2] for v in local)
        self.lo = [min(v[k] for v in local) for k in range(3)]; self.hi = [max(v[k] for v in local) for k in range(3)]
        d = [(x * cy - y * sy, x * sy + y * cy, z - self.low) for x, y, z in local]
        self.info = describe(d, tuple((self.hi[k] - self.lo[k]) / 2 for k in range(3)))
        self.height = self.hi[2] - self.lo[2]

    def world(self, at, x, y, z):
        r = math.radians(self.yaw); cy, sy = math.cos(r), math.sin(r)
        return (at[0] + x * cy - y * sy, at[1] + x * sy + y * cy, at[2] + z)


def box_clear(shape, at):
    """What is drawn inside the tape's box (raised 0.3 cm, 0.3 cm in at the sides), or None."""
    lo, hi = shape.lo, shape.hi
    c = shape.world(at, (lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2 + .2)
    half = u.Vector((hi[0] - lo[0]) / 2 - .3, (hi[1] - lo[1]) / 2 - .3, (hi[2] - lo[2]) / 2 - .25)
    h = u.SystemLibrary.box_trace_single(W['w'], u.Vector(*c), u.Vector(c[0], c[1], c[2] - .01), half, u.Rotator(roll=0, pitch=0, yaw=shape.yaw),
                                         spotlib.SEEN, True, W['ignore'], u.DrawDebugTrace.NONE, True)
    t = h.to_tuple() if h else None
    return label(t[9]) if t and t[0] else None


def crowds(shape, at, base):
    """A pickup place under or beside the tape (a pickup there any round would sit in it or touch it)."""
    c = shape.world(at, (shape.lo[0] + shape.hi[0]) / 2, (shape.lo[1] + shape.hi[1]) / 2, 0)
    rect = obb(c, ((shape.hi[0] - shape.lo[0]) / 2, (shape.hi[1] - shape.lo[1]) / 2), shape.yaw)
    for s in u.GameplayStatics.get_all_actors_of_class(W['w'], u.TallowPickupSpot):
        p = s.get_actor_location()
        if abs(p.z - base) > 25: continue
        if not spotlib.apart(rect, obb((p.x, p.y), (12, 12), 0), 3.0): return label(s)
    return None


def footprint(shape, at):
    c = shape.world(at, (shape.lo[0] + shape.hi[0]) / 2, (shape.lo[1] + shape.hi[1]) / 2, 0)
    base = at[2] + shape.low
    return obb(c, ((shape.hi[0] - shape.lo[0]) / 2, (shape.hi[1] - shape.lo[1]) / 2), shape.yaw), base, base + shape.height


def meets_tape(shape, at, name):
    rect, base, top = footprint(shape, at)
    for other, (orect, obase, otop) in TAKEN.items():
        if other != name and base < otop + 2 and obase < top + 2 and not spotlib.apart(rect, orect, 3.0): return other
    return None


def measure(shape, at, chapter, surface=None, reach=True, name=None):
    """A tape of this shape with its location at `at` (x, y, z): every check and its gaps."""
    base = at[2] + shape.low
    row = {'at': xyz(at), 'yaw': round(shape.yaw, 1), 'base_z': round(base, 2), 'room': room_of((at[0], at[1], base + 5)), 'problems': []}
    t = trace((at[0], at[1], base + 30), (at[0], at[1], base - 60))
    under = owner(t) if t else None
    row['under_centre'] = label(under) if under else None
    if t: row['centre_gap_cm'] = round(base - t[5].z, 2); row['centre_normal_z'] = round(t[7].z, 3)
    surface = surface or under
    row['surface'] = label(surface) if surface else None
    if not surface: row['problems'].append('nothing under it'); row['passed'] = False; return row
    m, bad = support(shape.info, at[0], at[1], base, surface)
    row.update(m)
    if bad: row['problems'].append('support: ' + bad)
    if t and t[7].z < .9: row['problems'].append('top not flat (normal z %.2f)' % t[7].z)
    hit = box_clear(shape, at)
    if hit: row['problems'].append('passes through ' + hit)
    near = crowds(shape, at, base)
    if near: row['problems'].append('crowds the pickup place ' + near)
    other = meets_tape(shape, at, name)
    if other: row['problems'].append('meets the tape ' + other)
    floor_name = min(FLOORS, key=lambda f: abs(base - FLOORS[f] - 80))
    if chapter in FLOOR_OF_CHAPTER and floor_name != FLOOR_OF_CHAPTER[chapter]: row['problems'].append('wrong floor for the chapter: ' + floor_name)
    if row['room'] == 'None' or (chapter is not None and row['room'] == 'collection'): row['problems'].append('not in a story room')
    if base - FLOORS[floor_name] > EYE_TOP: row['problems'].append('above her eye (%.0f cm over the floor)' % (base - FLOORS[floor_name]))
    if reach and not row['problems']:
        c = shape.world(at, (shape.lo[0] + shape.hi[0]) / 2, (shape.lo[1] + shape.hi[1]) / 2, 0)
        a = approach([(c[0], c[1], base + shape.height * .6), (c[0], c[1], base + shape.height)], FLOORS[floor_name], False)
        row['reach'] = a
        if not a: row['problems'].append('no navmesh walk from the chair to a place in reach and sight of it')
    row['passed'] = not row['problems']
    return row


def slide(shape, chapter, surface, reasons, centre, half=(SLIDE, SLIDE), step=2, near=None, name=None, top=None):
    """The nearest place (to `near`, else `centre`) over `surface`, within `half` of `centre`, where the tape passes
    with its base flush. `top`: the height of the face to sit on (default: the furniture's own top)."""
    top = main_top(surface) if top is None else top
    if top is None: reasons['its top has no flat face'] = 1; return None
    near = near or centre; nx, ny = int(half[0]), int(half[1])
    grid = [(centre[0] + dx, centre[1] + dy) for dx in range(-nx, nx + 1, step) for dy in range(-ny, ny + 1, step)]
    for x, y in sorted(grid, key=lambda q: (q[0] - near[0]) ** 2 + (q[1] - near[1]) ** 2):
        z, why = seat_height(shape.info, x, y, top, surface)
        if z is None: reasons[why] = reasons.get(why, 0) + 1; continue
        m, bad = support(shape.info, x, y, z + FLUSH, surface)
        if bad: reasons[bad] = reasons.get(bad, 0) + 1; continue
        row = measure(shape, (x, y, z + FLUSH - shape.low), chapter, surface, name=name)
        if row['passed']: return row
        why = row['problems'][0][:60]; reasons[why] = reasons.get(why, 0) + 1
    return None


def room_centre(room, floor):
    """The middle of a room's cell on the house plan grid (spotlib.room_of)."""
    for col, (x0, x1) in enumerate(((0, 585), (585, 975), (975, 1575))):
        for row, (y0, y1) in enumerate(((-650, 0), (-1300, -650))):
            if room_of(((x0 + x1) / 2, (y0 + y1) / 2, floor + 30)) == room: return ((x0 + x1) / 2, (y0 + y1) / 2)
    return None


def tops_near(p, floor, room):
    """Furniture in the room with a flat top at a child's reach, nearest first (house shell and big pieces left out),
    each with whether a tape stands on it already."""
    out = []
    for a in u.GameplayStatics.get_all_actors_of_class(W['w'], u.StaticMeshActor):
        if a in W['ignore'] or a.get_editor_property('hidden'): continue
        o, e = a.get_actor_bounds(False)
        if max(e.x, e.y) > 200 or min(e.x, e.y) < 9 or not (35 < o.z + e.z - floor < 120): continue
        if room_of((o.x, o.y, floor + 30)) != room: continue
        d = math.dist((o.x, o.y), (p[0], p[1]))
        held = any(abs(b - (o.z + e.z)) < 12 and all(abs(c[0] - o.x) <= e.x and abs(c[1] - o.y) <= e.y for c in r) for r, b, _ in TAKEN.values())
        if d < TOP_REACH: out.append((held, d, a))
    return [(held, a) for held, _, a in sorted(out, key=lambda q: (q[0], q[1]))]


def find_top(local, start, wall, name):
    """The nearest flat top in its room with no tape on it yet (squared to the furniture, its front to the middle of
    the room), or else the floor at the foot of the wall, or else a top that has a tape already (a chapter machine's
    top would read as two objectives). `wall`: the wall's normal for a tape hung on it, else None."""
    floor = min(FLOORS.values(), key=lambda f: abs(start[2] - f - (140 if wall else 80)))
    off = (start[0] + wall[0] * 30, start[1] + wall[1] * 30) if wall else (start[0], start[1])
    room = room_of((off[0], off[1], floor + 30)); middle = room_centre(room, floor) or off; reasons = {}
    def square(yaw0, x, y):
        into = math.degrees(math.atan2(middle[1] - y, middle[0] - x))
        return min((yaw0 + k * 90 for k in range(4)), key=lambda q: abs((q - into + 180) % 360 - 180))
    def on_top(surface):
        o, e = surface.get_actor_bounds(False)
        shape = Shape(local, square(surface.get_actor_rotation().yaw, o.x, o.y))
        return slide(shape, None, surface, reasons, (o.x, o.y), (e.x, e.y), 3, off, name), shape
    tops = tops_near(off, floor, room)
    for held, surface in tops:
        if held: continue
        best, shape = on_top(surface)
        if best: return best, shape, None
    t = trace((off[0], off[1], floor + 60), (off[0], off[1], floor - 20))
    if t and owner(t):
        shape = Shape(local, square(0, off[0], off[1]))
        best = slide(shape, None, owner(t), reasons, off, (36, 36), 3, off, name)
        if best: return best, shape, None
    for held, surface in tops:
        if not held: continue
        best, shape = on_top(surface)
        if best: return best, shape, None
    return None, None, 'no flat top in %s and no floor by it passes: %s' % (room, json.dumps(reasons)[:400])


def setup(w):
    W['w'] = w; freeze(w)
    player = u.GameplayStatics.get_player_pawn(w, 0)
    moving = roaming(w)
    for i, a in enumerate(moving): a.set_actor_location(u.Vector(-9000 + i * 50, -9000, -5000), False, True)
    tapes = list(u.GameplayStatics.get_all_actors_of_class(w, u.TallowStoryObject)) + list(u.GameplayStatics.get_all_actors_of_class(w, u.TallowSoundBite))
    W['ignore'] = moving + [player] + list(u.GameplayStatics.get_all_actors_of_class(w, u.TallowCreature)) + tapes
    R['augmented'] = augment(w)
    R['opened_house'] = open_house(w, player)
    # The collection opens with the power in the last chapter; its gate is opened here for the walk in.
    for gate in u.GameplayStatics.get_all_actors_of_class(w, u.TallowRouteGate):
        gate.set_editor_property('requires_power', False)
        gate.get_editor_property('door').set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
    return sorted(tapes, key=lambda s: (not isinstance(s, u.TallowStoryObject), s.get_actor_label()))


def measure_all(tapes, lists, into, fix):
    fixes = []
    template = next((body(t) for t in tapes if isinstance(t, u.TallowSoundBite) and not is_wall_tape(t)), None)
    TAKEN.clear()
    for s in tapes:
        if not is_wall_tape(s): p = s.get_actor_location(); TAKEN[s.get_actor_label()] = footprint(Shape(body(s), s.get_actor_rotation().yaw), (p.x, p.y, p.z))
    for s in tapes:
        name = s.get_actor_label(); machine = isinstance(s, u.TallowStoryObject)
        chapter = s.get_editor_property('chapter_index') if machine else None
        p = s.get_actor_location(); start = (p.x, p.y, p.z)
        row = {'tape': name, 'kind': 'machine' if machine else ('wall tape' if is_wall_tape(s) else 'lore tape'), 'chapter': chapter,
               'in_chapter_lists': lists.get(name, []) if machine else None}
        if row['kind'] == 'wall tape':
            row.update({'at': xyz(p), 'problems': ['hangs on the wall by its nail, not on a flat top'], 'passed': False})
        else:
            row.update(measure(Shape(body(s), s.get_actor_rotation().yaw), start, chapter, name=name))
        into.append(row)
        log(name, row['kind'], 'PASS' if row['passed'] else 'FAIL', 'on', row.get('surface'),
            'gap max %s min %s centre %s' % (row.get('bottom_gap_max'), row.get('bottom_gap_min'), row.get('centre_gap_cm')), '' if row['passed'] else row['problems'])
        if row['passed'] or not fix: continue
        reasons = {}; best = None; shape = None; why = ''
        if row['kind'] != 'wall tape':
            shape = Shape(body(s), s.get_actor_rotation().yaw)
            named = next((a for a in u.GameplayStatics.get_all_actors_of_class(W['w'], u.Actor) if a.get_actor_label() == AUTHORED_TOP.get(name)), None)
            t = trace((p.x, p.y, row['base_z'] + 30), (p.x, p.y, row['base_z'] - 60))
            surface = named or (owner(t) if t else None)
            if not machine:
                # Straight down onto whatever is under it (the shelf it hovers over), then over that piece's top.
                d = trace((p.x, p.y, row['base_z'] + 2), (p.x, p.y, row['base_z'] - 40))
                if d and owner(d) and d[7].z > .9: best = slide(shape, chapter, owner(d), reasons, start, (12, 12), 2, name=name, top=d[5].z)
            if surface and not best: best = slide(shape, chapter, surface, reasons, start, name=name)
            if not best: why = 'no place on %s passes: %s' % (label(surface), json.dumps(reasons)[:300])
        if not best and not machine:
            up = s.get_actor_up_vector() if row['kind'] == 'wall tape' else None
            local = template if row['kind'] == 'wall tape' else body(s)
            best, shape, more = find_top(local, start, (up.x, up.y) if up else None, name)
            why = (why + '; ' if why else '') + (more or '')
        if best:
            f = {'tape': name, 'kind': row['kind'], 'surface': best['surface'], 'from': xyz(p), 'to': best['at'], 'yaw': round(shape.yaw, 1),
                 'moved_cm': round(math.dist(xyz(p), best['at']), 1), 'gap_max_after': best.get('bottom_gap_max'), 'reach': best.get('reach')}
            TAKEN[name] = footprint(shape, best['at'])
            fixes.append(f); R['fixes'].append(f); log(name, 'FIX', json.dumps(f))
        else:
            R['dropped'].append({'tape': name, 'why': why}); log(name, 'CANNOT FIX', why)
    return fixes


def surface_form(tape):
    """A wall tape made the small recorder the surface tapes are (soundbites/place_soundbites.py on_surface)."""
    info = json.loads(open(u.Paths.project_dir() + 'SourceAssets/Story/Recorder/recorder.json').read())
    book = tape.get_editor_property('book'); book.set_static_mesh(u.load_asset('/Game/FinalPass/Story/SM_StoryRecorder'))
    book.set_relative_scale3d(u.Vector(SURFACE_SCALE, SURFACE_SCALE, SURFACE_SCALE))
    for name, r in zip(('reel_a', 'reel_b'), info['reels']):
        c = tape.get_editor_property(name); c.set_static_mesh(u.load_asset('/Game/FinalPass/Story/' + r['name'])); c.set_relative_location(u.Vector(*r['hub_cm']), False, False)
    tape.get_editor_property('nail').set_static_mesh(None)


def main():
    assert ED.load_level(LEVEL); yield 3
    u.SystemLibrary.execute_console_command(None, 'Editor.AsyncAssetCompilationFinishAll'); yield 6
    lists = chapter_lists(); R['chapter_lists'] = lists
    ED.editor_request_begin_play(); yield 8
    tapes = setup(u.EditorLevelLibrary.get_game_world()); yield 8
    fixes = measure_all(tapes, lists, R['before'], MODE != 'audit')
    ED.editor_request_end_play(); yield 4
    if MODE == 'audit' or not fixes:
        R['after'] = R['before']; return
    by_label = {a.get_actor_label(): a for a in ACT.get_all_level_actors() if isinstance(a, (u.TallowStoryObject, u.TallowSoundBite))}
    for f in fixes:
        tape = by_label[f['tape']]
        anchor = tape.get_editor_property('writing_anchor') if isinstance(tape, u.TallowStoryObject) else None
        words = anchor.get_world_location() if anchor else None
        if f['kind'] == 'wall tape': surface_form(tape)
        tape.set_actor_location_and_rotation(u.Vector(*f['to']), u.Rotator(roll=0, pitch=0, yaw=f['yaw']), False, False)
        if anchor: anchor.set_world_location(words, False, False)
        log(f['tape'], 'MOVED to', f['to'], 'yaw', f['yaw'])
    R['saved'] = bool(ED.save_current_level()); yield 3
    ED.editor_request_begin_play(); yield 8
    tapes = setup(u.EditorLevelLibrary.get_game_world()); yield 8
    measure_all(tapes, lists, R['after'], False)
    ED.editor_request_end_play(); yield 2


def done(e=None):
    if e: R['errors'].append(e); print('TAPE_SPOT_ERROR', e)
    must = [r for r in R['after'] if r['kind'] != 'machine' or r['in_chapter_lists']]
    R['passed'] = not R['errors'] and bool(R['after']) and all(r['passed'] for r in must)
    OUT.mkdir(parents=True, exist_ok=True); (OUT / ('tape-spots%s.json' % ('-audit' if MODE == 'audit' else ''))).write_text(json.dumps(R, indent=1))
    log('RESULT', 'PASS' if R['passed'] else 'FAIL', len(R['after']), 'tapes', len(R['fixes']), 'fixed', len(R['dropped']), 'cannot fix')


spotlib.run(main(), done, timeout=2400)
