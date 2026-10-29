"""Shared names for the parlour environment pass (paths, the room's zone box)."""
LEVEL = '/Game/Presentation/Playable/Lvl_Tallow_Playable'
DEST = '/Game/FinalPass/Env'
TEX = DEST + '/Textures'
MAT = DEST + '/Materials'
MESH = DEST + '/Meshes'
LEGACY = '/Game/Presentation/ArtDirection09/Materials'
V04 = '/Game/Presentation/Environment/HouseV04Preview'
# G1 parlour (cm): wall centre lines x 0..585, y -715..0, floor 0, ceiling 360. Surfaces are tested 2 cm out along
# their normal, so each face of a shared wall belongs to the room it faces.
ZONE_MIN = (0.0, -715.0, -5.0)
ZONE_MAX = (585.0, 0.0, 362.0)
ZONE_ALL = ((-1e5, -1e5, -1e5), (1e5, 1e5, 1e5))   # parlour-only meshes: no legacy side to keep
FOLDER = 'Env/Parlour'
TAG = 'Env.Parlour'
