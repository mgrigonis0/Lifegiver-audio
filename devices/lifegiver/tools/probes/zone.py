import sys, struct, numpy as np
def load(fn):
    b=open(fn,'rb').read(); i=12; fmt=ch=sr=bits=0; data=None
    while i+8<=len(b):
        cid=b[i:i+4]; n=struct.unpack('<I',b[i+4:i+8])[0]; p=b[i+8:i+8+n]
        if cid==b'fmt ': fmt,ch,sr=struct.unpack('<HHI',p[:8]); bits=struct.unpack('<H',p[14:16])[0]; fmt = struct.unpack('<H',p[24:26])[0] if fmt==0xFFFE else fmt
        if cid==b'data': data=p
        i+=8+n+(n&1)
    if fmt==3: x=np.frombuffer(data,'<f4').astype(float)
    elif bits==16: x=np.frombuffer(data,'<i2').astype(float)/32768
    else:
        a=np.frombuffer(data,np.uint8).reshape(-1,3).astype(np.int32); v=a[:,0]|(a[:,1]<<8)|(a[:,2]<<16); x=np.where(v>=2**23,v-2**24,v)/2**23
    return x.reshape(-1,ch).mean(1), sr
def bands(x,sr):
    X=np.abs(np.fft.rfft(x*np.hanning(len(x))))**2; f=np.fft.rfftfreq(len(x),1/sr)
    tot=X[(f>100)&(f<16000)].sum()
    g=lambda a,c: 10*np.log10(X[(f>=a)&(f<c)].sum()/tot+1e-30)
    return g(2500,5500), g(5500,16000), g(100,2500)
a,sr=load(sys.argv[1]); b,_=load(sys.argv[2])
za,ha,la=bands(a,sr); zb,hb,lb=bands(b,sr)
print(f"zone {zb-za:+5.2f}  above {hb-ha:+5.2f}  below {lb-la:+5.2f}")
