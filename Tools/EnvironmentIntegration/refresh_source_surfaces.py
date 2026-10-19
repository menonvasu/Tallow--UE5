import bpy
from pathlib import Path
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';bpy.ops.wm.open_mainfile(filepath=str(O/'HouseArchitecture.blend'))
for old,key in [('LimePlaster','Limewash'),('FadedWallpaper','Wallpaper')]:
 m=bpy.data.materials.get('M_HeroRoomV02_'+old);m.use_nodes=True;m.node_tree.nodes.clear();out=m.node_tree.nodes.new('ShaderNodeOutputMaterial');p=m.node_tree.nodes.new('ShaderNodeBsdfPrincipled');p.inputs['Roughness'].default_value=.89;tex=m.node_tree.nodes.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(O/'Surfaces'/('T_House_'+key+'_BaseColor.png')));tex.image.pack();m.node_tree.links.new(tex.outputs['Color'],p.inputs['Base Color']);m.node_tree.links.new(p.outputs['BSDF'],out.inputs['Surface'])
bpy.ops.wm.save_as_mainfile(filepath=str(O/'HouseArchitecture.blend'))
