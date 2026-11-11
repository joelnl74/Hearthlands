# Prototype of the terrain blend shader (docs/Concept.md, "Terrain blending"): every art pixel weighs the tile ids
# around it by distance, adds clustered noise and takes the strongest type. numpy + Pillow; writes
# scripts/prototypes/out/terrain_blend.png (panels: raw tiles, 2x2 bilinear, 3x3 smooth, 3x3 smooth + noise) and
# terrain_art.png: the same with the atlas (variants per tile, wet rows from moisture, shallow water and foam by the shore;
# a raised block of grass whose tiles blend only with tiles of their own level, as across a cliff in the game).
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
gau=lambda dx,dy: np.exp(-(dx*dx+dy*dy)/(2*0.75**2))  # tile_blend.frag: Sigma 0.75, 5 x 5 (Reach 2)
raw=pal[np.repeat(np.repeat(tiles,P,0),P,1)]
a=render(bil,0.0,1); b=render(gau,0.0,2); c=render(gau,0.18,2)
gap=np.full((H*P,6,3),30,np.uint8)
out=os.path.join(os.path.dirname(os.path.abspath(__file__)),"out","terrain_blend.png")
os.makedirs(os.path.dirname(out),exist_ok=True)
img=np.concatenate([raw,gap,a,gap,b,gap,c],1)
Image.fromarray(img).resize((img.shape[1]*2,img.shape[0]*2),Image.NEAREST).save(out)
print("wrote", out)

# With the atlas, like tile_blend.frag: a variant per tile, the wet row (WetRow + id) for moist land (moisture blended
# like the types) and for water near land, foam where land is close. Moisture: 255 next to water, 42 less per tile (WaterRules, MoistureReach 6).
A=np.asarray(Image.open(os.path.join(os.path.dirname(out),"..","..","..","Project","Assets","Textures","TerrainAtlas.png")).convert("RGB"))
T=32; WetRow=16; cols=A.shape[1]//T
def hash_cell(x,y,seed):
    v=np.sin(x*12.9898+y*78.233+seed*37.7)*43758.5453
    return v-np.floor(v)
steps=np.where(tiles==3,0,99)
for _ in range(6):
    p=np.pad(steps,1,constant_values=99)
    steps=np.minimum(steps,np.minimum.reduce([p[:-2,1:-1],p[2:,1:-1],p[1:-1,:-2],p[1:-1,2:]])+1)
moist=np.where(steps<=6,np.clip(255-42*steps,0,255),0)/255.0
levels=np.zeros((H,W),int); levels[2:7,9:15]=2   # HeightMap levels: tile_blend.frag blends only within a pixel's own level
tiles_art=tiles.copy(); tiles_art[2:7,9:15]=2
GX,GY=12,8
gy,gx=np.mgrid[0:H,0:W]   # groundwater: a patch peaking at 1 (GroundwaterRules), shown above ClueFrom 0.78
ground=np.clip(1.0-np.hypot(gx-GX,gy-GY)/14.0,0,1)
def render_art(noise_amt,seconds=0.0,clarity=0.25):
    clues=np.round(np.clip((ground-0.78)/0.22,0,1)*clarity*63)/63  # GroundwaterRules::Clue, as the texture's B carries it
    seconds=np.floor(seconds*6)/6  # WaterSteps
    py,px=np.mgrid[0:H*T,0:W*T]
    u=(px+0.5)/T; v=(py+0.5)/T
    tx=np.floor(u).astype(int); ty=np.floor(v).astype(int)
    wts=np.zeros((4,H*T,W*T)); land=np.zeros((H*T,W*T)); wetsum=np.zeros((H*T,W*T)); cluesum=np.zeros((H*T,W*T))
    base=levels[np.clip(ty,0,H-1),np.clip(tx,0,W-1)]
    for oy in range(-2,3):
        for ox in range(-2,3):
            nx=np.clip(tx+ox,0,W-1); ny=np.clip(ty+oy,0,H-1)
            k=gau(u-(tx+ox+0.5),v-(ty+oy+0.5))*(levels[ny,nx]==base); t=tiles_art[ny,nx]
            for i in range(4): wts[i]+=np.where(t==i,k,0)
            land+=np.where(t!=3,k,0); wetsum+=moist[ny,nx]*k; cluesum+=clues[ny,nx]*k
    share=land/wts.sum(0); wetness=wetsum/wts.sum(0); clue=cluesum/wts.sum(0)
    wts/=wts.sum(0,keepdims=True)
    n=(hashn(px,py,0)-0.5)*noise_amt
    for i in range(4): wts[i]+= n*(1 if i!=3 else 0.5)+prio[i]*1e-4 + (hashn(px,py,i+1)-0.5)*noise_amt*0.5
    win=np.argmax(wts,0); water=win==3
    tcx=np.clip(tx,0,W-1); tcy=np.clip(ty,0,H-1)
    col=(hash_cell(tx,ty,3.0)*cols).astype(int)%cols
    wet=np.where(water,share>0.04*(0.3+1.4*hash_cell(px//2,py//2,7.0)),wetness>0.62+(hash_cell(px//2,py//2,4.0)-0.5)*0.16)
    row=win+np.where(wet,WetRow,0)
    dx,dy=int(np.floor(1.5*seconds)),int(np.floor(0.75*seconds))  # Drift (the shader's y runs up)
    ix=np.where(water,(px+dx)%T,px%T); iy=np.where(water,(py-dy)%T,py%T)
    img=A[row*T+iy,col*T+ix].astype(float)
    wave=np.sin(px*0.21-py*0.37-seconds*2.2); across=np.sin(-px*0.13-py*0.29-seconds*1.3)
    glint=hash_cell(px,py,10.0+np.mod(np.floor(seconds*2),64))
    rip=water&(wave>0.9)&(across>0.2)&(glint>0.45)
    img=np.where(rip[...,None],img*0.3+np.array([141,180,230])*0.7,img)
    swell=np.sin(seconds*1.4+u*0.7-v*0.45)*0.18
    edge=(share-0.36)/(0.5-0.36)+swell+(hash_cell(px,py,6.0+np.mod(np.floor(seconds*3),64))-0.5)*0.6
    foam=water&(share>0.36)&(share<0.5)
    img=np.where((foam&(edge>0.75))[...,None],[233,239,246],np.where((foam&(edge>0.35))[...,None],img*0.2+np.array([141,180,230])*0.8,img))
    # Groundwater clues: cool wet specks in clusters and a rare glint, as plain as the clue (ClueClarity); a cool tint around.
    specks=clue*(0.25+0.5*hash_cell(px//2,py//2,11.0)); pick=hash_cell(px,py,12.0)
    damp=(~water)&(clue>0); c=clue[...,None]
    tint=img*(1-c*0.5)+img*np.array([0.85,0.9,1.0])*c*0.5
    img=np.where((damp&(pick<clue*0.04))[...,None],[140,175,205],np.where((damp&(pick<specks))[...,None],
                 img*0.4+np.array([70,88,105])*0.6,np.where(damp[...,None],tint,img)))
    return np.clip(img,0,255).astype(np.uint8)
art=render_art(0.18)
Image.fromarray(art).resize((art.shape[1]*2,art.shape[0]*2),Image.NEAREST).save(os.path.join(os.path.dirname(out),"terrain_art.png"))
print("wrote", os.path.join(os.path.dirname(out),"terrain_art.png"))
# The water's motion: 12 frames (2 seconds) of a crop around a lake, as terrain_water.gif.
frames=[Image.fromarray(render_art(0.18,f/6.0)[5*T:13*T,8*T:22*T]).resize((14*T*2,8*T*2),Image.NEAREST) for f in range(12)]
frames[0].save(os.path.join(os.path.dirname(out),"terrain_water.gif"),save_all=True,append_images=frames[1:],duration=166,loop=0)
print("wrote", os.path.join(os.path.dirname(out),"terrain_water.gif"))
# Groundwater clues at the start (clarity 0.25) and fully revealed (1.0), side by side, as terrain_groundwater.png.
crop=lambda c: render_art(0.18,0.0,c)[(GY-6)*T:(GY+6)*T,(GX-8)*T:(GX+8)*T]
pair=np.concatenate([crop(0.25),np.full((12*T,4,3),255,np.uint8),crop(1.0)],axis=1)
Image.fromarray(pair).resize((pair.shape[1]*2,pair.shape[0]*2),Image.NEAREST).save(os.path.join(os.path.dirname(out),"terrain_groundwater.png"))
print("wrote", os.path.join(os.path.dirname(out),"terrain_groundwater.png"))
