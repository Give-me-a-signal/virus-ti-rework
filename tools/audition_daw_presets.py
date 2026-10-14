#!/usr/bin/env python3
"""Compare five bank presets with the same 12-second Live MIDI clip."""
import json,time,re
from pathlib import Path
from ti_control import discover,request
OUT=Path(__file__).resolve().parents[1]/"analysis/daw-validation/presets"
s=[x for x in discover() if x["connected"]];assert len(s)==1
socket=s[0]["socket"];logpath=Path(s[0]["snapshot_directory"])/"session.log"
report={"baseline":s[0],"loads":[],"state":"preparing"}
def save():(OUT/"audition.json").write_text(json.dumps(report,indent=2)+"\n")
def load(bank,preset):
 before=time.time();r=request(socket,{"command":"load","bank":bank,"preset":preset});assert r["ok"],r
 ident=r["request_id"];deadline=time.monotonic()+15
 while r["completed_request_id"]<ident:
  assert time.monotonic()<deadline;time.sleep(.05);r=request(socket,"status")
 assert r["connected"] and r["last_command_succeeded"],r
 report["loads"].append({"bank":bank,"preset":preset,"started_wall":before,"completed_wall":time.time(),"name":r["preset_name"],"state":r});save()
load(8,0)
with logpath.open() as f:
 f.seek(0,2);report["state"]="waiting_for_note";save();print("Ready: start recording and launch the 12-second clip",flush=True)
 deadline=time.monotonic()+180
 while True:
  line=f.readline()
  if re.match(r"MIDI 144 60 45 frame=",line):zero=time.monotonic()-1;report["first_note_wall"]=time.time();report["first_note_frame"]=int(line.split("frame=")[1]);break
  assert time.monotonic()<deadline;time.sleep(.01)
report["state"]="playing";save()
for when,bank in [(9,4),(21,5),(33,7),(45,8)]:
 while time.monotonic()-zero<when:time.sleep(.05)
 load(bank,0);print("Loaded",report["loads"][-1]["name"],"at",round(time.monotonic()-zero,2),flush=True)
while time.monotonic()-zero<58:time.sleep(.05)
report["state"]="notes_complete";report["final"]=request(socket,"status");save();print("Stop the TI clip now; record its tail",flush=True)
