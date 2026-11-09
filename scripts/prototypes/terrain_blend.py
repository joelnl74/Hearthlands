# Prototype of the terrain blend shader (docs/Concept.md, "Terrain blending"): every art pixel weighs the tile ids
# around it by distance, adds clustered noise and takes the strongest type. numpy + Pillow; writes
# scripts/prototypes/out/terrain_blend.png (panels: raw tiles, 2x2 bilinear, 3x3 smooth, 3x3 smooth + noise).
#   py scripts/prototypes/terrain_blend.py
import os
import numpy as np
from PIL import Image
rng=np.random.default_rng(7)
W,H,P=28,18,12   # tiles, art pixels per tile
# value noise -> tile ids (0 sand,1 dirt,2 grass,3 water), like the reference image
def vnoise(w,h,s,seed):
    r=np.random.default_rng(seed).random((h//s+2,w//s+2))
    y,x=np.mgrid[0:h,0:w]/s; x0=x.astype(int); y0=y.astype(int); fx=x-x0; fy=y-y0
    fx=fx*fx*(3-2*fx); fy=fy*fy*(3-2*fy)
    a=r[y0,x0]*(1-fx)+r[y0,x0+1]*fx; b=r[y0+1,x0]*(1-fx)+r[y0+1,x0+1]*fx
    return a*(1-fy)+b*fy
h=0.65*vnoise(W,H,6,1)+0.35*vnoise(W,H,3,2)
tiles=np.select([h<0.35,h<0.45,h<0.55],[3,0,1],2).astype(np.uint8)
tiles[8,5]=3; tiles[9,6]=3; tiles[10,7]=3   # a diagonal 1-tile stream (the hard case)
tiles[4,20]=1                                # a lone tile
pal=np.array([[222,200,130],[120,85,55],[80,140,60],[50,90,160]],np.uint8)
prio=np.array([1,2,3,0])  # tie-break: grass>dirt>sand>water (only used on exact ties)
def hashn(x,y,seed):  # per art-pixel hash noise, clustered by 2x2 art pixels
    v=np.sin((x//2)*12.9898+(y//2)*78.233+seed*37.7)*43758.5453
    return v-np.floor(v)
def render(kernel,noise_amt,radius):
    out=np.zeros((H*P,W*P,3),np.uint8)
    py,px=np.mgrid[0:H*P,0:W*P]
    u=(px+0.5)/P; v=(py+0.5)/P          # position in tile units (art-pixel snapped)
    tx=np.floor(u).astype(int); ty=np.floor(v).astype(int)
    wts=np.zeros((4,H*P,W*P))
    for oy in range(-radius,radius+1):
        for ox in range(-radius,radius+1):
            nx=np.clip(tx+ox,0,W-1); ny=np.clip(ty+oy,0,H-1)
            dx=u-(tx+ox+0.5); dy=v-(ty+oy+0.5)
            k=kernel(dx,dy); t=tiles[ny,nx]
            for i in range(4): wts[i]+=np.where(t==i,k,0)
    wts/=wts.sum(0,keepdims=True)+1e-9
    n=(hashn(px,py,0)-0.5)*noise_amt
    for i in range(4): wts[i]+= n*(1 if i!=3 else 0.5)+prio[i]*1e-4 + (hashn(px,py,i+1)-0.5)*noise_amt*0.5
    return pal[np.argmax(wts,0)]
bil=lambda dx,dy: np.clip(1-np.abs(dx),0,1)*np.clip(1-np.abs(dy),0,1)
gau=lambda dx,dy: np.exp(-(dx*dx+dy*dy)/(2*0.45**2))
raw=pal[np.repeat(np.repeat(tiles,P,0),P,1)]
a=render(bil,0.0,1); b=render(gau,0.0,1); c=render(gau,0.18,1)
gap=np.full((H*P,6,3),30,np.uint8)
out=os.path.join(os.path.dirname(os.path.abspath(__file__)),"out","terrain_blend.png")
os.makedirs(os.path.dirname(out),exist_ok=True)
img=np.concatenate([raw,gap,a,gap,b,gap,c],1)
Image.fromarray(img).resize((img.shape[1]*2,img.shape[0]*2),Image.NEAREST).save(out)
print("wrote", out)
