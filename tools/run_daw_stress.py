#!/usr/bin/env python3
"""Poll the loaded DAW plug-in and exercise restoration via its private API."""
import json,re,time,shutil,sys,traceback
from pathlib import Path
import ti_control
root=Path(sys.argv[1]);root.mkdir(parents=True,exist_ok=True)
instance=ti_control.discover()
assert len(instance)==1 and instance[0]['connected']
endpoint=instance[0]['socket']
log_path=Path(instance[0]['snapshot_directory'])/'session.log'
schedule=json.loads((root/'schedule.json').read_text())
report={'baseline':instance[0],'cycles':[],'started_wall':time.time(),'state':'waiting_for_first_chord','firmware_flashed':False}
def save(): (root/'run.json').write_text(json.dumps(report,indent=2)+'\n')
def command(name):
 r=ti_control.request(endpoint,name)
 if not r.get('ok') or not r.get('request_id'):raise RuntimeError(str(r))
 deadline=time.monotonic()+15
 request_id=r['request_id']
 while True:
  s=ti_control.request(endpoint,'status')
  if s['completed_request_id']>=request_id:break
  if time.monotonic()>deadline:raise TimeoutError(name)
  time.sleep(.1)
 if 'failed' in s['status'].lower() or 'needs attention' in s['status'].lower():raise RuntimeError(str(s))
 if name=='disconnect' and s['connected']:raise RuntimeError('disconnect remained live')
 if name in ('connect','test-tone') and not s['connected']:raise RuntimeError('not connected')
 return s
# Start timing on a real DAW note, avoiding UI launch latency assumptions.
save();print('Waiting for the first stress chord (MIDI 48, velocity 32)',flush=True)
with log_path.open() as log:
 log.seek(0,2);deadline=time.monotonic()+180
 while True:
  line=log.readline()
  if re.match(r'MIDI 144 48 32 frame=',line):
   first_frame=int(line.split('frame=')[1]);zero=time.monotonic()-2;break
  if time.monotonic()>deadline:raise TimeoutError('No stress chord arrived')
  time.sleep(.02)
report.update(state='running',first_note_host_frame=first_frame,clip_zero_monotonic=zero);save()
next_cycle=0;next_progress=0
try:
 with (root/'telemetry.jsonl').open('w') as stream:
  while True:
   elapsed=time.monotonic()-zero;s=ti_control.request(endpoint,'status');s['elapsed']=elapsed;stream.write(json.dumps(s)+'\n');stream.flush()
   if elapsed>=schedule['duration_seconds']:break
   if next_cycle<len(schedule['reconnect_seconds']) and elapsed>=schedule['reconnect_seconds'][next_cycle]:
    cycle={'index':next_cycle+1,'disconnect_started':elapsed,'before':s};report['cycles'].append(cycle);save()
    off=command('disconnect');cycle['disconnected']=time.monotonic()-zero;cycle['off']=off
    src=Path(off['snapshot_directory']);dst=root/'sessions'/src.name;shutil.copytree(src,dst,dirs_exist_ok=True)
    cycle['restoration']={}
    for kind in ('single','part0','multi'):
     a=(src/f'before-{kind}.syx').read_bytes();b=(src/f'restored-{kind}.syx').read_bytes()
     differences=[i for i,(x,y) in enumerate(zip(a,b)) if x!=y]
     cycle['restoration'][kind]={'exact':a==b,'differences':differences}
     if a!=b:raise RuntimeError('Restoration mismatch: '+kind)
    command('connect');command('test-tone');cycle['ready']=time.monotonic()-zero;next_cycle+=1;save()
    print('Reconnect',next_cycle,'passed; all RAM snapshots exact; elapsed',round(cycle['ready'],1),flush=True)
   if elapsed>=next_progress:
    print('Elapsed',round(elapsed),'seconds',s['lifetime_counters'],flush=True);next_progress+=30
   time.sleep(1)
 report['state']='completed';report['final']=ti_control.request(endpoint,'status');save();print('Eight-minute run complete; stop Live and finalize recording.',flush=True)
except BaseException as error:
 report['state']='failed';report['error']=str(error)
 try:report['cleanup']=command('disconnect')
 except Exception as cleanup:report['cleanup_error']=str(cleanup)
 save();raise
