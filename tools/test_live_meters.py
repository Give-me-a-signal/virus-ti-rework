#!/usr/bin/env python3
"""Validate live MIDI activity and USB meters through one explicit hardware API instance."""
import argparse,json,time,sys,struct
from pathlib import Path
from ti_control import request
from test_hardware_matrix import audio_stats,FRAME_BYTES

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--buses',type=int,choices=(2,3),default=3);p.add_argument('--socket',required=True);p.add_argument('--audio',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--parts',type=int,default=16);p.add_argument('--noise-floor',type=float,default=.0003);p.add_argument('--check-volume-recall',action='store_true')
a=p.parse_args();assert 1<=a.parts<=16
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);assert not (out/'report.json').exists()
r={'success':False,'parts':[],'steps':[],'buses_tested':a.buses};initial=None;held=None

def persist(): (out/'report.json').write_text(json.dumps(r,indent=2)+'\n')
def run(command):
 s=request(a.socket,command);assert s['ok'],s;rid=s.get('request_id',0);deadline=time.monotonic()+180
 while s['busy'] or s.get('state_recall_pending') or s['completed_request_id']<rid:
  assert time.monotonic()<deadline,s;time.sleep(.03);s=request(a.socket,'status')
 assert s['last_command_succeeded'] and s['backend']=='hardware',s
 r['steps'].append({'command':command,'status':s['status']});persist();return s
try:
 baseline=run('status');assert baseline['connected'];r['baseline']=baseline
 initial=out/'initial.vtrstate';run({'command':'state-save','path':str(initial)})
 for part in range(1,a.parts+1):
  run({'command':'test-tone','part':part});run({'command':'route','part':part,'bus':(part-1)%a.buses+1})
 time.sleep(2)
 if a.check_volume_recall:
  loud=out/'probe-volume.vtrstate';run({'command':'state-save','path':str(loud)})
  raw=bytearray(loud.read_bytes());count=struct.unpack_from('<I',raw,8)[0];found=False
  for i in range(count):
   at=12+i*12
   if struct.unpack_from('<I',raw,at)[0]==268435627:struct.pack_into('<d',raw,at+4,0.);found=True
  assert found;muted=out/'probe-muted.vtrstate';muted.write_bytes(raw);run({'command':'state-load','path':str(muted)})
  held=(1,60);run({'command':'note-on','part':1,'note':60,'velocity':40});time.sleep(.3);silent=run('status')
  assert silent['usb_peak_meters'][0]<a.noise_floor,silent
  run({'command':'note-off','part':1,'note':60});held=None
  run({'command':'state-load','path':str(loud)});time.sleep(2);r['zero_cc7_project_recall_mutes_hardware']=True
 r['steady_start']=run('status')
 for part in range(1,a.parts+1):
  pre=run('status');assert max(pre['part_midi_activity'])==0
  begin=a.audio.stat().st_size//FRAME_BYTES*FRAME_BYTES
  held=(part,48+part);run({'command':'note-on','part':part,'note':held[1],'velocity':40})
  time.sleep(.15);on=run('status');assert on['part_meter_source']=='midi-note-on-velocity'
  assert on['part_midi_activity'][part-1]>.03,on
  assert all(v==0 for i,v in enumerate(on['part_midi_activity']) if i!=part-1),on
  bus=(part-1)%a.buses;assert on['usb_peak_meters'][bus]>.0005,on
  assert all(v<a.noise_floor for i,v in enumerate(on['usb_peak_meters']) if i!=bus),on
  time.sleep(.2);run({'command':'note-off','part':part,'note':held[1]});held=None
  time.sleep(2.2);off=run('status');assert max(off['part_midi_activity'])==0,off
  assert max(off['usb_peak_meters'])<a.noise_floor,off
  end=a.audio.stat().st_size//FRAME_BYTES*FRAME_BYTES;stats=audio_stats(a.audio,begin,end)
  assert max(stats['peak'][bus*2:bus*2+2])>.0005 and not any(stats['clipped_samples']),stats
  r['parts'].append({'part':part,'note_on':on,'after_release':off,'audio':stats,'start_byte':begin,'end_byte':end});persist();print('meters part',part,'PASS',flush=True)
 r['steady_end']=run('status')
 for key in ('capture_errors','fifo_overruns','fifo_underruns'):assert r['steady_end']['lifetime_counters'][key]==r['steady_start']['lifetime_counters'][key],key
 r['success']=True
except Exception as e:r['error']=str(e);print('FAIL',str(e),file=sys.stderr,flush=True)
finally:
 try:
  if held:run({'command':'note-off','part':held[0],'note':held[1]})
  if initial and initial.exists():run({'command':'state-load','path':str(initial)})
  r['disconnected']=run('disconnect');assert not r['disconnected']['connected'];assert max(r['disconnected']['part_midi_activity'])==0 and max(r['disconnected']['usb_peak_meters'])==0
 except Exception as e:r['cleanup_error']=str(e);r['success']=False
 persist()
print('PASS' if r['success'] else 'FAIL',out/'report.json',flush=True)
sys.exit(0 if r['success'] else 1)
