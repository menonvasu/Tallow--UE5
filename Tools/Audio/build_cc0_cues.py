"""Build TALLOW's interim sound cues from Kenney CC0 packs plus sounds synthesised here (also CC0, ours).
Each cue is designed from what is on screen: a child's light steps on old boards, a 2.4 m cloaked Keeper whose damp
hem drags and whose porcelain mask cracks open, a match, a tallow candle, a ceramic fuse in a brass box.
Placeholder until Ty's own sound design pass; see Tallow/SourceAssets/Audio/CREDITS.md.

  <venv with numpy+scipy>/bin/python Tools/Audio/build_cc0_cues.py
Sources: Tallow/SourceAssets/Audio/CC0/Kenney/*.ogg (decoded with ffmpeg). Output: Tallow/SourceAssets/Audio/CC0/Cues/*.wav
"""
import subprocess, wave
from pathlib import Path
import numpy as np
from scipy import signal

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT/'Tallow/SourceAssets/Audio/CC0/Kenney'
OUT = ROOT/'Tallow/SourceAssets/Audio/CC0/Cues'
SR = 48000
rng = np.random.default_rng(252)

def load(name):
    raw = subprocess.run(['ffmpeg','-v','error','-i',str(SRC/(name+'.ogg')),'-ac','1','-ar',str(SR),'-f','f32le','-'],capture_output=True,check=True).stdout
    return np.frombuffer(raw,np.float32).astype(np.float64)

def sec(t): return int(t*SR)
def silence(t): return np.zeros(sec(t))
def noise(t): return rng.standard_normal(sec(t))
def brown(t):
    x = np.cumsum(rng.standard_normal(sec(t))); return signal.sosfilt(signal.butter(1,20,'hp',fs=SR,output='sos'),x)

def filt(x,f,kind,order=4): return signal.sosfilt(signal.butter(order,f,kind,fs=SR,output='sos'),x)
def pitch(x,semis):
    # Varispeed like tape: lower is also longer, which is what makes heavy things read heavy.
    f = 2**(semis/12); n = max(2,int(len(x)/f)); return np.interp(np.linspace(0,len(x)-1,n),np.arange(len(x)),x)
def env(n,attack,tau,hold=0.):
    t = np.arange(n)/SR; a = np.clip(t/max(attack,1e-4),0,1)
    return a*np.where(t<attack+hold,1.,np.exp(-(t-attack-hold)/tau))
def at(base,x,t,gain=1.):
    i = sec(t); end = i+len(x)
    if end>len(base): base = np.concatenate([base,np.zeros(end-len(base))])
    base[i:end] += gain*x; return base
def fade(x,fin=.005,fout=.03):
    x = x.copy(); a,b = min(sec(fin),len(x)//2),min(sec(fout),len(x)//2)
    if a: x[:a] *= np.linspace(0,1,a)
    if b: x[-b:] *= np.linspace(1,0,b)
    return x
def trim(x,thresh=1e-3):
    idx = np.nonzero(np.abs(x)>thresh*np.abs(x).max())[0]; return x[max(0,idx[0]-sec(.002)):idx[-1]+sec(.02)] if len(idx) else x
def norm(x,db): return x*(10**(db/20)/max(1e-9,np.abs(x).max()))
def loop_seam(x,t=.5):
    # Crossfade the tail into the head so the loop has no click.
    n = sec(t); head,tail = x[:n],x[-n:]; w = np.linspace(0,1,n)
    return np.concatenate([tail*(1-w)+head*w,x[n:-n]])
def write(name,x,db=-3.):
    OUT.mkdir(parents=True,exist_ok=True); y = np.clip(norm(x,db),-1,1)
    with wave.open(str(OUT/(name+'.wav')),'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes((y*32767).astype('<i2').tobytes())
    print(f'{name:22s} {len(y)/SR:5.2f}s')

# --- You: a small child in socks on old boards -------------------------------------------------------------
for i in range(5):
    write(f'P_Step_{i:02d}',fade(filt(filt(pitch(trim(load(f'footstep_wood_{i:03d}')),2),120,'hp'),6500,'lp')),-9)
for i in range(3):
    write(f'P_Creak_{i:02d}',fade(filt(trim(load(f'creak{i+1}')),150,'hp'),.01,.15),-8)

# Match: a dry rasp on the box, the head flaring, then a thin hiss while it burns.
rasp = filt(noise(.24),[1500,7000],'bp')*np.repeat(np.abs(rng.standard_normal(sec(.24)//48+1)),48)[:sec(.24)]*env(sec(.24),.03,.05,.14)
flare = filt(noise(.6),4200,'lp')*env(sec(.6),.008,.12)
whump = filt(noise(.3),380,'lp')*env(sec(.3),.004,.07)
hiss = filt(noise(1.),[2200,8000],'bp')*env(sec(1.),.05,.45)*.12
crackle = np.zeros(sec(1.2))
for t in rng.uniform(.25,1.1,9): crackle = at(crackle,filt(noise(.006),[2000,5000],'bp')*env(sec(.006),.0005,.002),t,rng.uniform(.3,.8))
match = at(at(at(at(rasp,flare,.21,.9),whump,.21,.8),hiss,.4),crackle,0,.5)
write('P_Match_Strike',fade(match,.002,.2),-4)
# The wick taking: a soft "ffh" that settles.
write('P_Candle_Catch',fade(filt(noise(.8),1300,'lp')*env(sec(.8),.05,.22)+filt(noise(.8),[3000,7000],'bp')*env(sec(.8),.08,.3)*.15,.01,.2),-12)
# Going out: a small puff of air and the thin wisp after it.
puff = filt(noise(.25),[180,1400],'bp')*env(sec(.25),.006,.06)
wisp = filt(noise(.7),[3000,6500],'bp')*env(sec(.7),.05,.25)*.18
write('P_Candle_Puff',fade(at(puff,wisp,.08),.002,.2),-10)
for i in range(2):
    write(f'P_Cup_Rustle_{i:02d}',fade(filt(trim(load(f'cloth{i+1}')),5000,'lp'),.01,.08),-14)

# Tools: an iron bar, a ceramic fuse, a brass box, nailed boards.
write('P_Pickup_Crowbar',fade(at(pitch(trim(load('impactMetal_light_000')),-3),trim(load('handleSmallLeather')),.02,.6)),-8)
write('P_Pickup_Fuse',fade(pitch(trim(load('impactPlate_light_000')),2)),-14)
write('P_Fuse_Insert',fade(at(trim(load('metalClick')),pitch(trim(load('impactPlate_light_001')),-2),.06,.7)),-7)
# Nails letting go: stick-slip squeal climbing in pitch, then the board cracking free.
t = np.arange(sec(.65))/SR; f0 = 850+700*t/.65+40*np.sin(2*np.pi*23*t)
stick = np.repeat(rng.uniform(.3,1.,sec(.65)//240+1),240)[:len(t)]
squeal = sum(np.sin(2*np.pi*np.cumsum(f0*k)/SR)/k for k in (1,2,3))*stick*env(len(t),.05,.4,.15)
squeal = filt(squeal+.15*filt(noise(.65),[1000,6000],'bp'),[500,7000],'bp')
pry = at(at(squeal*.45,pitch(trim(load('creak2')),3),.0,.5),trim(load('impactWood_heavy_000')),.62,1.)
write('P_Board_Pry',fade(pry,.005,.2),-4)
write('P_Board_Drop',fade(at(trim(load('impactPlank_medium_000')),pitch(trim(load('impactWood_medium_001')),-3),.09,.6)),-5)
write('P_Breaker_Clunk',fade(at(pitch(trim(load('impactMetal_heavy_000')),-5),pitch(trim(load('metalLatch')),-4),.01,.7),.002,.25),-3)
write('P_Latch',fade(at(trim(load('metalLatch')),pitch(trim(load('creak3')),2)[:sec(.5)],.05,.35)),-6)
write('P_Chair_Free',fade(at(trim(load('impactWood_medium_002')),trim(load('cloth3')),.03,.5)),-6)
for i,(o,c) in enumerate([(1,1),(2,3)]):
    write(f'D_Open_{i:02d}',fade(pitch(trim(load(f'doorOpen_{o}')),-2),.005,.2),-6)
    write(f'D_Close_{i:02d}',fade(pitch(trim(load(f'doorClose_{c}')),-2),.005,.15),-6)

# Power back: old bulbs and wiring humming in the walls (exact 50 Hz cycles so it loops).
t = np.arange(sec(4.))/SR
hum = sum(np.sin(2*np.pi*50*k*t+rng.uniform(0,6.3))/k**1.2 for k in range(1,15))
hum = filt(np.tanh(1.2*hum),1800,'lp')+.015*filt(noise(4.),[1500,4000],'bp')  # warm mains drone, not a harsh buzz
write('H_Power_Hum',loop_seam(np.concatenate([hum,hum[:sec(.5)]]),.5),-20)
for i in range(3):
    write(f'H_Settle_{i:02d}',fade(filt(pitch(trim(load(f'creak{i+1}')),-4),2600,'lp'),.02,.3),-10)

# --- The Keeper: 2.4 m, a damp ragged cloak, long fingers, a porcelain mask -----------------------------------
for i in range(4):
    # More board than body, or it reads as a kick drum: a damped low knock (no ring) under the pitched-down boards.
    knock = filt(noise(.12),160,'lp',2)*env(sec(.12),.002,.025)
    board = pitch(trim(load(f'footstep_wood_{i:03d}')),-5)
    write(f'K_Step_{i:02d}',fade(filt(at(board,knock,.0,.5),70,'hp'),.003,.08),-8)
for i in range(3):
    # The hem dragging after it: cloth slowed right down so it is heavy and wet, not a rustle.
    write(f'K_Drag_{i:02d}',fade(filt(filt(pitch(trim(load(f'cloth{i+1}')),-7),1800,'lp'),90,'hp'),.06,.25),-11)
write('K_Click',fade(pitch(trim(load('impactPlate_light_003')),5)[:sec(.18)],.001,.05),-16)
# A sniff behind the mask: two short nasal pulls.
sniff = silence(.5)
for t0,d in ((0,.11),(.17,.16)):
    sniff = at(sniff,filt(noise(d),[1100,4800],'bp')*env(sec(d),.035,d/3),t0)
write('K_Sniff',fade(filt(sniff,3200,'lp'),.002,.08),-12)
# The reveal: the porcelain splits, something wet tears behind it. The roar that follows is the existing capture cue.
tear = filt(noise(.35),[300,1500],'bp')*np.repeat(np.abs(rng.standard_normal(sec(.35)//96+1)),96)[:sec(.35)]*env(sec(.35),.02,.12,.08)
crack = at(at(pitch(trim(load('impactPlate_heavy_000')),-3),pitch(trim(load('impactGlass_light_001')),-6),.04,.6),tear,.1,.7)
write('K_Mask_Crack',fade(crack,.001,.2),-2)

# The capture sting under the crack, roar and scream: the floor dropping out, a blast, a cluster that will not resolve.
t = np.arange(sec(1.8))/SR
sub = np.tanh(1.6*np.sin(2*np.pi*np.cumsum(90*np.exp(-t/.5)+32)/SR))*np.exp(-t/.7)
blast = filt(noise(1.8),[60,9000],'bp')*env(sec(1.8),.003,.16)
cluster = sum(signal.sawtooth(2*np.pi*f*t+rng.uniform(0,6.3)) for f in (196,207.6,220,233.1,293.7,311.1))/6
cluster = filt(cluster,[150,4000],'bp')*np.clip(t/.12,0,1)*np.exp(-np.clip(t-.12,0,None)/.9)
write('K_Capture_Sting',fade(sub*.9+blast*.6+cluster*.55,.001,.3),-1)

# --- Outside at the end: cold wind in the garden ------------------------------------------------------------
w = filt(brown(16.5),[120,900],'bp')
gust = np.interp(np.arange(len(w)),np.linspace(0,len(w),24),rng.uniform(.25,1.,24)); gust = np.convolve(gust,np.ones(sec(.8))/sec(.8),'same')
whistle = filt(noise(16.5),[560,640],'bp',2)*np.clip(gust-.6,0,1)*3
write('A_Outro_Wind',loop_seam(w*gust+.25*whistle,.5),-10)
