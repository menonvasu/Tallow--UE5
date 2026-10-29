"""The upstairs rooms (U1 toy room, U2 collection, U3 upper landing, U3b back landing, U4 bedroom, U5 nursery) for the
house library. Merged by env/env_library.py (ROOMS).

A chandler's family floor in 1894, kept as it was. The landing carries on the stair hall's distempered plaster and dark
dado from below. The children's toy room has Morris's Daisy, the paper of a nursery, faded where the sun came in;
the parents' bedroom Willow Bough gone to a dusty rose. The infant nursery is distempered a pale institutional green,
too clean for the rest of the house. The collection is the one room somebody still keeps: oxblood distemper like a
museum gallery, black dado, boards waxed dark, a ceiling smoked by the candles that burn there.

Shared runs (the rule of two texture sets per generated instance): every upstairs wall is Paper (Daisy, Willow Bough)
or Limewash, every lower wall Paint, every floor Floor boards (the slab under them is the ground floor's Plaster
ceiling), every ceiling Plaster. Window joinery and cornice paint, which shares its mesh with both floors below, is one
zone for the whole upper floor ('U Upper floor paint') so that mesh stays inside twelve rooms.
Units: cm; texture sizes are real sizes times 1.3. World z: upper floor 390, ceiling 720. The library helpers place
soot from a ground-floor height, so lift() moves it up a storey.
"""
from env_library import paper, boards, plaster_ceiling, plaster_walls, joinery, paint, _s

STOREY = 390.0


def lift(finish, dz=STOREY):
    """The same finish with its soot line (Grime z, a world height) raised by one storey."""
    texset, params = finish
    g = params.get('Grime')
    if g and 0 < g[2] < 9000:
        params = dict(params, Grime=(g[0], g[1], g[2] + dz, g[3]))
    return texset, params


def waxed(tint=(1.6, 1.35, 1.2, 0.65)):
    """Boards stained and waxed dark, kept polished: lower roughness, no worn border."""
    texset, params = boards(tint, border=40)
    return texset, dict(params, Rough=(0.75, -0.14, 0, 0.5), Mark=(0, 40, 0.15, 0.2))


# Candle soot over the flames that burn here (centre x, y, z, radius): the sconces and the landing candle.
SOOT = {'U1': [(4, -548, 640, 45)], 'U2': [(570, -1040, 640, 45), (20, -820, 520, 30)], 'U3': [(960, -190, 650, 45), (780, -8, 560, 40)],
        'U4': [(1550, -350, 640, 45)], 'U5': [(1550, -970, 645, 45)]}

ROOMS = {
    'U1 Toy room': dict(level='U', x=(0, 585), y=(-650, 0), soot=SOOT['U1'],
                        wall=lift(paper('Daisy', tint=(0.8, 0.74, 0.61, 0.8), soot=0.22, spots=0.5)),
                        wall_lower=lift(paint((0.34, 0.37, 0.29, 0.75))), floor=boards((2.45, 2.2, 2.05, 0.6)),
                        ceiling=plaster_ceiling((0.92, 0.84, 0.7, 0.7), soot=0.12), joinery=joinery((1.05, 0.95, 0.88, 0.85))),
    'U2 Collection': dict(level='U', x=(0, 585), y=(-1300, -650), soot=SOOT['U2'],
                          wall=lift(plaster_walls((0.52, 0.13, 0.1, 0.9), dirt=0.15, soot=0.35)),
                          wall_lower=lift(paint((0.09, 0.07, 0.06, 0.6))), floor=waxed(),
                          ceiling=plaster_ceiling((0.62, 0.53, 0.42, 0.6), soot=0.4, spots=0.8), joinery=joinery((0.62, 0.52, 0.48, 0.9))),
    'U3 Upper landing': dict(level='U', x=(585, 975), y=(-1020, 0), soot=SOOT['U3'],
                             wall=lift(plaster_walls((0.5, 0.47, 0.4, 0.7))), wall_lower=lift(paint((0.2, 0.13, 0.09, 0.8))),
                             floor=boards((2.2, 1.95, 1.82, 0.65)), ceiling=plaster_ceiling(soot=0.25), joinery=joinery()),
    'U3b Back landing': dict(level='U', x=(585, 975), y=(-1300, -1020),
                             wall=lift(plaster_walls((0.46, 0.43, 0.36, 0.7), dirt=0.45, soot=0.35)),
                             wall_lower=lift(paint((0.18, 0.12, 0.08, 0.8))), floor=boards((2.05, 1.82, 1.7, 0.6)),
                             ceiling=plaster_ceiling(soot=0.3, spots=0.8), joinery=joinery((0.95, 0.85, 0.8, 0.9))),
    'U4 Bedroom': dict(level='U', x=(975, 1560), y=(-650, 0), soot=SOOT['U4'],
                       wall=lift(paper('WillowBough', tint=(0.74, 0.57, 0.52, 0.7), soot=0.3, spots=0.55)),
                       wall_lower=lift(paint((0.3, 0.17, 0.14, 0.75))), floor=boards((2.25, 1.98, 1.85, 0.6)),
                       ceiling=plaster_ceiling(), joinery=joinery()),
    'U5 Nursery': dict(level='U', x=(975, 1560), y=(-1300, -650), soot=SOOT['U5'],
                       wall=lift(plaster_walls((0.78, 0.86, 0.76, 0.5), dirt=0.22, soot=0.15)),
                       wall_lower=lift(paint((0.44, 0.48, 0.41, 0.6))), floor=boards((2.6, 2.35, 2.2, 0.5)),
                       ceiling=plaster_ceiling((0.95, 0.9, 0.8, 0.7), soot=0.1), joinery=joinery((1.1, 1.0, 0.94, 0.85))),
    # One paint zone for the whole floor (window joinery, cornice): see the note above.
    'U Upper floor paint': dict(level='U', x=(0, 1560), y=(-1300, 0), paint=lift(paint((0.58, 0.52, 0.42, 0.8)))),
}

# Props upstairs whose own materials read wrong by candlelight (apply_upper.py OVERRIDE / build_env_materials.py):
# the collection's cabinets want a dark, waxed, not glassy wood (their curved backs turned the library's polish into
# plastic), and the dust sheet an unbleached grey linen (the library linen goes yellow next to a candle).
SURFACES = {
    'UpperCabinet': _s('DarkWood', 60, (0.62, 0.5, 0.44, 0.75), (0.8, 0.1, 0, 0.4), (0, 1, 1.0, 0.5), (0, 0, 0.15, 0)),
    'DustSheet': _s('Linen', 35, (0.5, 0.49, 0.46, 0.25), (1.0, 0.08, 0, 0.3), (0, 1, 1.0, 0.5)),
}
# The white-painted nursery chairs at the toy room tea table read as lit plastic under a candle: old, greyed paint.
# The generated high chair's own colour map is a hot orange: dulled to old varnished pine.
PROP_TUNE = {'NurseryChair': dict(Tint=(0.66, 0.62, 0.56, 0.75)),
             'HighChair': dict(Tint=(0.6, 0.5, 0.44, 0.45), Rough=(1.0, 0.08, 0, 0.8))}
