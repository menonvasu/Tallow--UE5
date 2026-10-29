"""Import the DualSense glyphs (build with pad_extra.py) into /Game/Presentation/UI/Engraved beside the key cap.

Rerunnable: every texture is reimported over itself from Tools/FinalPass/icons/Raster.
Marks get mips (they are shown well below their master size, at 720p a third of it) with
trilinear filtering so fine cuts average instead of shimmering. Grounds wrap for tiling.

  UnrealEditor <project>/Tallow/Tallow.uproject -unattended -nosound -run=pythonscript -script=/abs/path/import_pad.py
"""
from pathlib import Path

import unreal as u

HERE = Path(__file__).resolve().parent
RASTER = HERE / "Raster"
DEST = "/Game/Presentation/UI/Engraved"
GROUNDS = {"Soot", "Sheet"}
CARDS = {"Card", "Key"}


def settle(tex, name):
    tex.set_editor_property("lod_group", u.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("never_stream", True)
    tex.set_editor_property("compression_settings", u.TextureCompressionSettings.TC_EDITOR_ICON)
    if name in GROUNDS:
        tex.set_editor_property("mip_gen_settings", u.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
        tex.set_editor_property("address_x", u.TextureAddress.TA_WRAP)
        tex.set_editor_property("address_y", u.TextureAddress.TA_WRAP)
        tex.set_editor_property("filter", u.TextureFilter.TF_TRILINEAR)
    elif name in CARDS:
        # Nine-sliced: the edge must not bleed across mip levels, and it is drawn near 1:1.
        tex.set_editor_property("mip_gen_settings", u.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("address_x", u.TextureAddress.TA_CLAMP)
        tex.set_editor_property("address_y", u.TextureAddress.TA_CLAMP)
        tex.set_editor_property("filter", u.TextureFilter.TF_BILINEAR)
    else:
        tex.set_editor_property("mip_gen_settings", u.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
        tex.set_editor_property("address_x", u.TextureAddress.TA_CLAMP)
        tex.set_editor_property("address_y", u.TextureAddress.TA_CLAMP)
        tex.set_editor_property("filter", u.TextureFilter.TF_TRILINEAR)


def run():
    tasks = []
    names = []
    for png in sorted(RASTER.glob("T_*.png")):
        t = u.AssetImportTask()
        t.filename = str(png)
        t.destination_path = DEST
        t.destination_name = png.stem
        t.automated = True
        t.replace_existing = True
        t.save = False
        tasks.append(t)
        names.append(png.stem)
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for stem in names:
        path = f"{DEST}/{stem}"
        tex = u.load_asset(path)
        if not tex:
            print("ENGRAVED_IMPORT_MISSING", path)
            continue
        settle(tex, stem[2:])
        u.EditorAssetLibrary.save_loaded_asset(tex, False)
        print("ENGRAVED_IMPORTED", path, tex.blueprint_get_size_x(), tex.blueprint_get_size_y())
    print("ENGRAVED_IMPORT_DONE", len(names))


run()
