#!/usr/bin/env python3
from pathlib import Path
import xml.etree.ElementTree as E,hashlib,json
root=Path(__file__).resolve().parents[2]
p=root/'analysis/package-expanded/Core_components.pkg/Payload/Library/Application Support/Access Music/Virus TI/Common/Virus Control.bundle/Contents/Resources/remotedevice.xml'
t=E.parse(p).getroot();params={s.get('name')+'/'+a.get('name'):a for s in t.find('ParameterSets') for a in s if a.tag=='Parameter'}
lines=['// Generated from original remotedevice.xml; SHA256 '+hashlib.sha256(p.read_bytes()).hexdigest(), 'struct Mapping { const char* name; unsigned index; int minimum, maximum; bool ti; };','static constexpr Mapping mappings[]{']
for mid,ti in [('VirusC_Singledump',False),('VirusTI_Singledump',True)]:
 m=next(x for x in t.find('MIDIMessages') if x.get('id')==mid)
 for x in m:
  if x.get('type')!='parameter':continue
  assert set(x.attrib)=={'type','index','content'}
  n=x.get('content');a=params.get(n)
  if a is None:continue
  lines.append(' {%s,%s,%s,%s,%s},'%(json.dumps(n),x.get('index'),a.get('minValue'),a.get('maxValue'),str(ti).lower()))
lines+=['};','static constexpr const char* category_names[]{']
v=next(x for x in t.find('Valuelists') if x.get('name')=='Val Name Categories')
c={int(x.get('value',x.get('index'))):x.text for x in v}
for i in range(23):lines.append(' '+json.dumps(c[i])+',')
lines+=['};']
Path(__file__).with_name('browser_mapping.inc').write_text('\n'.join(lines)+'\n')
print('Generated mapping from two exact XML single-dump definitions')
