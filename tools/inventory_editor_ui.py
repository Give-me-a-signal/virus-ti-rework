#!/usr/bin/env python3
"""Extract the shipped Desktop editor without executing it or modifying assets."""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import struct
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
RESOURCES = ROOT / 'analysis/package-expanded/Core_components.pkg/Payload/Library/Application Support/Access Music/Virus TI/Common/Virus Control.bundle/Contents/Resources'
METADATA = {'Size', 'BackColor', 'ForeColor', 'TileInsetRect'}


def number(value):
    if value is None:
        return None
    try:
        return int(value)
    except ValueError:
        try:
            return float(value)
        except ValueError:
            return value


def raw_tree(element):
    return {'type': element.tag, 'attrs': dict(element.attrib),
            'text': (element.text or '').strip(), 'children': [raw_tree(c) for c in element]}


def condition_ast(element):
    # The AST represents XML syntax faithfully. It does not claim that spelling
    # aliases or range boundary behavior have been verified in compiled code.
    tests = []
    for test in element:
        assert test.tag == 'Test'
        tests.append({'op': 'test', 'type': test.attrib['type'],
                      'parameter': test.attrib['param'],
                      'min': number(test.attrib['minvalue']),
                      'max': number(test.attrib['maxvalue']), 'attrs': dict(test.attrib)})
    operator = element.attrib['operand']
    assert operator in ('and', 'or')
    expression = {'op': operator, 'args': tests}
    result = element.attrib.get('result')
    if result is None:
        return {'op': 'unspecified-result', 'arg': expression}
    assert result in ('normal', 'invert')
    return {'op': 'not', 'arg': expression} if result == 'invert' else expression


def extract(resources):
    sources = {}
    trees = {}
    for name in ('editor.xml', 'remotedevice.xml', 'editordevice.xml'):
        data = (resources / name).read_bytes()
        sources[name] = {'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)}
        trees[name] = ET.fromstring(data)

    parameters, conditions, valuelists = {}, {}, {}
    parameter_variants, condition_variants, valuelist_variants = defaultdict(list), defaultdict(list), defaultdict(list)
    for name in ('remotedevice.xml', 'editordevice.xml'):
        root = trees[name]
        for group in root.find('ParameterSets'):
            for parameter in group:
                key = group.attrib['name'] + '/' + parameter.attrib['name']
                item = {'key': key, 'source': name, 'attrs': dict(parameter.attrib),
                        'min': number(parameter.attrib.get('minValue')),
                        'max': number(parameter.attrib.get('maxValue')),
                        'default': number(parameter.attrib.get('defaultValue')),
                        'part': parameter.attrib.get('part'),
                        'valuelist': parameter.attrib.get('valuelist')}
                parameter_variants[key].append(item)
                parameters[key] = item
        for condition in root.find('Conditionsets'):
            key = condition.attrib['name']
            item = {'source': name, 'attrs': dict(condition.attrib), 'ast': condition_ast(condition)}
            condition_variants[key].append(item)
            conditions[key] = item
        for valuelist in root.find('Valuelists'):
            key = valuelist.attrib['name']
            item = {'source': name, 'attrs': dict(valuelist.attrib),
                    'entries': [{'type': e.tag, 'attrs': dict(e.attrib),
                                 'index': number(e.attrib.get('index')),
                                 'value': number(e.attrib.get('value')),
                                 'text': (e.text or '').strip()} for e in valuelist]}
            valuelist_variants[key].append(item)
            valuelists[key] = item

    bitmaps = {}
    for path in sorted(resources.rglob('*.png')):
        data = path.read_bytes()
        assert data[:8] == b'\x89PNG\r\n\x1a\n' and data[12:16] == b'IHDR'
        width, height = struct.unpack('>II', data[16:24])
        key = path.relative_to(resources).as_posix()
        bitmaps[key] = {'file': key, 'width': width, 'height': height,
                        'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data),
                        'declarations': []}
    for declaration in trees['editor.xml'].find('Bitmaps'):
        key = declaration.attrib['Name']
        item = bitmaps.setdefault(key, {'file': key, 'missing': True, 'declarations': []})
        item['declarations'].append(raw_tree(declaration))

    nodes = []
    def node(element, parent=None, child_index=0, path='Frame'):
        rect_element = element.find('Size')
        rect = {k: number(rect_element.attrib[k]) for k in ('left', 'top', 'width', 'height')} if rect_element is not None else {
            'left': 0, 'top': 0, 'width': number(element.attrib['width']), 'height': number(element.attrib['height'])}
        absolute = dict(rect)
        if parent:
            absolute['left'] += parent['absolute_rect']['left']
            absolute['top'] += parent['absolute_rect']['top']
        result = {'id': 'node%04d' % len(nodes), 'type': element.tag,
                  'attrs': dict(element.attrib), 'rect': rect, 'absolute_rect': absolute,
                  'parent_id': parent['id'] if parent else None, 'draw_order': len(nodes),
                  'sibling_index': child_index, 'path': path,
                  'metadata': {c.tag: dict(c.attrib) for c in element if c.tag in METADATA},
                  'children': [], 'sprites': {}}
        nodes.append(result)
        for attr, filename in element.attrib.items():
            if not filename.endswith('.png'):
                continue
            image = bitmaps.get(filename)
            sprite = {'file': filename, 'missing': image is None or image.get('missing', False)}
            if image and not image.get('missing'):
                sprite.update({'image_width': image['width'], 'image_height': image['height']})
                if rect['width'] == image['width'] and rect['height'] > 0 and image['height'] % rect['height'] == 0:
                    sprite.update({'vertical_frame_count': image['height'] // rect['height'],
                                   'frame_width': rect['width'], 'frame_height': rect['height'],
                                   'frame_count_basis': 'PNG dimensions divided by control rectangle; runtime frame selection unverified'})
            result['sprites'][attr] = sprite
        controls = [c for c in element if c.tag not in METADATA]
        for index, child in enumerate(controls):
            result['children'].append(node(child, result, index, path + '/' + child.tag + '[' + str(index) + ']'))
        return result
    frame = node(trees['editor.xml'].find('Frame'))
    flat_nodes = [{**n, 'children': [c['id'] for c in n['children']]} for n in nodes]
    condition_references = {n['attrs']['Condition'] for n in nodes if 'Condition' in n['attrs']}
    parameter_references = {n['attrs'][k] for n in nodes for k in ('Parameter', 'parameter') if k in n['attrs']}
    test_references = {test['attrs']['param'] for variants in condition_variants.values() for item in variants
                       for test in (item['ast']['arg'] if item['ast']['op'] in ('not', 'unspecified-result') else item['ast'])['args']}
    missing_bitmaps = sorted({s['file'] for n in nodes for s in n['sprites'].values() if s['missing']})
    navigation = [n for n in flat_nodes if n['attrs'].get('Parameter') in (
        'VirusControl/SelectedPage', 'VirusControl/Selected Section',
        'VirusControl/SelectedUtilityPage', 'Multi Parameters/Panel Part')]
    catalog = {
        'schema_version': 1, 'resource_root': str(resources.resolve()), 'sources': sources,
        'interpretation': {
            'selection_policy': 'For lookup maps only: editordevice.xml overrides same-named remotedevice.xml records. All source variants preserved. Compiled precedence unverified.',
            'geometry': 'Original integer top-left coordinates; absolute_rect is sum of ancestors. XML child order preserved as draw_order. Runtime clipping/occlusion not proven.',
            'condition_ast': 'Exact XML operators and test spellings preserved. Range inclusivity and equal/equals aliasing require compiled-code verification.',
            'prototype_policy': 'Separate authoring prototype records; not merged into concrete Frame nodes.',
            'part_policy': 'Raw Part and parameter part preserved. -1 appears on current-part controls; runtime inheritance/override semantics require verification.',
        },
        'frame': frame, 'nodes': flat_nodes,
        'prototypes': [raw_tree(e) for e in trees['editor.xml'].find('Prototypes')],
        'parameters': parameters, 'parameter_variants': dict(parameter_variants),
        'conditions': conditions, 'condition_variants': dict(condition_variants),
        'valuelists': valuelists, 'valuelist_variants': dict(valuelist_variants),
        'bitmaps': bitmaps, 'navigation_controls': navigation,
        'diagnostics': {
            'missing_control_conditions': sorted(condition_references - conditions.keys()),
            'missing_control_parameters': sorted(parameter_references - parameters.keys()),
            'missing_test_parameters': sorted(test_references - parameters.keys()),
            'missing_control_bitmaps': missing_bitmaps,
            'duplicate_condition_names': sorted(k for k, v in condition_variants.items() if len(v) > 1),
            'duplicate_parameter_names': sorted(k for k, v in parameter_variants.items() if len(v) > 1),
            'duplicate_valuelist_names': sorted(k for k, v in valuelist_variants.items() if len(v) > 1),
        },
        'counts': {'frame_nodes_including_root': len(nodes), 'top_level_containers': len(frame['children']),
                   'widget_types': dict(sorted(Counter(n['type'] for n in nodes).items())),
                   'png_files': len([v for v in bitmaps.values() if not v.get('missing')]),
                   'bitmap_declarations': len(trees['editor.xml'].find('Bitmaps')),
                   'prototypes': len(trees['editor.xml'].find('Prototypes')),
                   'parameters': len(parameters), 'conditions': len(conditions), 'valuelists': len(valuelists)}
    }
    assert len({n['id'] for n in nodes}) == len(nodes)
    assert not missing_bitmaps, missing_bitmaps
    assert frame['rect'] == {'left': 0, 'top': 0, 'width': 907, 'height': 513}
    assert len(navigation) >= 16
    by_id = {n['id']: n for n in nodes}
    for item in flat_nodes:
        assert all(by_id[c]['parent_id'] == item['id'] for c in item['children'])
        if item['parent_id']:
            parent = by_id[item['parent_id']]
            assert item['absolute_rect']['left'] == item['rect']['left'] + parent['absolute_rect']['left']
            assert item['absolute_rect']['top'] == item['rect']['top'] + parent['absolute_rect']['top']
    assert len(nodes)-1 == sum(1 for e in trees['editor.xml'].find('Frame').iter() if e.tag not in METADATA)-1
    assert sum(map(len, condition_variants.values())) == 335
    return catalog


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--resources', type=Path, default=RESOURCES)
    parser.add_argument('--output', type=Path, default=ROOT / 'analysis/editor-ui-catalog.json')
    parser.add_argument('--check', action='store_true', help='Check saved catalog against fresh extraction without modifying it')
    args = parser.parse_args()
    catalog = extract(args.resources)
    if args.check:
        assert json.loads(args.output.read_text()) == catalog, 'Saved catalog differs from source extraction'
    else:
        args.output.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'output': str(args.output), 'counts': catalog['counts'], 'diagnostics': catalog['diagnostics']}, indent=2))


if __name__ == '__main__':
    main()
