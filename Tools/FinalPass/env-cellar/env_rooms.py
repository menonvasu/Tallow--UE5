"""The cellar rooms (B1 wake room, B2 workshop, B3 cellar hall, B4 boiler room, B5 laundry) for the house library.

Merged by env/env_library.py (ROOMS, SETS, SURFACES, PROP_TUNE, SLOT_WORDS). A chandler's cellar in 1894: lime-washed
brick and lime plaster perished by rising damp, York-stone flags in the dry rooms and brick paviours where water and
coal are handled, and overhead the undersides of the ground floor's own boards between the joists. Soot gathers where
the boiler and the candles burned; the walls are cleanest where nobody stood.

Shared runs (the rule of two texture sets per generated instance): every cellar wall is lime plaster or lime-washed
brick, every lower wall perished plaster or brick, every floor flags or paviours, every ceiling the ground floor's
board set (the slab is one mesh with the floors above, so it stays on the set the ground floor already uses).
Units: cm; texture sizes are real sizes times 1.3. World z: cellar floor -330, cellar ceiling -30.
"""
from env_library import paint

# Poly Haven sets added for the cellar (CC0; fetch_cellar_textures.py downloads them, 2k).
SETS = {'Whitebrick': 'whitewashed_brick', 'Pavers': 'worn_brick_floor', 'Perished': 'damaged_plaster'}

FLOOR, CEIL = -330.0, -30.0


def limewash(tint=(0.74, 0.71, 0.64, 0.45), damp=0.4, soot=0.35, spots=0.5):
    """Lime plaster gone grey-green with damp: rising damp from the floor, smoke under the ceiling."""
    return ('Limewash', dict(Tile=(130, 130, 100, 100), Tint=tint, Rough=(1.0, 0.06, 0, 0.3), Grime=(70, damp, CEIL - 110, soot),
                             Mark=(spots, 0, 0, 0), Opt=(0, 0, 1.0, 0.55), Extra=(0, 0, 0.45, 0)))


def whitebrick(tint=(0.78, 0.74, 0.67, 0.5), damp=0.35, soot=0.4, spots=0.6):
    """Lime-washed stock brick: the wash worn off the arrises, damp low down, smoke high up."""
    return ('Whitebrick', dict(Tile=(260, 260, 100, 100), Tint=tint, Rough=(1.0, 0.05, 0, 0.3), Grime=(60, damp, CEIL - 120, soot),
                               Mark=(spots, 0, 0, 0), Opt=(0, 0, 1.0, 0.7), Extra=(0, 0, 0.45, 0)))


def perished(tint=(0.6, 0.55, 0.48, 0.55), damp=0.45):
    """Lower wall: the plaster blown by damp, brick showing through."""
    return ('Perished', dict(Tile=(240, 240, 100, 100), Tint=tint, Rough=(1.0, 0.06, 0, 0.3), Grime=(55, damp, 9999, 0),
                             Opt=(0, 0, 1.0, 0.7), Extra=(0, 0, 0.45, 0)))


def tar(tint=(0.075, 0.065, 0.055, 0.5)):
    """Lower wall of the brick rooms: tarred against the damp, scuffed, the brick's relief still under it."""
    return ('Paint', dict(Tile=(200, 200, 100, 100), Tint=tint, Rough=(0.6, 0.0, 0, 0.4), Grime=(45, 0.25, 9999, 0),
                          Opt=(0, 0, 0.6, 0.5), Extra=(0, 0.2, 0.37, 0)))


def flags(tint=(0.62, 0.6, 0.56, 0.45), edge=60):
    """Stone flags, dirt drifted against the walls."""
    return ('Flags', dict(Tile=(260, 260, 100, 100), Tint=tint, Rough=(0.95, 0.04, 0, 0.35), Grime=(0, 0, 9999, 0),
                          Mark=(0, edge, 0.3, 0.0), Opt=(0, 0, 1.0, 0.6), Extra=(0, 0.15, 0.3, 0)))


def pavers(tint=(0.62, 0.5, 0.44, 0.6), edge=55):
    """Brick paviours, blacker towards the walls (coal dust, soap scum)."""
    return ('Pavers', dict(Tile=(260, 260, 100, 100), Tint=tint, Rough=(0.95, 0.02, 0, 0.35), Grime=(0, 0, 9999, 0),
                           Mark=(0, edge, 0.35, 0.0), Opt=(0, 0, 1.0, 0.7), Extra=(0, 0.1, 0.3, 0)))


def underboards(tint=(0.85, 0.75, 0.66, 0.6), soot=0.3):
    """The ground floor's boards seen from below: unpolished, dusty, darker over the hot rooms."""
    return ('Floor', dict(Tile=(264, 264, 100, 100), Tint=tint, Rough=(1.0, 0.18, 0, 0.25), Grime=(0, 0, CEIL - 20, soot),
                          Mark=(0.6, 0, 0, 0), Opt=(0, 0, 0.8, 0.6), Extra=(0, 0, 0.042, 0)))


def deal(tint=(0.6, 0.56, 0.52, 0.18)):
    """Cellar joinery: cheap deal, once stained, now dull and scuffed (the dark, even scan: the table-top grain of the
    house joinery streaked across the long stair sides)."""
    return ('DarkWood', dict(Tile=(110, 110, 100, 100), Tint=tint, Rough=(0.9, 0.12, 0, 0.3), Grime=(25, 0.35, 9999, 0),
                             Opt=(0, 0, 1.0, 0.5), Extra=(0, 0.1, 0.2, 0)))


def cellar_paint(tint=(0.55, 0.52, 0.45, 0.5)):
    return ('Paint', dict(Tile=(200, 200, 100, 100), Tint=tint, Rough=(0.75, 0.05, 0, 0.35), Grime=(15, 0.25, CEIL - 90, 0.35),
                          Mark=(0.4, 0, 0, 0), Opt=(0, 0, 0.6, 0.4), Extra=(0, 0, 0.37, 0)))


# Soot spots (x, y, z, radius). A generated instance holds four, gathered room by room in this order, so the boiler
# (the one that matters) comes first: the wall behind it and the boards above. Then the wake room's candle table.
ROOMS = {
    'B4 Boiler room': dict(level='B', x=(975, 1560), y=(-560, 0), soot=[(1500, -95, -110, 210)],
                           wall=whitebrick(tint=(0.7, 0.66, 0.6, 0.5), soot=0.55), wall_lower=tar(),
                           floor=pavers(tint=(0.5, 0.42, 0.37, 0.55)), ceiling=underboards(tint=(0.7, 0.62, 0.55, 0.6), soot=0.5),
                           joinery=deal(), paint=cellar_paint(), portal_paint=cellar_paint()),
    'B1 Wake room': dict(level='B', x=(0, 585), y=(-650, 0), soot=[(365, -325, CEIL, 70)],
                         wall=limewash(), wall_lower=perished(), floor=flags(), ceiling=underboards(),
                         joinery=deal(), paint=cellar_paint(), portal_paint=cellar_paint()),
    'B2 Workshop': dict(level='B', x=(0, 585), y=(-1300, -650),
                        wall=limewash(tint=(0.76, 0.73, 0.66, 0.45), damp=0.35), wall_lower=perished(), floor=flags(),
                        ceiling=underboards(), joinery=deal(), paint=cellar_paint(), portal_paint=cellar_paint()),
    'B3 Cellar hall': dict(level='B', x=(585, 975), y=(-1300, 0),
                           wall=whitebrick(), wall_lower=tar(),
                           floor=flags(tint=(0.56, 0.54, 0.5, 0.45)), ceiling=underboards(), joinery=deal(),
                           paint=cellar_paint(), portal_paint=cellar_paint()),
    'B5 Laundry': dict(level='B', x=(975, 1560), y=(-1300, -560),
                       wall=limewash(tint=(0.72, 0.72, 0.66, 0.4), damp=0.55), wall_lower=perished(tint=(0.56, 0.54, 0.48, 0.5), damp=0.55),
                       floor=pavers(), ceiling=underboards(), joinery=deal(), paint=cellar_paint(), portal_paint=cellar_paint()),
}

# ---------------------------------------------------------------------------------------------------------------
# Cellar surfaces (MI_Env_Lib_<name>, world-aligned, M_Env_World). Used by apply_cellar.py and import_cellar_meshes.py.
def _s(set_, tile, tint, rough, opt=(0, 0, 1.0, 0.5), extra=(0, 0, 0.2, 0)):
    return (set_, dict(Tile=(tile, tile, 100, 100), Tint=tint, Rough=rough, Opt=opt, Extra=extra))


SURFACES = {
    'CellarBrick': _s('Firebrick', 195, (0.58, 0.43, 0.36, 0.6), (1.0, 0.06, 0, 0.3), (0, 0, 1.0, 0.7), (0, 0.1, 0.3, 0)),   # arches, flue
    'StoneStep': _s('Flags', 260, (0.6, 0.58, 0.54, 0.45), (0.95, 0.06, 0, 0.35), (0, 0, 1.0, 0.6), (0, 0.15, 0.3, 0)),
    'WindowBoards': _s('DarkWood', 100, (0.6, 0.56, 0.52, 0.2), (0.9, 0.1, 0, 0.3), (0, 0, 1.0, 0.6), (0, 0.1, 0.2, 0)),
    'ScreenBoards': _s('Joinery', 90, (0.95, 0.92, 0.85, 0.25), (1.0, 0.12, 0, 0.3), (0, 0, 1.0, 0.6), (0, 0.1, 0.04, 0)),   # lime-washed deal
    'DustSheet': ('Linen', dict(Tile=(35, 35, 100, 100), Tint=(0.6, 0.58, 0.54, 0.0), Rough=(1.0, 0.08, 0, 0.3), Opt=(0, 1, 1.0, 0.5),
                                Extra=(0, 0, 0.2, 0))),                                                  # unbleached cotton, grey with dust
    'OldOak': _s('DarkWood', 90, (0.78, 0.7, 0.64, 0.35), (0.75, 0.08, 0, 0.4), (0, 0, 1.0, 0.6), (0, 0.1, 0.2, 0)),      # dark, dull, handled
    'StairBoards': _s('DarkWood', 110, (0.64, 0.6, 0.56, 0.18), (0.9, 0.12, 0, 0.3), (0, 1, 1.0, 0.6), (0, 0.1, 0.2, 0)),  # soft triplanar: sloped soffit
    'Runner': _s('Jacquard', 40, (0.42, 0.27, 0.23, 0.6), (1.0, 0.08, 0, 0.3), (0, 0, 1.0, 0.6)),                        # worn wool runner
    'CupboardPaint': _s('Paint', 200, (0.15, 0.16, 0.125, 0.45), (0.6, 0.05, 0, 0.35), (0, 0, 0.6, 0.4)),   # old sage gone dark
    'Tar': _s('Paint', 200, (0.07, 0.06, 0.05, 0.5), (0.6, 0.0, 0, 0.4), (0, 0, 0.6, 0.4)),
    'BenchTop': _s('Joinery', 90, (0.8, 0.7, 0.6, 0.6), (0.85, 0.1, 0, 0.35), (0, 0, 1.0, 0.6), (0, 0.15, 0.04, 0)),       # scrubbed, scored deal
    'Tallow': _s('Paint', 30, (0.86, 0.8, 0.62, 0.85), (0.45, 0.14, 0, 0.35), (0, 0, 0.25, 0.15)),
    'Tin': _s('Iron', 60, (0.62, 0.6, 0.57, 0.3), (0.6, 0.0, 1.0, 0.5), (0, 0, 0.8, 0.4), (0.6, 0, 0.08, 0)),
    'Japanned': _s('Iron', 60, (0.12, 0.11, 0.1, 0.2), (0.45, 0.0, 1.0, 0.5), (0, 0, 0.6, 0.4), (0.2, 0, 0.08, 0)),
}
