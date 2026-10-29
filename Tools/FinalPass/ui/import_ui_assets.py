"""Import the interface's type, surfaces, wordmark and story plates into /Game/FinalPass/UI.

Rerunnable: every asset is reimported over itself from Tools/FinalPass/ui.
  source/fonts/*.ttf          -> /Game/FinalPass/UI/Fonts/FF_Fell, FF_FellItalic, FF_NellHand (inline font faces)
  source/textures/T_*.png     -> /Game/FinalPass/UI/T_*      (HUD surfaces, drawn near 1:1, no mips)
  plates/out/Wordmark.png     -> /Game/FinalPass/UI/T_Wordmark (mips, trilinear: shown at several sizes)
  slides/<Name>.png           -> /Game/FinalPass/UI/Slides/T_<Name> (the story plates)

  UnrealEditor <project>/Tallow/Tallow.uproject -unattended -nosound -run=pythonscript -script=/abs/path/import_ui_assets.py
TALLOW_UI_IMPORT=fonts,textures,wordmark,slides limits the steps (default: all).
"""
import os
from pathlib import Path

import unreal as u

HERE = Path(__file__).resolve().parent
DEST = "/Game/FinalPass/UI"
FONTS = {
    "FF_Fell": "IMFellEnglish-Regular.ttf",
    "FF_FellItalic": "IMFellEnglish-Italic.ttf",
    "FF_NellHand": "ShadowsIntoLightTwo-Regular.ttf",
}
STEPS = set((os.environ.get("TALLOW_UI_IMPORT") or "fonts,textures,wordmark,slides").split(","))


def task(path, dest, name):
    t = u.AssetImportTask()
    t.filename = str(path)
    t.destination_path = dest
    t.destination_name = name
    t.automated = True
    t.replace_existing = True
    t.save = False
    return t


def run_tasks(tasks):
    if tasks:
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)


def settle_texture(tex, mips):
    tex.set_editor_property("lod_group", u.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("never_stream", True)
    tex.set_editor_property("compression_settings", u.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property("address_x", u.TextureAddress.TA_CLAMP)
    tex.set_editor_property("address_y", u.TextureAddress.TA_CLAMP)
    if mips:
        tex.set_editor_property("mip_gen_settings", u.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
        tex.set_editor_property("filter", u.TextureFilter.TF_TRILINEAR)
    else:
        tex.set_editor_property("mip_gen_settings", u.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("filter", u.TextureFilter.TF_BILINEAR)


def import_fonts():
    tasks, names = [], []
    for name, ttf in FONTS.items():
        src = HERE / "source" / "fonts" / ttf
        if not src.exists():
            print("UI_IMPORT_MISSING_SOURCE", src)
            continue
        tasks.append(task(src, DEST + "/Fonts", name))
        names.append(name)
    run_tasks(tasks)
    for name in names:
        face = u.load_asset(f"{DEST}/Fonts/{name}")
        if not isinstance(face, u.FontFace):
            print("UI_IMPORT_FAILED", name, type(face).__name__ if face else None)
            continue
        # Inline: the face is cooked into the asset, so Slate can build composites from it at runtime.
        face.set_editor_property("loading_policy", u.FontLoadingPolicy.INLINE)
        u.EditorAssetLibrary.save_loaded_asset(face, False)
        print("UI_IMPORTED_FONT", name)


def import_textures(folder, dest, prefix, mips, rename=lambda s: s, exact=False):
    folder = Path(folder)
    if not folder.exists():
        print("UI_IMPORT_NO_FOLDER", folder)
        return
    pngs = sorted(p for p in folder.glob("*.png") if (p.stem == prefix if exact else p.stem.startswith(prefix)))
    tasks = [task(p, dest, rename(p.stem)) for p in pngs]
    run_tasks(tasks)
    for p in pngs:
        path = f"{dest}/{rename(p.stem)}"
        tex = u.load_asset(path)
        if not tex:
            print("UI_IMPORT_FAILED", path)
            continue
        settle_texture(tex, mips)
        u.EditorAssetLibrary.save_loaded_asset(tex, False)
        print("UI_IMPORTED_TEXTURE", path, tex.blueprint_get_size_x(), tex.blueprint_get_size_y())


if "fonts" in STEPS:
    import_fonts()
if "textures" in STEPS:
    import_textures(HERE / "source" / "textures", DEST, "T_", False)
if "wordmark" in STEPS:
    import_textures(HERE / "plates" / "out", DEST, "Wordmark", True, lambda s: "T_Wordmark", exact=True)
if "slides" in STEPS:
    import_textures(HERE / "slides", DEST + "/Slides", "", False, lambda s: "T_" + s)
print("UI_IMPORT_DONE", sorted(STEPS))
