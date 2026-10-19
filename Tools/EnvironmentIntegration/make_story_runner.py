from pathlib import Path
import numpy as np
from PIL import Image
p=Path(__file__).resolve().parents[2]/'Tallow/SourceAssets/Production/HouseStoryV02';n=2048;y,x=np.mgrid[:n,:n];u=x/(n-1);v=y/(n-1);rng=np.random.default_rng(252)
weave=np.sin(x*np.pi)*1.5+np.sin(y*2.1)*2+rng.normal(0,2,(n,n));c=np.empty((n,n,3));c[:]=[100,59,45];c+=weave[:,:,None];edge=np.minimum(u,1-u);border=(edge<.11)|(v<.06)|(v>.94);c[border]=[48,61,56]+weave[border,None];line=((edge>.075)&(edge<.082))|((v>.035)&(v<.041))|((v>.959)&(v<.965));c[line]=[141,116,75]+weave[line,None]
# Sparse empty diamonds echo frames; worn central weave breaks the exact repeat.
for cy in [.24,.5,.76]:
 d=np.abs(u-.5)/.25+np.abs(v-cy)/.1;mask=(d>.94)&(d<1.04);c[mask]=[140,111,72]+weave[mask,None]
wear=np.exp(-((u-.52)/.21)**2)*np.sin(v*9)**2;c+=wear[:,:,None]*8;Image.fromarray(np.uint8(np.clip(c,0,255))).save(p/'T_Story_Runner.png')
