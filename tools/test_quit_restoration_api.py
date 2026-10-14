#!/usr/bin/env python3
"""Exercise quit restoration on real TI RAM using only a child API test host.
Never sends firmware or stored-bank writes. Does not terminate any DAW process.
"""
import argparse,json,os,signal,subprocess,time
from pathlib import Path
from ti_control import discover,request
from audit_ti_midi_capture import audit
from test_multipart_api import decode,sound

def wait_state(endpoint,command):
    result=request(endpoint,command);assert result['ok'],result
    target=result.get('request_id',0);end=time.monotonic()+150
    while result['busy'] or result.get('state_recall_pending') or result['completed_request_id']<target:
        assert time.monotonic()<end,result
        time.sleep(.2);result=request(endpoint,'status')
    assert result['last_command_succeeded'],result
    return result

def verify(session,helper=True):
    folder=session/'restoration-helper' if helper else session
    outcomes=[]
    for part in range(16):
        before=(session/f'before-part{part}.syx').read_bytes()
        after=(folder/f'restored-part{part}.syx').read_bytes()
        assert sound(before)==sound(after),(part,before[9],after[9])
        outcomes.append({'part':part+1,'sound_equal':True})
    assert (session/'before-single.syx').read_bytes()==(folder/'restored-single.syx').read_bytes()
    before=(session/'before-multi.syx').read_bytes();after=(folder/'restored-multi.syx').read_bytes()
    assert all(a==b for i,(a,b) in enumerate(zip(before,after)) if i not in (10,265))
    return {'parts':outcomes,'single_exact':True,'multi_sound_and_routing_equal':True,
            'traffic':[audit(p) for p in folder.glob('private-out*.bin')]}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host',type=Path,required=True)
    parser.add_argument('--state',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--scenario',choices=['graceful','abrupt','explicit'],required=True)
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    report={'scenario':args.scenario};proc=None;session=None
    def persist(): (out/'result.json').write_text(json.dumps(report,indent=2))
    try:
        log=open(out/'host.log','w')
        proc=subprocess.Popen([str(args.host.resolve()),'/dev/null'],stdout=log,stderr=log)
        report['host_pid']=proc.pid;persist()
        end=time.monotonic()+15;endpoint=None
        while time.monotonic()<end:
            endpoint=next((x['socket'] for x in discover() if x['pid']==proc.pid),None)
            if endpoint:break
            assert proc.poll() is None,'Host failed';time.sleep(.2)
        assert endpoint,'No API endpoint';report['socket']=endpoint
        print(args.scenario+': recalling all 16 test sounds',flush=True)
        state=wait_state(endpoint,{'command':'state-load','path':str(args.state.resolve())})
        assert state['connected'],state
        session=Path(state['snapshot_directory']);report['session']=str(session)
        guard=json.loads((session/'restoration-status.json').read_text());assert guard['state']=='armed',guard
        report['connected']=state;persist()
        # Hardware readback proves that all sixteen test buffers are loaded.
        wait_state(endpoint,{'command':'state-save','path':str(out/'loaded.vtrstate')})
        actual,_,_=decode(out/'loaded.vtrstate');wanted,_,_=decode(args.state)
        assert all(sound(a)==sound(b) for a,b in zip(actual,wanted))
        report['all_16_test_sounds_verified']=True;persist()
        for part,note in [(1,48),(8,55),(16,60)]:wait_state(endpoint,{'command':'note-on','part':part,'note':note,'velocity':30})
        time.sleep(.3)
        start=time.monotonic();print(args.scenario+': closing with notes held',flush=True)
        if args.scenario=='explicit':
            report['disconnect']=wait_state(endpoint,'disconnect')
            proc.terminate()
        elif args.scenario=='abrupt':proc.kill()
        else:proc.terminate()
        proc.wait(timeout=20);report['host_exit_seconds']=time.monotonic()-start;report['exit_code']=proc.returncode;persist()
        end=time.monotonic()+100
        while time.monotonic()<end:
            status=json.loads((session/'restoration-status.json').read_text())
            if status['state'] in ('restored','completed-by-plugin','failed'):break
            time.sleep(.25)
        report['restoration']=status;report['restoration_seconds']=time.monotonic()-start;persist()
        assert status['state']==('completed-by-plugin' if args.scenario=='explicit' else 'restored'),status
        report['verification']=verify(session,args.scenario!='explicit');report['passed']=True;persist()
        print(json.dumps({k:report[k] for k in ('scenario','session','host_exit_seconds','restoration_seconds','passed')}),flush=True)
    except BaseException as error:
        report['error']=repr(error);persist()
        if proc and proc.poll() is None:
            proc.terminate()
            try:proc.wait(timeout=20)
            except subprocess.TimeoutExpired:proc.kill();proc.wait()
        raise
if __name__=='__main__':main()
