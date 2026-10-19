"""CC0 Poly Haven intake -> SM_Furnish_<Name>.fbx: joined, real scale, budget-decimated, author UV kept, pivot per use, UCX box, DX normals.
blender -b --factory-startup --threads 4 --python-exit-code 1 --python furnish_v03_prepare.py [-- Name ...]"""
import bpy,json,sys,shutil,math
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseFurnishV03';E=O/'Export';T=E/'Textures';L=O/'Work/Assets';[p.mkdir(parents=True,exist_ok=True) for p in (E,T,L)]
credits=json.loads((O/'credits.json').read_text());only=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
# pivot: floor=base centre | wall=back-centre-bottom (front -Y) | ceiling=top centre. budget=max triangles. col: box|none. yaw: fix so front faces -Y. scale: uniform correction.
S=dict(GothicBed=dict(budget=12000),SpinningWheel=dict(budget=12000),ChinaHutch=dict(budget=14000),GothicHighChair=dict(budget=9000),TeaTable=dict(budget=9000),IronBed=dict(budget=14000),
 GrandfatherClock=dict(budget=12000),PaintingLandscape=dict(pivot='wall',col='none'),PaintingTown=dict(pivot='wall',col='none',budget=4000),OvalPortrait=dict(pivot='wall',col='none'),
 Chandelier=dict(pivot='ceiling',col='none',budget=8000),BareBulb=dict(pivot='ceiling',col='none',budget=3000),HangingShelf=dict(pivot='wall'),ChessSet=dict(budget=9000,col='none'),BookSet=dict(budget=8000,col='none'),
 TeaSet=dict(budget=8000,col='none'),Candelabra=dict(budget=6000,col='none'),Lantern=dict(budget=5000,col='none'),Trunk=dict(budget=8000),PedestalTable=dict(scale=.75),OilLamp=dict(scale=.7),LaundryBasket=dict(scale=1.7),Mirror=dict(pivot='wall',col='none',budget=4000,scale=1.4),Cake=dict(budget=3000,col='none'),Bust=dict(budget=5000,col='none'))
SMALL={'MantelClock','AlarmClock','Candlestick','OilLamp','Goblets','WineBottles','BenchVice','HandPlane','Handsaw','HandDrill','Magnifier','PipeWrench','OilCan','Mousetrap','ToyElephant','Spectacles','PocketWatch','Apple','Bowl','Blowtorch','RustedTin','OilTin','PorcelainHorse','BlueVase','WashJug','CopperPot','CopperPan','Crock','MilkChurn','Bucket','Broom','WateringCan'}
rows=json.loads((E/'manifest.json').read_text()) if (E/'manifest.json').exists() else {}
def tris(o):o.data.calc_loop_triangles();return len(o.data.loop_triangles)
for name,rec in credits.items():
 if only and name not in only:continue
 spec=dict(pivot='floor',budget=5000 if rec['resolution']=='1k' else 12000,col='none' if name in SMALL else 'box',yaw=0,scale=1);spec.update(S.get(name,{}))
 bpy.ops.wm.read_factory_settings(use_empty=True);d=O/'Raw'/rec['polyhaven_id'];bpy.ops.import_scene.gltf(filepath=str(next(d.glob('*.gltf'))))
 ms=[o for o in bpy.context.scene.objects if o.type=='MESH']
 for o in bpy.context.scene.objects:o.select_set(o in ms)
 bpy.context.view_layer.objects.active=ms[0];bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM');bpy.ops.object.join();o=bpy.context.object;o.name=o.data.name='SM_Furnish_'+name
 for x in [x for x in bpy.context.scene.objects if x!=o]:bpy.data.objects.remove(x)
 o.shape_key_clear() if o.data.shape_keys else None;o.rotation_euler.z+=math.radians(spec['yaw']);o.scale*=spec['scale'];bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);raw=tris(o)
 if raw>spec['budget']:m=o.modifiers.new('budget','DECIMATE');m.ratio=spec['budget']/raw;bpy.ops.object.modifier_apply(modifier=m.name)
 vs=[v.co for v in o.data.vertices];lo=Vector([min(v[i] for v in vs) for i in range(3)]);hi=Vector([max(v[i] for v in vs) for i in range(3)])
 piv=Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z)) if spec['pivot']=='floor' else Vector(((lo.x+hi.x)/2,hi.y,lo.z)) if spec['pivot']=='wall' else Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,hi.z))
 for v in o.data.vertices:v.co-=piv
 lo-=piv;hi-=piv;mats=[]
 for i,mat in enumerate(o.data.materials):
  mat.name='M_Furnish_'+name+('_%d'%i if len(o.data.materials)>1 else '');tex={}
  for n in mat.node_tree.nodes:
   if n.type!='TEX_IMAGE' or not n.image:continue
   src=Path(bpy.path.abspath(n.image.filepath));low=src.name.lower();role='BaseColor' if '_diff' in low else 'ARM' if '_arm' in low else 'Normal' if 'nor_gl' in low else 'Opacity' if 'alpha' in low else 'Roughness' if 'rough' in low else 'Metallic' if 'metal' in low else 'AO' if '_ao' in low else None
   if not role or role in tex:continue
   if role=='Normal':
    dx=d/'textures_dx'/src.name.replace('nor_gl','nor_dx')
    if dx.exists():src=dx
   dst=T/('T_Furnish_'+name+('_%d'%i if len(o.data.materials)>1 else '')+'_'+role+src.suffix);shutil.copyfile(src,dst);tex[role]=dst.name
   if role!='Normal':n.image.filepath=str(dst)
  mats.append(dict(material=mat.name,textures=tex,normal_convention='DirectX (Poly Haven nor_dx)',alpha=mat.blend_method if hasattr(mat,'blend_method') else None))
 ucx=[]
 if spec['col']=='box':
  me=bpy.data.meshes.new('UCX_'+o.name+'_00');cs=[(x,y,z) for x in (lo.x,hi.x) for y in (lo.y,hi.y) for z in (lo.z,hi.z)];me.from_pydata(cs,[],[(0,1,3,2),(4,6,7,5),(0,4,5,1),(2,3,7,6),(0,2,6,4),(1,5,7,3)]);c=bpy.data.objects.new(me.name,me);bpy.context.scene.collection.objects.link(c);ucx.append(c.name)
 for x in bpy.context.scene.objects:x.select_set(True)
 fbx=E/(o.name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(fbx),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',bake_anim=False,path_mode='RELATIVE',mesh_smooth_type='FACE')
 bpy.ops.wm.save_as_mainfile(filepath=str(L/(name+'.blend')))
 rows[name]=dict(mesh=o.name,fbx=fbx.name,source='polyhaven:'+rec['polyhaven_id'],license=rec['license'],authors=rec['authors'],triangles=tris(o),raw_triangles=raw,dims_cm=[round((hi[i]-lo[i])*100,1) for i in range(3)],
  bounds_cm=dict(min=[round(lo.x*100,1),round(-hi.y*100,1),round(lo.z*100,1)],max=[round(hi.x*100,1),round(-lo.y*100,1),round(hi.z*100,1)]),pivot=spec['pivot'],front='+Y Unreal at yaw 0 (Blender -Y)',collision=ucx or 'none (small/wall prop; NoCollision)',materials=mats,uv='Poly Haven author UV retained (CC0)')
 (E/'manifest.json').write_text(json.dumps(rows,indent=1));print('PREPARED',name,raw,'->',rows[name]['triangles'],rows[name]['dims_cm'],flush=True)
(E/'manifest.json').write_text(json.dumps(rows,indent=1));print('RESULT',len(rows),'assets in manifest')
