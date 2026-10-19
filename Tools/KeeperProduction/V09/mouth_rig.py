"""V09 maw morph maths shared by fit and validation (pure numpy, no bpy).
Open shape = the designed maw (HOOD_OPEN=1). Closed shape (Basis) = the same maw squeezed shut between the hood halves and drawn back
into the head, so it sits wholly inside the closed hood/mask. CHOMP = the oral disc and throat clench toward the axis, lips purse,
every tooth pivots ~28 deg on its root toward the throat; the collar edge glued behind the hood's cut edges stays put."""
import numpy as np
CX=-.04
ANCHOR=np.array([CX,.020,2.215]);SQUEEZE=np.array([.22,.55,.60]);SHIFT=np.array([0,.060,.030])  # squeeze across the seam/depth/height, then back and up behind the mask folds
def closed(P):return ANCHOR+(np.asarray(P)-ANCHOR)*SQUEEZE+SHIFT
def _radial(P,axis_pt):
 r=np.asarray(P)-np.asarray(axis_pt);r[:,1]=0;return r
def flesh_chomp(P,axis_pt,w,part,depth):
 """Delta for flesh vertices (open space). part: 0 sack,1 collar,2 lips/disc,3 throat."""
 r=_radial(P,axis_pt);k=np.where(part==3,.20+.10*np.clip((depth-.06)/.10,0,1),.30)*w  # deeper throat constricts harder (swallow)
 d=-r*k[:,None]
 d[:,1]+=np.where(part<=2,-.012,-.004)*w  # lips purse forward, throat pulls slightly
 return d
def _rot(axis,ang):
 axis=axis/np.maximum(np.linalg.norm(axis,axis=1,keepdims=True),1e-9);c,s=np.cos(ang),np.sin(ang)
 x,y,z=axis[:,0],axis[:,1],axis[:,2];C=1-c
 return np.stack([np.stack([c+x*x*C,x*y*C-z*s,x*z*C+y*s],1),np.stack([y*x*C+z*s,c+y*y*C,y*z*C-x*s],1),np.stack([z*x*C-y*s,z*y*C+x*s,c+z*z*C],1)],1)
def teeth_chomp(P,base,tdir,ring,axis_pt):
 """Delta for tooth vertices: root rides the flesh contraction, tooth pivots toward the axis/throat."""
 P=np.asarray(P);base=np.asarray(base);tdir=np.asarray(tdir)
 k=np.select([ring==2,ring==0],[.20,.28],.30)
 db=-_radial(base,axis_pt)*k[:,None];db[:,1]+=np.where(ring==2,-.004,-.012)
 inward=-_radial(base,axis_pt);inward/=np.maximum(np.linalg.norm(inward,axis=1,keepdims=True),1e-9);inward[:,1]+=.6  # toward axis and down the throat
 ax=np.cross(tdir,inward);ang=np.radians(np.where(ring==2,22.,28.))
 R=_rot(ax,ang);v=P-base
 return base+db+np.einsum('nij,nj->ni',R,v)-P
