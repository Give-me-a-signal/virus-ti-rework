#!/usr/bin/env python3
"""Exercise bank recall in a running DAW plug-in; RAM part 1 only."""
import json, time, shutil
from pathlib import Path
from ti_control import discover, request
OUT=Path(__file__).resolve().parents[1]/"analysis/daw-validation/presets"
OUT.mkdir(parents=True,exist_ok=True)
instances=[s for s in discover() if s["connected"]]
assert len(instances)==1
SOCKET=instances[0]["socket"]
def wait(command):
 r=request(SOCKET,command)
 if not r["ok"]:raise RuntimeError(r)
 ident=r["request_id"];deadline=time.monotonic()+15
 while r["completed_request_id"]<ident:
  if time.monotonic()>deadline:raise TimeoutError(command)
  time.sleep(.05);r=request(SOCKET,"status")
 if not r["last_command_succeeded"]:raise RuntimeError(r)
 return r
banks=request(SOCKET,"banks")["banks"]
results=[];all_presets={}
for bank in banks:
 presets=request(SOCKET,{"command":"presets","bank":bank["id"]})["presets"]
 assert len(presets)==bank["count"];all_presets[str(bank["id"])]=presets
 for index in sorted({0,len(presets)-1}) if presets else []:
  state=wait({"command":"load","bank":bank["id"],"preset":index})
  folder=OUT/"readbacks"/f'{bank["id"]:02d}-{index:03d}';folder.mkdir(parents=True,exist_ok=True)
  for name in ("preset-request.syx","preset-readback.syx"):
   shutil.copy2(Path(state["snapshot_directory"])/name,folder/name)
  a=(folder/"preset-request.syx").read_bytes();b=(folder/"preset-readback.syx").read_bytes()
  assert b[7:9]==bytes([0,0]);assert state["preset_name"]==presets[index]["name"]
  row={"loaded_wall":time.time(),"bank":bank["id"],"preset":index,"name":state["preset_name"],"source_bytes":len(a),"source_version":a[9],"returned_version":b[9],"differences":[[i,a[i],b[i]] for i in range(min(len(a),len(b))-2) if a[i]!=b[i]],"state":state}
  results.append(row);(OUT/"bank-recall-results.json").write_text(json.dumps(results,indent=2)+"\n")
 print(bank["id"],bank["name"],len(presets),"OK",flush=True)
(OUT/"all-presets.json").write_text(json.dumps(all_presets,indent=2)+"\n")
print("Verified",len(results),"RAM recalls across",sum(bool(b["count"]) for b in banks),"populated banks",flush=True)
