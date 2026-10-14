#!/usr/bin/env python3
"""Summarize an actual Live USB-return recording for the five-pass comparison."""
import json,sys,wave,hashlib
from pathlib import Path
import numpy as np
path=Path(sys.argv[1]);root=path.parent
with wave.open(str(path),"rb") as f:
 rate=f.getframerate();channels=f.getnchannels();width=f.getsampwidth();n=f.getnframes();raw=f.readframes(n)
assert width==3 and channels==2
b=np.frombuffer(raw,dtype=np.uint8).reshape(-1,3).astype(np.int32);v=b[:,0]|(b[:,1]<<8)|(b[:,2]<<16);v=np.where(v&0x800000,v-0x1000000,v).reshape(-1,channels)/8388608.
mono=v.mean(axis=1);activity=np.max(np.abs(v),axis=1);first=int(np.flatnonzero(activity>0.0005)[0]);onset=first/rate
names=[x["name"] for x in json.loads((root/"audition.json").read_text())["loads"]]
passes=[]
for k,name in enumerate(names):
 start=onset+12*k+1.8;end=onset+12*k+7.4;x=mono[int(start*rate):int(end*rate)]
 centroids=[]
 for pos in range(0,len(x)-2048,1024):
  frame=x[pos:pos+2048]
  if np.sqrt(np.mean(frame**2))<.0001:continue
  spec=np.abs(np.fft.rfft(frame*np.hanning(len(frame))))
  centroids.append(float(np.sum(spec*np.fft.rfftfreq(len(frame),1/rate))/max(np.sum(spec),1e-12)))
 passes.append({"pass":k+1,"name":name,"window_seconds":[start,end],"rms":float(np.sqrt(np.mean(x*x))),"peak":float(np.max(np.abs(x))),"median_spectral_centroid_hz":float(np.median(centroids)) if centroids else None})
result={"path":str(path.resolve()),"sha256":hashlib.sha256(path.read_bytes()).hexdigest(),"sample_rate":rate,"channels":channels,"seconds":len(v)/rate,"first_audio_seconds":onset,"peak":float(activity.max()),"clipped_samples":int(np.sum(np.abs(v)>=.9999)),"passes":passes,"last_three_seconds_rms":float(np.sqrt(np.mean(v[-rate*3:]**2))),"last_three_seconds_peak":float(np.max(np.abs(v[-rate*3:])))}
assert all(p["rms"]>.0001 and p["median_spectral_centroid_hz"] for p in passes),result
(root/"audio-analysis.json").write_text(json.dumps(result,indent=2)+"\n");print(json.dumps(result,indent=2))
