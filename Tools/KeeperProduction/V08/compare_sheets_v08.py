"""Stack before (V07) over after (V08) sheets per game clip: Review/compare-<Clip>.png. Plain python3 + PIL."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
R=Path(__file__).resolve().parents[3]/'Tallow/SourceAssets/Keeper/V08Animation/Review';f=ImageFont.load_default(size=22);n=0
for b in sorted(R.glob('before-*.png')):
    if b.name.endswith('-arcs.png'):continue
    a=R/b.name.replace('before-','after-')
    if not a.exists():continue
    A=Image.open(b).convert('RGB');B=Image.open(a).convert('RGB');W=max(A.width,B.width);o=Image.new('RGB',(W,A.height+B.height+60),(12,12,12));d=ImageDraw.Draw(o)
    d.text((8,4),'BEFORE (V07)',fill=(255,120,120),font=f);o.paste(A,(0,30));d.text((8,A.height+32),'AFTER (V08)',fill=(120,255,140),font=f);o.paste(B,(0,A.height+60))
    o.save(R/b.name.replace('before-','compare-'));n+=1
print('COMPARE',n)
