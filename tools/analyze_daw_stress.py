#!/usr/bin/env python3
"""Screen a scheduled DAW capture; candidates are not a perceptual click verdict."""
import argparse,json,wave,hashlib,shutil
from pathlib import Path
import numpy as np
p=argparse.ArgumentParser();p.add_argument('directory',type=Path);p.add_argument('recording',type=Path);args=p.parse_args();root=args.directory
schedule=json.loads((root/'schedule.json').read_text());run=json.loads((root/'run.json').read_text())
with wave.open(str(args.recording)) as f:
 sr,ch,width,n=f.getframerate(),f.getnchannels(),f.getsampwidth(),f.getnframes();assert width==3 and ch==2
 mono=np.empty(n,dtype=np.float32);peaks=np.zeros(ch);squares=np.zeros(ch);offset=0
 while offset<n:
  raw=f.readframes(min(sr*10,n-offset));b=np.frombuffer(raw,np.uint8).reshape(-1,ch,3).astype(np.int32);v=b[:,:,0]|(b[:,:,1]<<8)|(b[:,:,2]<<16);v=((v^0x800000)-0x800000).astype(np.float32)/8388608
  mono[offset:offset+len(v)]=v[:,0];offset+=len(v);peaks=np.maximum(peaks,np.max(abs(v),axis=0));squares+=np.sum(v.astype(float)**2,axis=0)
hop=round(sr*.01)
envelope=np.array([np.sqrt(np.mean(mono[i:i+hop]**2)) for i in range(0,min(n,sr*150)-hop,hop)])
active=envelope>.0002;edges=np.diff(np.r_[False,active,False].astype(np.int8));starts=np.flatnonzero(edges==1);ends=np.flatnonzero(edges==-1)
long_spans=[(a,b) for a,b in zip(starts,ends) if (b-a)*.01>6]
assert long_spans,'No sustained stress chord found'
zero=long_spans[0][0]*.01-2 # First eight-second chord, not shorter pre-roll notes.
def samples(a,b):return mono[max(0,round((zero+a)*sr)):min(n,round((zero+b)*sr))]
cycles=run['cycles'];intentional=[]
for c in cycles:
 base=int(c['disconnect_started']//60)*60
 next_note=min(base+s['start'] for s in schedule['segments'] if s['kind']=='notes' and base+s['start']>c['ready'])
 intentional.append((c['disconnect_started']-.2,next_note+.25))
def excluded(a,b):return any(a<end and b>start for start,end in intentional)
rests=[];dropouts=[];clicks=[];plateaus=[]
for loop in range(8):
 base=60*loop
 for seg in schedule['segments']:
  a,b=base+seg['start'],base+seg['end']
  if zero+b>n/sr:continue
  if seg['kind']=='silence':
   v=samples(a+.5,b-.1)
   if len(v):rests.append({'start':a+.5,'end':b-.1,'peak':float(abs(v).max()),'rms':float(np.sqrt(np.mean(v*v)))})
  if seg['kind']=='position_plateau':
   v=samples(a+.3,b-.2);spectrum=abs(np.fft.rfft(v*np.hanning(len(v))))**2;hz=np.fft.rfftfreq(len(v),1/sr);mask=(hz>50)&(hz<15000)
   plateaus.append({'loop':loop,'position':seg['position'],'centroid_hz':float(np.sum(hz[mask]*spectrum[mask])/max(np.sum(spectrum[mask]),1e-30)),'rms':float(np.sqrt(np.mean(v*v)))})
  if seg['kind']=='notes' and b-a>1:
   start,end=a+.25,b-.15
   if excluded(start,end):continue
   v=samples(start,end);silent=abs(v)<1e-7;edges=np.diff(np.r_[False,silent,False].astype(np.int8));ss=np.flatnonzero(edges==1);ee=np.flatnonzero(edges==-1)
   for x,y in zip(ss,ee):
    if y-x>=round(sr*.002):dropouts.append({'start':start+x/sr,'seconds':(y-x)/sr})
   # Isolated sample discontinuities, excluding intended note attacks/releases.
   hop=round(.02*sr)
   for j in range(0,len(v)-hop,hop):
    block=v[j:j+hop];d2=np.diff(block,n=2);peak=float(abs(d2).max());rms=float(np.sqrt(np.mean(d2*d2)))
    if peak>.015 and peak>12*max(rms,1e-9):clicks.append({'time':start+j/sr,'second_difference_peak':peak,'crest':peak/max(rms,1e-9)})
transitions=[]
for c in cycles:
 center=c['disconnect_started'];v=samples(center-.4,center+.4);origin=round((zero+center-.4)*sr)
 quiet=abs(v)<1e-7;edges=np.diff(np.r_[False,quiet,False].astype(np.int8));ss=np.flatnonzero(edges==1);ee=np.flatnonzero(edges==-1)
 candidates=[(x,y) for x,y in zip(ss,ee) if y-x>sr*.02 and x>0]
 item={'cycle':c['index'],'scheduled_disconnect':center,'max_adjacent_sample_step_near_disconnect':float(abs(np.diff(v)).max()) if len(v)>1 else None}
 if candidates:
  x,y=min(candidates,key=lambda pair:abs(pair[0]/sr-.4));edge=origin+x
  item.update(silence_edge_seconds=edge/sr-zero,step_into_silence=float(abs(mono[edge]-mono[edge-1])),preceding_20ms_rms=float(np.sqrt(np.mean(mono[max(0,edge-sr//50):edge]**2))))
 transitions.append(item)
report={'recording':str(args.recording),'seconds':n/sr,'sample_rate':sr,'channels':ch,'peak':peaks.tolist(),'rms':np.sqrt(squares/n).tolist(),'estimated_clip_zero_seconds':zero,'alignment_uncertainty_seconds':.03,'intentional_reconnect_windows':intentional,'silent_gap_checks':rests,'stuck_note_candidates':[r for r in rests if r['peak']>.0002],'unexpected_zero_runs_in_sustained_notes':dropouts,'isolated_discontinuity_candidates':clicks,'wavetable_plateaus':plateaus,'disconnect_transitions':transitions,'last_half_second_peak':float(abs(mono[-sr//2:]).max()),'scheduled_final_silence_peak':float(abs(samples(476.5,479.9)).max()),'analysis_scope_seconds':[zero,zero+480],'screening_limitations':'Waveform screening cannot rule out all audible clicks. Note attacks, releases and deliberate reconnect windows are excluded from steady-tone dropout/click checks; inspect reported candidates and transition audio separately. Host/device timestamp continuity is not exposed by this API.'}
(root/'audio-analysis.json').write_text(json.dumps(report,indent=2)+'\n')
shutil.copy2(args.recording,root/'Live-stress-return.wav')
print(json.dumps({k:report[k] for k in ['seconds','peak','rms','estimated_clip_zero_seconds','stuck_note_candidates','unexpected_zero_runs_in_sustained_notes','isolated_discontinuity_candidates','scheduled_final_silence_peak','disconnect_transitions']},indent=2))
