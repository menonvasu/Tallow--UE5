from pathlib import Path
import numpy as np
from PIL import Image,ImageFilter
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01/Surfaces';O.mkdir(exist_ok=True);n=2048;r=np.random.default_rng(252);y,x=np.mgrid[:n,:n]
noise=np.array(Image.fromarray(r.integers(0,255,(64,64),dtype=np.uint8)).resize((n,n),Image.Resampling.BICUBIC).filter(ImageFilter.GaussianBlur(12)),dtype=float)/255-.5
fine=r.normal(0,1.4,(n,n));base=np.array([151,146,126])[None,None,:]+noise[:,:,None]*8+fine[:,:,None];Image.fromarray(np.uint8(np.clip(base,0,255))).save(O/'T_House_Limewash_BaseColor.png')
# Faded original narrow vertical botanical rhythm; deliberately restrained, non-photoreal.
base=np.array([107,117,108])[None,None,:]+noise[:,:,None]*7+fine[:,:,None];stripe=np.minimum(x%256,256-x%256)<3;base[stripe]=[85,98,89]
for cy in range(0,n,256):
 for cx in range(128,n,256):
  dx=(x-cx)/25;dy=(y-cy)/48;leaf=(dx*dx+dy*dy<1)&(dx*dx+dy*dy>.68);base[leaf]=[86,99,88]
Image.fromarray(np.uint8(np.clip(base,0,255))).save(O/'T_House_Wallpaper_BaseColor.png')
