"""
Generates include/presets.h from the local copies of:
  data/bands-r2.json         OpenWebRX+ band plan with the usual digital mode frequencies
                             https://github.com/0xAF/openwebrxplus/blob/master/bands-r2.json
  data/dx_community.json     KiwiSDR community DX labels
                             https://github.com/jks-prv/KiwiSDR/blob/master/unix_env/kiwi.config/dist.dx_community.json
  data/dx_community_config.json  KiwiSDR DX type names

Only frequencies inside a band of the firmware band table (src/main.cpp) are kept, so every
preset can be tuned by the receiver. Run from the tools folder:  python gen_presets.py
"""
import html, json, os, re, unicodedata
from collections import OrderedDict
from u8font import Font, parse_c_array

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DATA = os.path.join(HERE, 'data')
OUT = os.path.join(ROOT, 'include', 'presets.h')

# Longest name that fits the middle of the band ruler (bold 20 px font). The menu lists cut
# the names further at run time to leave room for the frequency.
MAX_NAME_PX = 185


def font_widths(name):
    path = os.path.join(ROOT, 'include', 'fonts', 'font_%s.h' % name)
    f = Font(parse_c_array(open(path, encoding='latin-1').read(), 'u8g2_font_' + name))
    return {chr(k): g.dx for k, g in f.glyphs.items()}


B20 = font_widths('B20_tf')


def text_px(s):
    return sum(B20.get(c, 0) for c in s)


def fit(s, px):
    while s and text_px(s) > px:
        s = s[:-1]
    return s.rstrip(' -/,')


def clean(s):
    s = re.sub(r'<a[^>]*>.*?</a>', '', s)      # HTML links in some labels
    s = re.sub(r'<[^>]*>', '', s)
    s = html.unescape(s.replace('%22', '"'))
    s = unicodedata.normalize('NFKD', s).encode('ascii', 'ignore').decode()
    s = s.split('\n')[0]
    s = re.sub(r'\s+', ' ', s).strip()
    return s


# ---------------------------------------------------------------------------------------------
# Firmware band table: { "Ham 20M", SW_BAND_TYPE, USB, 14000, 14350, ... }
src = open(os.path.join(ROOT, 'src', 'main.cpp'), encoding='utf-8').read()
bands = [(n, t, int(lo), int(hi)) for n, t, lo, hi in
         re.findall(r'\{\s*"([^"]+)"\s*,\s*(\w+_BAND_TYPE)\s*,\s*\w+\s*,\s*(\d+)\s*,\s*(\d+)', src)]
assert bands, 'band table not found'


def receivable(hz):
    khz = hz / 1000.0
    return any(t != 'FM_BAND_TYPE' and lo <= khz <= hi for n, t, lo, hi in bands)


# ---------------------------------------------------------------------------------------------
# Types (KiwiSDR dx_type names, plus the OpenWebRX+ digital modes as Ham)
kcfg = json.load(open(os.path.join(DATA, 'dx_community_config.json'), encoding='utf-8'))
kiwi_types = {t['key']: t['name'] for t in kcfg['dx_type']}
TYPE_ORDER = ['Ham', 'Bcast', 'Time', 'Utility', 'CW', 'FSK', 'FAX', 'Marine', 'Aero', 'HFDL', 'ALE', 'Milcom', 'Spy', 'Channel']

MODES = {'USB': 'USB', 'LSB': 'LSB', 'AM': 'AM', 'AMN': 'AM', 'CW': 'CW', 'CWN': 'CW'}
NARROW = {'AMN', 'CWN'}
WITH_NOTES = {'Aero', 'HFDL', 'FAX'}

presets = OrderedDict()     # (hz, mode) -> dict


def add(hz, mode, narrow, typ, name):
    if not receivable(hz) or not name:
        return
    key = (hz, mode)
    if key in presets:
        p = presets[key]
        if name not in p['names']:
            p['names'].append(name)
        return
    presets[key] = {'hz': hz, 'mode': mode, 'narrow': narrow, 'type': typ, 'names': [name]}


# OpenWebRX+ usual frequencies
OWRX_NAMES = {'bpsk31': 'PSK31', 'cwskimmer': 'CW Skimmer', 'rtty450': 'RTTY', 'fst4w': 'FST4W'}
OWRX_MODE = {'cwskimmer': 'CW', 'navtex': 'CW'}
owrx = json.load(open(os.path.join(DATA, 'bands-r2.json'), encoding='utf-8'))
for b in owrx:
    for mode_name, val in b.get('frequencies', {}).items():
        items = val if isinstance(val, list) else [val]
        for it in items:
            hz = it['frequency'] if isinstance(it, dict) else it
            under = it.get('underlying', 'usb').upper() if isinstance(it, dict) else 'USB'
            mode = OWRX_MODE.get(mode_name, under)
            typ = 'Marine' if mode_name == 'navtex' else 'Ham'
            add(int(hz), mode, False, typ, OWRX_NAMES.get(mode_name, mode_name.upper()))

# KiwiSDR community labels
dx = json.load(open(os.path.join(DATA, 'dx_community.json'), encoding='utf-8'))['dx']
for khz, kmode, ident, notes, opts in dx:
    if kmode not in MODES:
        continue
    t = next((int(k[1:]) for k in opts if re.fullmatch(r'T\d+', k)), 0)
    typ = kiwi_types.get(t, 'Utility')
    if typ in ('Reserved', 'masked'):
        continue
    name = clean(ident)
    if typ in WITH_NOTES and clean(notes):
        name += ' ' + clean(notes)          # Generic idents (VOLMET, MWARA, HFDL) need the location
    add(int(round(khz * 1000)), MODES[kmode], kmode in NARROW, typ, name)

# ---------------------------------------------------------------------------------------------
# Menu groups: the OpenWebRX+ band the preset is in (ham, broadcast, CB), else its MHz range
GROUP_RENAME = {'AM Broadcast': 'Bcast LW/MW', '11m CB': 'CB 11m'}


def group_of(hz):
    for b in owrx:
        if b['lower_bound'] <= hz <= b['upper_bound'] and b['upper_bound'] <= 30000000:
            n = b['name']
            if n in GROUP_RENAME:
                return GROUP_RENAME[n], b['lower_bound']
            if 'hamradio' in b.get('tags', []):
                return 'Ham ' + n, b['lower_bound']
            if n.endswith(' Broadcast'):
                return 'Bcast ' + n[:-len(' Broadcast')], b['lower_bound']
            return n, b['lower_bound']
    if hz < 200000:
        return 'LF', 0
    mhz = hz // 1000000
    return '%d MHz' % mhz, mhz * 1000000


items = sorted(presets.values(), key=lambda p: (p['hz'], p['mode']))
groups = OrderedDict()
for p in items:
    g, start = group_of(p['hz'])
    p['group'] = g
    groups.setdefault(g, start)
group_names = sorted(groups, key=lambda g: (groups[g], g))
types_used = [t for t in TYPE_ORDER if any(p['type'] == t for p in items)]
types_used += sorted({p['type'] for p in items} - set(types_used))

max_list = 0
for g in group_names:
    for t in types_used:
        max_list = max(max_list, sum(1 for p in items if p['group'] == g and p['type'] == t))
assert max_list < 250, 'list too long for the menu (u8sl uses uint8_t)'


def c_str(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


MODE_C = {'AM': 'AM', 'LSB': 'LSB', 'USB': 'USB', 'CW': 'CW'}
out = []
out.append('// Generated by tools/gen_presets.py - do not edit.')
out.append('// Sources (local copies in tools/data): OpenWebRX+ bands-r2.json and KiwiSDR dist.dx_community.json')
out.append('#pragma once')
out.append('#include <Arduino.h>')
out.append('#include "global.h"')
out.append('')
out.append('#define PRESET_NARROW 0x01        // AMN / CWN: narrow filter')
out.append('')
out.append('struct Preset {')
out.append('    uint32_t hz;                // Dial frequency (carrier for AM/CW)')
out.append('    uint8_t mode;               // AM, LSB, USB or CW')
out.append('    uint8_t type;               // Index in presetTypeNames')
out.append('    uint8_t group;              // Index in presetGroupNames')
out.append('    uint8_t flags;')
out.append('    const char* name;           // Fits the menu and the band ruler')
out.append('};')
out.append('')
out.append('#define PRESET_TYPE_COUNT %d' % len(types_used))
out.append('static const char* const presetTypeNames[] = { %s };' % ', '.join(c_str(t) for t in types_used))
out.append('')
out.append('#define PRESET_GROUP_COUNT %d' % len(group_names))
out.append('static const char* const presetGroupNames[] = {')
for g in group_names:
    out.append('    %s,' % c_str(g))
out.append('};')
out.append('')
out.append('#define PRESET_MAX_LIST %d        // Largest group/type list' % max_list)
out.append('#define PRESET_COUNT %d' % len(items))
out.append('static const Preset presets[PRESET_COUNT] = {')
for p in items:
    name = fit('/'.join(p['names']), MAX_NAME_PX)
    out.append('    { %9d, %-3s, %2d, %2d, %d, %s },' % (
        p['hz'], MODE_C[p['mode']], types_used.index(p['type']), group_names.index(p['group']),
        1 if p['narrow'] else 0, c_str(name)))
out.append('};')
out.append('')

open(OUT, 'w', encoding='ascii', newline='\n').write('\n'.join(out))
print('%d presets, %d groups, %d types, largest list %d -> %s' % (len(items), len(group_names), len(types_used), max_list, OUT))
for g in group_names:
    print('  %-14s %3d' % (g, sum(1 for p in items if p['group'] == g)))
