"""Fail closed if hand-transcribed short template encoders diverge from source.

This checks original packaged XML, not the derived parameter JSON catalog.
It does not establish that the binary selects these definitions at runtime.
"""
from pathlib import Path
import hashlib
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[3]
source = root / ('analysis/package-expanded/Core_components.pkg/Payload/Library/'
                 'Application Support/Access Music/Virus TI/Common/'
                 'Virus Control.bundle/Contents/Resources/remotedevice.xml')
digest = hashlib.sha256(source.read_bytes()).hexdigest()
assert digest == '817c444db3a3b089bf09b82fd0391be734057afd24ca34ca272491ccb9152944'
xml = ET.parse(source).getroot()
messages = {message.attrib['id']: message for message in xml.findall('.//MIDIMessage')}
for key, first in [('cc', 'controller'), ('pp', 'polypressure')]:
    expected = [{'type': 'structure', 'index': str(i), 'content': content}
                for i, content in enumerate([first, 'controllerid', 'value'])]
    assert [b.attrib for b in messages[key].findall('byte')] == expected
for bank, command, suffix in [('C',110,'Single-3'), ('D',111,'Single-4'),
                              ('A',112,'Single-1'), ('B',113,'Single-2'),
                              ('E',114,'Multi'), ('F',115,'Global'), ('G',116,'Intern')]:
    key = f'Bank {bank} ({command:02X}) {suffix}'
    expected = []
    for i, (content, literal) in enumerate([
        ('sysexstart',None), ('byte',0), ('byte',32), ('byte',51), ('byte',1),
        ('deviceid',None), ('byte',command), ('part',None),
        ('controllerid',None), ('value',None), ('sysexend',None)]):
        row = {'type':'structure', 'index':str(i), 'content':content}
        if literal is not None:
            row['tag'] = f'{literal:02d}'
        expected.append(row)
    assert [b.attrib for b in messages[key].findall('byte')] == expected, key
cutoff = xml.find(".//ParameterSet[@name='Filter 1']/Parameter[@name='Cutoff']")
assert cutoff is not None
for key,value in dict(defaultmessage='cc', controllerindex='40', minValue='0',
                      maxValue='127', defaultValue='127').items():
    assert cutoff.attrib[key] == value
print('Verified 9 short XML message templates + Filter 1/Cutoff; source SHA-256', digest)
