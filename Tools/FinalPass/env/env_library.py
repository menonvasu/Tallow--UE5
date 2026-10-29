"""The house environment library: every finish, prop fix and room recipe as data. Text, so each floor can add its rooms
in its own env_rooms.py; build_env_materials.py, apply_room_finish.py and fix_furniture.py generate the assets
and map changes from it.

Units: cm. Texture sizes are real-world sizes times 1.3 (the house is built a third over life size).
"""
# ---------------------------------------------------------------------------------------------------------------
# Scanned texture sets: key -> Poly Haven id (CC0; fetch_polyhaven.py downloads them, 2k). Each gives T_Env_<key>_D/_N/_ARM.
SETS = {
    'Paper': 'decrepit_wallpaper', 'Floor': 'old_wooden_floor_02', 'Plaster': 'white_plaster_02', 'Paint': 'painted_plaster_wall',
    'Joinery': 'wood_table_worn', 'Velvet': 'velour_velvet', 'Damask': 'quatrefoil_jacquard_fabric', 'Iron': 'rusty_metal_04',
    'Firebrick': 'factory_brick', 'Tiles': 'terracotta_floor_tiles', 'Leather': 'brown_leather', 'Linen': 'rough_linen',
    'Jacquard': 'floral_jacquard', 'DarkWood': 'dark_wood', 'Planks': 'worn_planks', 'Limewash': 'white_plaster_rough_01',
    'Quarry': 'worn_tile_floor', 'Hallway': 'checkered_pavement_tiles', 'Brick': 'brick_wall_13', 'Flags': 'slate_floor_03',
}
# Wallpaper patterns (Met Open Access, CC0): key -> (Met object id, repeat across cm, repeat down cm at house scale)
PATTERNS = {'WillowBough': (384022, 34.4, 58.4), 'Daisy': (384017, 33.5, 44.6)}
PATTERN_FILES = {'WillowBough': 'willow_bough_tile.png', 'Daisy': 'daisy_tile.png'}   # make_wallpaper.py output

# ---------------------------------------------------------------------------------------------------------------
# Surface finishes for props and anything without its own maps (world-aligned, M_Env_World). name -> (set, params)
# Params (4-vectors): Tile (u, v, -, -), Tint (rgb, saturation), Rough (mul, add, metal mul, specular),
# Opt (-, soft blend for folded cloth, normal strength, AO strength), Extra (metal add, roughness lift from the scan's
# lightness, that lightness's mean, -), Grime (dirt height, dirt, soot start z, soot).
def _s(set_, tile, tint, rough, opt=(0, 0, 1.0, 0.5), extra=(0, 0, 0.2, 0), grime=(0, 0, 9999, 0)):
    return (set_, dict(Tile=(tile, tile, 100, 100), Tint=tint, Rough=rough, Opt=opt, Extra=extra, Grime=grime))


SURFACES = {
    'DarkWood': _s('Joinery', 72, (1.1, 0.98, 0.92, 0.95), (0.5, -0.02, 0, 0.45), (0, 1, 1.0, 0.5), (0, 0.15, 0.04, 0)),
    'PolishedWood': _s('Joinery', 72, (0.85, 0.7, 0.6, 0.95), (0.38, -0.02, 0, 0.5), (0, 1, 1.0, 0.5), (0, 0.1, 0.04, 0)),
    'PaintedWood': _s('Paint', 200, (0.6, 0.53, 0.41, 0.8), (0.55, 0.0, 0, 0.35), (0, 1, 0.6, 0.4), (0, 0, 0.37, 0)),
    'PaintedOlive': _s('Paint', 200, (0.22, 0.25, 0.16, 0.8), (0.55, 0.0, 0, 0.35), (0, 1, 0.6, 0.4)),
    'PaintedOxide': _s('Paint', 200, (0.36, 0.13, 0.08, 0.8), (0.55, 0.0, 0, 0.35), (0, 1, 0.6, 0.4)),
    'Brass': _s('Paint', 60, (1.5, 1.05, 0.45, 1), (0.4, 0.0, 0, 0.5), (0, 1, 0.4, 0.3), (0.9, 0, 0.37, 0)),
    'Iron': _s('Iron', 60, (0.5, 0.47, 0.45, 0.35), (0.75, 0.0, 1.0, 0.5), (0, 1, 1.0, 0.4), (0.35, 0, 0.08, 0)),
    'Bakelite': _s('Joinery', 40, (0.22, 0.15, 0.11, 0.8), (0.3, 0.0, 0, 0.5), (0, 1, 0.3, 0.3)),
    'BlackGlass': _s('Paint', 60, (0.03, 0.03, 0.03, 0), (0, 0.06, 0, 0.5), (0, 1, 0.0, 0.0)),
    'Wax': _s('Paint', 30, (0.85, 0.78, 0.6, 0.8), (0.4, 0.05, 0, 0.35), (0, 1, 0.3, 0.2)),
    'Porcelain': _s('Paint', 40, (0.8, 0.76, 0.68, 0.8), (0.2, 0.0, 0, 0.5), (0, 1, 0.2, 0.2)),
    'Ivory': _s('Paint', 40, (0.78, 0.7, 0.52, 0.8), (0.4, 0.0, 0, 0.5), (0, 1, 0.3, 0.3)),
    'Ebony': _s('Joinery', 40, (0.28, 0.24, 0.22, 0.5), (0.4, 0.0, 0, 0.5), (0, 1, 0.5, 0.3)),
    'Velvet': _s('Velvet', 39, (0.55, 0.4, 0.4, 0.75), (1.0, 0.05, 0, 0.3), (0, 1, 1.0, 0.6)),
    'Damask': _s('Damask', 47, (2.6, 2.0, 1.15, 0.0), (1.0, 0.1, 0, 0.3), (0, 1, 1.0, 0.5), grime=(0, 0, 260, 0.25)),
    'Linen': _s('Linen', 35, (0.75, 0.7, 0.6, 0.8), (1.0, 0.05, 0, 0.3), (0, 1, 1.0, 0.5)),
    'Jacquard': _s('Jacquard', 33, (0.8, 0.7, 0.62, 0.8), (1.0, 0.05, 0, 0.3), (0, 1, 1.0, 0.5)),
    'Leather': _s('Leather', 52, (0.7, 0.55, 0.45, 0.85), (0.8, 0.0, 0, 0.4), (0, 1, 1.0, 0.5)),
    'Coal': _s('Iron', 25, (0.11, 0.105, 0.1, 0.0), (0.55, -0.08, 0, 0.5), (0, 1, 1.0, 0.3), (0.05, 0, 0.08, 0)),
    'Planks': _s('Planks', 182, (0.9, 0.85, 0.8, 0.85), (1.0, 0.0, 0, 0.35), (0, 1, 1.0, 0.5)),
}
# Parlour pieces with their own marks (soot spots in Soot0/Soot1, a printed pattern in Pat / pattern).
SURFACES.update({
    'G1_Chimney': ('Paper', dict(Tile=(325, 325, 34.4, 58.4), Pat=(0.153, 0.25, 1, 0.55), Tint=(0.62, 0.57, 0.48, 0.9), Rough=(0.9, 0.0, 0, 0.3),
                                 Grime=(28, 0.3, 225, 0.38), Soot0=(125, -325, 310, 120), Soot1=(125, -325, 100, 60), Mark=(0.55, 0, 0, 0),
                                 Opt=(0, 0, 1.0, 0.6), Extra=(0, 0, 0.15, 0), pattern='WillowBough')),
    'G1_Mantel': ('Paint', dict(Tile=(200, 200, 100, 100), Tint=(0.52, 0.47, 0.38, 0.7), Rough=(0.45, 0.0, 0, 0.4), Grime=(10, 0.15, 250, 0.3),
                                Soot0=(135, -325, 100, 70), Mark=(0.85, 0, 0, 0), Opt=(0, 0, 1.0, 0.5), Extra=(0, 0, 0.37, 0))),
    'G1_HearthTiles': ('Tiles', dict(Tile=(150, 150, 100, 100), Tint=(1.3, 1.0, 0.9, 0.6), Rough=(0.3, -0.08, 0, 0.5), Soot0=(135, -325, 50, 60),
                                     Mark=(0.5, 0, 0, 0), Opt=(0, 0, 0.8, 0.5))),
    'G1_Firebox': ('Firebrick', dict(Tile=(130, 130, 100, 100), Tint=(0.4, 0.33, 0.3, 0.6), Rough=(1.0, 0.05, 0, 0.3), Grime=(0, 0, -9999, 0.5),
                                     Soot0=(100, -325, 100, 70), Mark=(0.8, 0, 0, 0), Opt=(0, 1, 1.0, 0.7))),
})

# Per-furnishing tuning of the props' own scanned maps (MI_Env_P_<Name>_<slot>). Tint (rgb, saturation),
# Rough (mul, add, metal mul, AO strength), Recol/Range (re-cover the light parts, e.g. an upholstered seat).
PROP_TUNE = {
    'PedestalTable': dict(Tint=(1.0, 0.92, 0.85, 1), Rough=(0.7, -0.05, 1, 0.8)),
    'TeaSet': dict(Tint=(0.92, 0.88, 0.8, 0.9), Rough=(0.45, -0.04, 1, 0.8), Spec=0.5),
    'Armchair': dict(Tint=(0.95, 0.88, 0.82, 1), Recol=(0.12, 0.035, 0.03, 1), Range=(0.09, 0.17, 0.26, 0)),
    'GreenChair': dict(Tint=(0.95, 0.9, 0.82, 0.9)),
    'Mirror': dict(Tint=(0.9, 0.85, 0.75, 1)),
    'MantelClock': dict(Tint=(1, 0.95, 0.9, 1), Rough=(0.75, -0.05, 1, 0.8)),
    'Commode': dict(Rough=(0.8, -0.03, 1, 0.8)), 'Bookcase': dict(Rough=(0.9, 0, 1, 0.8)), 'GrandfatherClock': dict(Rough=(0.75, -0.03, 1, 0.8)),
    'ChinaHutch': dict(Rough=(0.8, -0.03, 1, 0.8)), 'HallConsole': dict(Rough=(0.75, -0.03, 1, 0.8)),
}

# ---------------------------------------------------------------------------------------------------------------
# Prop fixes (fix_furniture.py). A slot is fixed when what it renders is a shared house surface (ArtDirection09
# *_World), a 32 px flat swatch or the engine default:
#  1. a Poly Haven / own furnishing whose own imported material has its maps -> MI_Env_P_<Mesh>_<slot> built from them
#     (glass and other see-through slots keep their own material);
#  2. otherwise the slot name says what the part is made of -> that SURFACE.
SLOT_WORDS = [  # (words in the slot name, surface), first match wins
    (('wax',), 'Wax'), (('bakelite',), 'Bakelite'), (('blackglass', 'glass'), 'BlackGlass'), (('brass', 'gold', 'gilt'), 'Brass'),
    (('iron', 'steel', 'metal'), 'Iron'), (('coal',), 'Coal'), (('velvet', 'wool', 'plush'), 'Velvet'), (('linen', 'sheet'), 'Linen'),
    (('leather',), 'Leather'), (('rug', 'runner', 'carpet'), 'Velvet'), (('oxide',), 'PaintedOxide'), (('olive',), 'PaintedOlive'),
    (('paintedpanel', 'paint'), 'PaintedWood'), (('ashtimber', 'planks', 'board'), 'Planks'), (('porcelain', 'china'), 'Porcelain'),
    (('oak', 'walnut', 'wood', 'timber', 'mahogany'), 'DarkWood'),
]
# Own materials that are generic house finishes in disguise are treated like the shared surfaces, except these.
KEEP_OWN = {'M_HouseV04_Rug', 'M_HouseV04_Coal'}
# Prop actors whose overrides I set by hand (rooms/Review/Results/hand-edits.json): never restyled.
HAND_MATERIALS = {'StaticMeshActor_24', 'StaticMeshActor_25'}

# Room cells (wall centre lines) for filtering and audits: name -> (level, x range, y range)
ROOM_GRID = {
    'B1 Wake room': ('B', (0, 585), (-650, 0)), 'B2 Workshop': ('B', (0, 585), (-1300, -650)),
    'B3 Cellar stair hall': ('B', (585, 975), (-1020, 0)), 'B4 Boiler room': ('B', (975, 1560), (-650, 0)),
    'B5 Laundry': ('B', (975, 1560), (-1300, -650)),
    'G1 Parlour': ('G', (0, 585), (-715, 0)), 'G2 Dining room': ('G', (0, 585), (-1300, -715)),
    'G3 Front hall': ('G', (585, 975), (-1020, 0)), 'G4 Study': ('G', (975, 1560), (-650, 0)),
    'G5 Kitchen': ('G', (975, 1560), (-1300, -650)), 'G6 Pantry': ('G', (585, 975), (-1300, -1020)),
    'U1 Toy room': ('U', (0, 585), (-650, 0)), 'U2 Collection': ('U', (0, 585), (-1300, -650)),
    'U3 Upper landing': ('U', (585, 975), (-1020, 0)), 'U3b Back landing': ('U', (585, 975), (-1300, -1020)),
    'U4 Bedroom': ('U', (975, 1560), (-650, 0)), 'U5 Nursery': ('U', (975, 1560), (-1300, -650)),
}


def room_of(x, y, z):
    for name, (lv, (x0, x1), (y0, y1)) in ROOM_GRID.items():
        lo, hi = LEVELS[lv]
        if x0 <= x <= x1 and y0 <= y <= y1 and lo - 20 <= z <= hi + 10:
            return name
    return None


# ---------------------------------------------------------------------------------------------------------------
# Room recipes (apply_room_finish.py). Box: x, y ranges of the wall centre lines and the floor level. Each room names
# a finish per role; a finish is (texture set, params). Roles: wall, wall_lower (the second wall material), floor,
# ceiling, joinery (skirting, dado, door casings), paint (window joinery, cornice), portal_paint.
LEVELS = {'B': (-330.0, -30.0), 'G': (0.0, 360.0), 'U': (390.0, 720.0)}
FLOORS_Z = {'B': -330.0, 'G': 0.0, 'U': 390.0}


def paper(pattern, tint=(0.62, 0.57, 0.48, 0.9), soot=0.38, spots=0.55):
    return ('Paper', dict(Tile=(325, 325) + PATTERNS[pattern][1:], Pat=(0.153, 0.25, 1, 0.55), Tint=tint, Rough=(0.9, 0.0, 0, 0.3),
                          Grime=(28, 0.3, 225, soot), Mark=(spots, 0, 0, 0), Opt=(0, 0, 1.0, 0.6), Extra=(0, 0, 0.15, 0), pattern=pattern))


def boards(tint=(2.35, 2.05, 1.85, 0.85), border=75):
    return ('Floor', dict(Tile=(264, 264, 100, 100), Tint=tint, Rough=(1.0, -0.06, 0, 0.4), Grime=(0, 0, 9999, 0),
                          Mark=(0, border, 0.35, 0.12), Opt=(0, 0, 1.0, 0.5), Extra=(0, 0.25, 0.042, 0)))


def plaster_ceiling(tint=(0.9, 0.8, 0.63, 0.7), soot=0.18, spots=0.6):
    return ('Plaster', dict(Tile=(130, 130, 100, 100), Tint=tint, Rough=(1.0, 0.03, 0, 0.3), Grime=(0, 0, 0, soot),
                            Mark=(spots, 0, 0, 0), Opt=(0, 0, 0.8, 0.3), Extra=(0, 0, 0.25, 0)))


def plaster_walls(tint=(0.62, 0.58, 0.48, 0.75), dirt=0.35, soot=0.3):
    """Limewashed or distempered plaster: hall, kitchen, pantry."""
    return ('Limewash', dict(Tile=(130, 130, 100, 100), Tint=tint, Rough=(1.0, 0.02, 0, 0.3), Grime=(35, dirt, 220, soot),
                             Mark=(0.6, 0, 0, 0), Opt=(0, 0, 1.0, 0.5), Extra=(0, 0, 0.3, 0)))


def tiles(tint=(1.0, 0.92, 0.85, 0.8), size=260):
    """Worn quarry tiles (hall, kitchen, pantry share one set so a floor slab stays within two)."""
    return ('Quarry', dict(Tile=(size, size, 100, 100), Tint=tint, Rough=(0.9, -0.05, 0, 0.4), Grime=(0, 0, 9999, 0),
                           Mark=(0, 60, 0.3, 0.1), Opt=(0, 0, 1.0, 0.6), Extra=(0, 0.15, 0.2, 0)))


def joinery(tint=(1.15, 1.02, 0.95, 0.9)):
    return ('Joinery', dict(Tile=(72, 72, 100, 100), Tint=tint, Rough=(0.6, 0.0, 0, 0.4), Grime=(12, 0.2, 9999, 0),
                            Opt=(0, 0, 1.0, 0.5), Extra=(0, 0.15, 0.04, 0)))


def paint(tint=(0.6, 0.53, 0.41, 0.8)):
    return ('Paint', dict(Tile=(200, 200, 100, 100), Tint=tint, Rough=(0.55, 0.0, 0, 0.35), Grime=(10, 0.15, 250, 0.3),
                          Mark=(0.4, 0, 0, 0), Opt=(0, 0, 0.6, 0.4), Extra=(0, 0, 0.37, 0)))


ROOMS = {
    # The parlour benchmark. Soot spots: chimney breast above the fire, oil lamp, table candle.
    'G1 Parlour': dict(level='G', x=(0, 585), y=(-715, 0), soot=[(125, -325, 310, 120), (85, -700, 250, 70), (291, -319, 360, 55)],
                       wall=paper('WillowBough'), wall_lower=paint(), floor=boards(), ceiling=plaster_ceiling(),
                       joinery=joinery(), paint=paint(), portal_paint=paint()),
    # The rest of the ground floor. A wall run reaches several rooms, so walls use two sets between them
    # (Paper, Limewash) and the ground slab two (Floor boards, Quarry tiles; the cellar ceilings below use Floor too).
    'G2 Dining room': dict(level='G', x=(0, 585), y=(-1300, -715), soot=[(290, -1000, 360, 80)],
                           wall=paper('Daisy', tint=(0.5, 0.44, 0.37, 0.8)), wall_lower=paint((0.3, 0.13, 0.09, 0.8)),
                           floor=boards(), ceiling=plaster_ceiling(), joinery=joinery(), paint=paint()),
    'G3 Front hall': dict(level='G', x=(585, 975), y=(-1020, 0), wall=plaster_walls((0.5, 0.47, 0.4, 0.7)), stairs=joinery((0.8, 0.72, 0.66, 0.85)),
                          wall_lower=paint((0.2, 0.13, 0.09, 0.8)), floor=tiles((0.9, 0.78, 0.7, 0.75), 200),
                          ceiling=plaster_ceiling(soot=0.25), joinery=joinery(), paint=paint()),
    'G4 Study': dict(level='G', x=(975, 1560), y=(-650, 0), wall=paper('WillowBough', tint=(0.44, 0.46, 0.37, 0.8)),
                     wall_lower=paint((0.22, 0.2, 0.14, 0.8)), floor=boards((2.0, 1.75, 1.6, 0.85)), ceiling=plaster_ceiling(),
                     joinery=joinery((0.9, 0.8, 0.75, 0.9)), paint=paint()),
    'G5 Kitchen': dict(level='G', x=(975, 1560), y=(-1300, -650), soot=[(1250, -1250, 300, 140)],
                       wall=plaster_walls((0.66, 0.62, 0.52, 0.7), dirt=0.45, soot=0.45), wall_lower=paint((0.2, 0.24, 0.16, 0.8)),
                       floor=tiles((1.05, 0.8, 0.68, 0.85), 260), ceiling=plaster_ceiling(soot=0.4, spots=0.8),
                       joinery=joinery((0.85, 0.8, 0.72, 0.8)), paint=paint((0.24, 0.27, 0.19, 0.8))),
    'G6 Pantry': dict(level='G', x=(585, 975), y=(-1300, -1020), wall=plaster_walls((0.66, 0.66, 0.6, 0.6), dirt=0.3, soot=0.2),
                      wall_lower=paint((0.5, 0.5, 0.45, 0.6)), floor=tiles((0.75, 0.72, 0.68, 0.4), 230),
                      ceiling=plaster_ceiling(soot=0.12), joinery=joinery((0.8, 0.75, 0.7, 0.8)), paint=paint()),
}


# ---------------------------------------------------------------------------------------------------------------
# Each floor's rooms: every Tools/FinalPass/<folder>/env_rooms.py may define ROOMS, SURFACES, PROP_TUNE, SETS, PATTERNS
# (dicts, merged in folder order) and SLOT_WORDS (list, put first). It can use this module's helpers:
#   from env_library import paper, boards, plaster_ceiling, joinery, paint
def _merge_floor_rooms():
    import importlib.util
    from pathlib import Path
    for path in sorted(Path(__file__).resolve().parent.parent.glob('*/env_rooms.py')):
        spec = importlib.util.spec_from_file_location('env_rooms_' + path.parent.name.replace('-', '_'), path)
        mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
        for name in ('ROOMS', 'SURFACES', 'PROP_TUNE', 'SETS', 'PATTERNS'):
            globals()[name].update(getattr(mod, name, {}))
        SLOT_WORDS[:0] = getattr(mod, 'SLOT_WORDS', [])


_merge_floor_rooms()
