"""Shared measuring for the pickup spot scripts (survey_spots.py finds the poses, audit_spots.py checks the map).
Everything runs in a PIE world: full collision and the runtime's own hidden/visible state."""
import math, time, traceback
import unreal as u

CHAIR = u.Vector(300, -420, -330)
FLOORS = {"Basement": -330.0, "Ground": 0.0, "Upper": 390.0}
START_ROOMS_X = 585.0        # the boarded workshop door: the crowbar must be found west of it
EYE = 123.5                  # standing camera above the floor: capsule 60 + 2, camera 61.5 above the capsule centre
REACH = 185.0                # the interaction line is 200 cm from the camera
GAP = 0.5                    # bottom of the item to the top under it
MARGIN = 1.5                 # to the edge of the top
SPACING = 3.0                # between any two spots
REST = {"Crowbar": (0, 0, 90), "Fuse": (90, 0, 0), "Matchbox": (0, 0, 0)}   # ATallowPickupSpot::RestingRotation
GAME = u.TraceTypeQuery.TRACE_TYPE_QUERY1      # Visibility: what the interaction line sees
SEEN = u.TraceTypeQuery.TRACE_TYPE_QUERY2      # Camera: everything drawn, including props without collision
W = {"w": None, "ignore": []}


def xyz(v):
    if hasattr(v, "x"): v = (v.x, v.y, v.z)
    return [round(v[0], 2), round(v[1], 2), round(v[2], 2)]


def axes(p, y, r):
    """FRotationMatrix rows: where the local X, Y and Z axes go (UE's own convention)."""
    sp, cp = math.sin(math.radians(p)), math.cos(math.radians(p))
    sy, cy = math.sin(math.radians(y)), math.cos(math.radians(y))
    sr, cr = math.sin(math.radians(r)), math.cos(math.radians(r))
    return ((cp * cy, cp * sy, sp), (sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp),
            (-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp))


def turn(v, a): return tuple(v[0] * a[0][k] + v[1] * a[1][k] + v[2] * a[2][k] for k in range(3))


def hull(d):
    """Indices of the plan convex hull of the points d."""
    pts = sorted(set((round(v[0], 3), round(v[1], 3), i) for i, v in enumerate(d)))
    def cross(o, a, b): return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0: lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0: upper.pop()
        upper.append(p)
    return [p[2] for p in lower[:-1] + upper[:-1]]


class Model:
    """An item mesh laid in its resting rotation, then turned to yaw, relative to the spot (its bounds' middle over
    the spot, its bottom on it): exactly what ATallowPickupSpot::SeatItem does."""
    def __init__(self, kind, mesh):
        self.kind = kind; self.mesh = mesh; verts = []; tris = []
        for s in range(mesh.get_num_sections(0)):
            sec = u.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, s)
            base = len(verts); verts += [(v.x, v.y, v.z) for v in sec[0]]; tris += [base + i for i in sec[1]]
        index = {}; self.verts = []; remap = []
        for v in verts:
            k = (round(v[0], 2), round(v[1], 2), round(v[2], 2))
            if k not in index: index[k] = len(self.verts); self.verts.append(v)
            remap.append(index[k])
        edges = set()
        for i in range(0, len(tris), 3):
            a, b, c = remap[tris[i]], remap[tris[i + 1]], remap[tris[i + 2]]
            for p, q in ((a, b), (b, c), (c, a)):
                if p != q: edges.add((min(p, q), max(p, q)))
        self.edges = sorted(edges)
        b = mesh.get_bounding_box()
        self.corners = [(x, y, z) for x in (b.min.x, b.max.x) for y in (b.min.y, b.max.y) for z in (b.min.z, b.max.z)]
        self.cache = {}

    def at(self, yaw):
        if yaw in self.cache: return self.cache[yaw]
        rest = axes(*REST[self.kind]); spin = axes(0, yaw, 0)
        f = lambda v: turn(turn(v, rest), spin)
        cs = [f(c) for c in self.corners]
        lo = [min(c[k] for c in cs) for k in range(3)]; hi = [max(c[k] for c in cs) for k in range(3)]
        mid = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2)
        d = [(v[0] - mid[0], v[1] - mid[1], v[2] - lo[2]) for v in (f(v) for v in self.verts)]
        self.cache[yaw] = describe(d, self.box_half())
        return self.cache[yaw]

    def box_half(self):
        """Half size of the box the item fills, in its own turned frame (rest only)."""
        rest = axes(*REST[self.kind]); cs = [turn(c, rest) for c in self.corners]
        return tuple((max(c[k] for c in cs) - min(c[k] for c in cs)) / 2 for k in range(3))

    def long_axis_yaw(self):
        """Yaw at which the item's long side runs along world X."""
        h = self.box_half()
        return 0.0 if h[0] >= h[1] else 90.0


def describe(d, half):
    """Points relative to the spot (plan middle, bottom at 0): which rest on the top, which to test under."""
    lo = min(v[2] for v in d)
    bottom = [i for i, v in enumerate(d) if v[2] - lo < .35]
    cells = {}
    for i, v in enumerate(d): cells.setdefault((round(v[0]), round(v[1])), i)
    h = hull(d)
    under = sorted(set(cells.values()) | set(h) | set(bottom[:: max(1, len(bottom) // 60)]))
    return {"d": d, "half": half, "bottom": set(bottom), "under": under, "hull": h, "height": max(v[2] for v in d) - lo, "low": lo}


def trace(a, b, channel=SEEN, complex_=True, ignore=None):
    h = u.SystemLibrary.line_trace_single(W["w"], u.Vector(*a), u.Vector(*b), channel, complex_, W["ignore"] if ignore is None else ignore,
                                          u.DrawDebugTrace.NONE, True)
    if not h: return None
    t = h.to_tuple()
    return t if t[0] else None


def owner(t): return t[9]


def label(a): return a.get_actor_label() if a else "?"


def box_hit(centre, half, yaw, ignore=None):
    """What a box (turned to yaw) touches where it stands, on the drawn geometry, or None."""
    h = u.SystemLibrary.box_trace_single(W["w"], u.Vector(*centre), u.Vector(centre[0], centre[1], centre[2] - .01), u.Vector(*half),
                                         u.Rotator(pitch=0, yaw=yaw, roll=0), SEEN, True, W["ignore"] if ignore is None else ignore,
                                         u.DrawDebugTrace.NONE, True)
    if not h: return None
    t = h.to_tuple()
    return label(t[9]) if t[0] else None


def obb(c, half, yaw):
    """Corners of a footprint in the plan."""
    cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return [(c[0] + x * cy - y * sy, c[1] + x * sy + y * cy) for x, y in ((half[0], half[1]), (-half[0], half[1]), (-half[0], -half[1]), (half[0], -half[1]))]


def apart(a, b, margin):
    """Separating axis test on two plan rectangles (corners), grown by margin."""
    for poly in (a, b):
        for i in range(4):
            p, q = poly[i], poly[(i + 1) % 4]
            n = (q[1] - p[1], p[0] - q[0]); ln = math.hypot(*n) or 1; n = (n[0] / ln, n[1] / ln)
            pa = [x * n[0] + y * n[1] for x, y in a]; pb = [x * n[0] + y * n[1] for x, y in b]
            if max(pa) + margin < min(pb) or max(pb) + margin < min(pa): return True
    return False


def support(info, sx, sy, z, surface, ignore=None):
    """Under the item at spot (sx, sy, z): the worst bottom gap, the deepest vertex below the top, whether every
    vertex is over the surface (and the outline pushed out MARGIN too). Returns (measures, problem or None)."""
    start = z + info["height"] + 4; gaps = []; worst_drop = 0.0
    for i in info["under"]:
        d = info["d"][i]
        t = trace((sx + d[0], sy + d[1], start), (sx + d[0], sy + d[1], z - 25), ignore=ignore)
        if not t: return {"vertex": i}, "nothing under"
        if owner(t) != surface: return {"vertex": i}, "on " + label(owner(t))
        s = t[5].z
        if i in info["bottom"]:
            gaps.append(z + d[2] - s)
        worst_drop = min(worst_drop, s - z)
    m = {"bottom_gap_max": round(max(gaps), 3) if gaps else None, "bottom_gap_min": round(min(gaps), 3) if gaps else None,
         "lowest_top_under_cm": round(worst_drop, 2)}
    if not gaps: return m, "no bottom"
    if min(gaps) < -.1: return m, "sunk %.2f" % min(gaps)
    if max(gaps) > GAP: return m, "gap %.2f" % max(gaps)
    if worst_drop < -1.0: return m, "over a drop"
    for i in info["hull"]:
        d = info["d"][i]; ln = math.hypot(d[0], d[1]) or 1
        x, y = sx + d[0] + d[0] / ln * MARGIN, sy + d[1] + d[1] / ln * MARGIN
        t = trace((x, y, start), (x, y, z - 25), ignore=ignore)
        if not t or owner(t) != surface or t[5].z < z - 1.0: return m, "too near the edge"
    return m, None


def intersects(model, info, sx, sy, z, yaw, ignore=None):
    """Anything in the item's box (raised 0.3 cm) or 5 cm above it, or any drawn surface crossing an item edge
    (raised 0.1 cm). Returns what it meets, or None."""
    h = info["half"]
    hit = box_hit((sx, sy, z + .3 + h[2] + 2.5), (h[0], h[1], h[2] + 2.5), yaw, ignore)
    if hit: return "box meets " + hit
    for a, b in model.edges:
        p, q = info["d"][a], info["d"][b]
        t = trace((sx + p[0], sy + p[1], z + p[2] + .1), (sx + q[0], sy + q[1], z + q[2] + .1), ignore=ignore)
        if t: return "edge meets " + label(owner(t))
    return None


def seat_height(info, sx, sy, top, surface):
    """The spot height for this plan position on a top near `top`: the item lowered until it touches."""
    start = top + info["height"] + 4; best = None
    for i in info["under"]:
        d = info["d"][i]
        t = trace((sx + d[0], sy + d[1], start), (sx + d[0], sy + d[1], top - 25))
        if not t: return None, "nothing under"
        if owner(t) != surface: return None, "on " + label(owner(t))
        if t[7].z < .9: return None, "sloped"
        z = t[5].z - d[2]
        best = z if best is None else max(best, z)
    if abs(best - top) > 3: return None, "another level"
    return best, None


def walk_to(p, floor, crowbar):
    """(feet, path length) on the navmesh from the chair to near p, or None. The crowbar's whole walk stays west
    of the boarded workshop door."""
    q = u.NavigationSystemV1.project_point_to_navigation(W["w"], u.Vector(p[0], p[1], floor + 5), None, None, u.Vector(12, 12, 60))
    if isinstance(q, tuple): q = q[-1]
    if q is None or math.dist((q.x, q.y), p) > 15 or abs(q.z - floor) > 40: return None
    path = u.NavigationSystemV1.find_path_to_location_synchronously(W["w"], CHAIR, q)
    if not path or path.is_partial(): return None
    pts = path.path_points
    if len(pts) < 2 or math.dist((pts[-1].x, pts[-1].y), (q.x, q.y)) > 30: return None
    if crowbar and any(k.x >= START_ROOMS_X for k in pts): return None
    return (q.x, q.y, q.z), sum(math.dist((pts[i].x, pts[i].y, pts[i].z), (pts[i + 1].x, pts[i + 1].y, pts[i + 1].z)) for i in range(len(pts) - 1))


def approach(targets, floor, crowbar, hits=None, ignore=None):
    """The nearest-by-walk standing spot from which the eye line reaches one of the targets within reach.
    hits(eye, target) -> bool decides what reaching means; by default nothing in the way (game and drawn)."""
    centre = targets[0]; best = None
    for rr in (40, 55, 70, 85, 100, 120, 140):
        for k in range(24):
            a = k * math.pi / 12
            f = walk_to((centre[0] + math.cos(a) * rr, centre[1] + math.sin(a) * rr), floor, crowbar)
            if not f or (best and f[1] >= best["path_cm"]): continue
            eye = (f[0][0], f[0][1], f[0][2] + EYE)
            for target in targets:
                dist = math.dist(eye, target)
                if dist > REACH: continue
                if hits: ok = hits(eye, target)
                else:
                    stop = tuple(eye[j] + (target[j] - eye[j]) * (1 - 2.5 / dist) for j in range(3))
                    ok = not trace(eye, stop, GAME, False, ignore) and not trace(eye, stop, ignore=ignore)
                if ok:
                    best = {"feet": xyz(f[0]), "eye_cm": round(dist, 1), "path_cm": round(f[1]), "target": xyz(target)}
                    break
        if best: return best
    return best


def augment(w):
    """Props drawn without collision get query collision on the Camera channel only, never navigation."""
    n = 0
    for a in u.GameplayStatics.get_all_actors_of_class(w, u.Actor):
        if a in W["ignore"] or a.get_editor_property("hidden"): continue
        for c in a.get_components_by_class(u.StaticMeshComponent):
            if not c.static_mesh or not c.is_visible(): continue
            if c.get_collision_enabled() != u.CollisionEnabled.NO_COLLISION: continue
            c.set_editor_property("can_ever_affect_navigation", False)     # raw flag: no navigation update at all
            c.set_collision_response_to_all_channels(u.CollisionResponseType.ECR_IGNORE)
            c.set_collision_response_to_channel(u.CollisionChannel.ECC_CAMERA, u.CollisionResponseType.ECR_BLOCK)
            c.set_collision_enabled(u.CollisionEnabled.QUERY_ONLY); n += 1
    return n


def main_top(surface):
    """The furniture's own flat top: the height most of a grid of downward traces lands on (props on it ignored)."""
    o, e = surface.get_actor_bounds(False); heights = []
    for i in range(9):
        for j in range(9):
            x, y = o.x - e.x + 3 + (2 * e.x - 6) * i / 8, o.y - e.y + 3 + (2 * e.y - 6) * j / 8
            t = trace((x, y, o.z + e.z + 5), (x, y, o.z - e.z)); skip = list(W["ignore"])
            while t and owner(t) != surface and t[5].z > o.z - e.z + 2:      # through anything standing on it
                skip.append(owner(t)); t = trace((x, y, t[5].z + 1), (x, y, o.z - e.z), ignore=skip)
            if t and owner(t) == surface and t[7].z > .9: heights.append(t[5].z)
    if not heights: return None
    groups = {}
    for z in heights: groups.setdefault(round(z / 2), []).append(z)
    best = max(groups.values(), key=lambda g: (len(g), max(g)))
    return sorted(best)[len(best) // 2]


def room_of(p):
    """Stable chapter room IDs on the house map's fifteen-room grid."""
    x, y, z = p
    fl = 0 if z < -100 else 1 if z < 300 else 2
    if not (0 <= x < 1575 and -1300 <= y < 0): return "None"
    col = 0 if x < 585 else (1 if x < 975 else 2)
    names = ((('wake', 'workshop'), ('cellar_hall', 'cellar_hall'), ('boiler', 'laundry')),
             (('parlour', 'dining'), ('hall', 'hall'), ('study', 'back')),
             (('toys', 'collection'), ('upper_hall', 'upper_hall'), ('bedroom', 'upper_back')))
    return names[fl][col][int(y < -650)]


def open_house(w, player):
    """The house once the boards are pried and the doors unlocked: what every pickup but the crowbar is measured in.
    Planks and boards come off through their own completion, doors through SetLocked; the navmesh then rebuilds."""
    held = player.get_carried_item()
    player.set_carried_item(u.CarriedItemType.CROWBAR)
    boards = list(u.GameplayStatics.get_all_actors_of_class(w, u.TallowOptionalBoards))
    try:
        for plank in u.GameplayStatics.get_all_actors_of_class(w, u.TallowPlank): plank.call_method("OnInteractComplete", (player,))
        for board in boards:
            if board.is_route_blocked(): board.call_method("OnInteractComplete", (player,))
    finally:
        player.set_carried_item(held)
    blocked = [label(board) for board in boards if board.is_route_blocked()]
    assert not blocked, "Board removal fixture failed: %s" % blocked
    for door in u.GameplayStatics.get_all_actors_of_class(w, u.TallowDoor): door.set_locked(False)
    return {"boards_checked": len(boards), "blocked_after": blocked}


def freeze(w):
    """Mim parked far away and asleep, so nothing moves while measuring."""
    for k in u.GameplayStatics.get_all_actors_of_class(w, u.TallowCreature):
        ai = k.get_controller(); ai.allow_capture = False; ai.set_hunt_enabled(False)
        k.set_actor_location(u.Vector(-9000, -9000, 2000), False, True)


def roaming(w):
    """The pickups the director moves each run (the start box by the candle stays)."""
    return list(u.GameplayStatics.get_all_actors_of_class(w, u.TallowCrowbarPickup)) + list(u.GameplayStatics.get_all_actors_of_class(w, u.TallowFusePickup)) \
        + [b for b in u.GameplayStatics.get_all_actors_of_class(w, u.TallowMatchbox) if b.get_actor_label() != "Matchbox_Start"]


def run(gen, done, timeout=1500):
    """Drive a generator from the editor tick (it yields seconds to wait); done(error) at the end."""
    state = {"next": 0, "busy": False, "start": time.monotonic()}
    def tick(dt):
        if time.monotonic() - state["start"] > timeout: finish("timeout"); return
        if time.monotonic() < state["next"] or state["busy"]: return
        state["busy"] = True
        try: state["next"] = time.monotonic() + next(gen)
        except StopIteration: finish(None)
        except Exception: finish(traceback.format_exc())
        finally: state["busy"] = False
    def finish(e):
        u.unregister_slate_post_tick_callback(state["h"])
        try: done(e)
        finally:
            u.get_editor_subsystem(u.LevelEditorSubsystem).editor_request_end_play(); u.SystemLibrary.quit_editor()
    state["h"] = u.register_slate_post_tick_callback(tick)
