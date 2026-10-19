"""Self-authored tiling pigment/grain maps, not generated-source UV reuse."""
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFilter
O=Path(__file__).resolve().parents[2]/'Tallow/SourceAssets/Production/HeroRoomV02';O.mkdir(exist_ok=True);N=2048;r=np.random.default_rng(252)
y,x=np.mgrid[0:1:complex(N),0:1:complex(N)]
def save(name,c,rough):
 if name not in ['BlackenedTimber','AshTimber'] or not (O/('T_'+name+'_BaseColor.png')).exists():Image.fromarray(np.uint8(np.clip(c,0,255))).save(O/('T_'+name+'_BaseColor.png'))
 Image.fromarray(np.uint8(np.clip(rough,0,255))).save(O/('T_'+name+'_Roughness.png'))
# Grain runs along V. Curving fibres and knots avoid evenly spaced straight bands.
w=x+.007*np.sin(y*15+x*10)+.004*np.sin(y*39+x*24)
grain=np.sin(w*490+3*np.sin(w*75))*3+np.sin(w*1700+y*8)*1.4
for kx,ky in [(.23,.35),(.73,.79)]:
 rad=np.sqrt(((x-kx)*2.8)**2+((y-ky)*.38)**2);grain+=np.sin(rad*360)*np.exp(-rad*22)*8
grain+=r.normal(0,1.2,(N,N));wash=np.asarray(Image.fromarray(np.uint8(r.random((N,N))*255)).filter(ImageFilter.GaussianBlur(20)),dtype=float)-127
for name,col in [('AshTimber',[65,54,43]),('BlackenedTimber',[33,37,34]),('WornEdge',[68,60,47])]:save(name,np.array(col)[None,None,:]+grain[:,:,None]*np.array([1,.86,.65])+wash[:,:,None]*.2,np.full((N,N),194)+grain*.7)
noise=r.normal(0,1.6,(N,N));broad=9*np.sin(x*8+y*3)+5*np.sin(y*14-x*4)
patch=np.asarray(Image.fromarray(np.uint8(r.random((12,12))*255)).resize((N,N),Image.Resampling.BICUBIC).filter(ImageFilter.GaussianBlur(20)),dtype=float)-127
plaster=np.array([115,112,101])[None,None,:]+(noise+broad+wash*.7+patch*.14)[:,:,None]
pi=Image.fromarray(np.uint8(np.clip(plaster,0,255)));pd=ImageDraw.Draw(pi)
for sx,sy in [(40,140),(1830,500),(400,1900)]:
 pts=[(sx,sy)]
 for j in range(11):pts.append((pts[-1][0]+int(r.integers(-20,25)),pts[-1][1]+int(r.integers(6,23))))
 pd.line(pts,fill=(76,75,67),width=2)
save('LimePlaster',np.asarray(pi),np.full((N,N),222)+noise)
im=Image.new('RGB',(N,N),(106,108,92));d=ImageDraw.Draw(im)
for row in range(-1,9):
 for col in range(-1,9):
  cx=col*256+(row%2)*128;cy=row*256
  d.arc((cx-42,cy-100,cx+42,cy+100),80,275,fill=(65,75,66),width=5)
  for s in [-1,1]:
   d.ellipse((cx+s*38-14,cy-45,cx+s*38+14,cy+8),fill=(68,79,68));d.ellipse((cx+s*22-9,cy+30,cx+s*22+9,cy+65),fill=(73,79,64))
  d.ellipse((cx-7,cy-86,cx+7,cy-72),fill=(93,69,55))
a=np.asarray(im,dtype=float)+(noise+wash*.6+patch*.13)[:,:,None];save('FadedWallpaper',a,np.full((N,N),210)+noise)
print('Authored 5 materials, 10 maps at 2k')
