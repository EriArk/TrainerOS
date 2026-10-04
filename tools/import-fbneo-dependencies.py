#!/usr/bin/env python3
"""Generate factual archive dependency names from upstream FBNeo's arcade DAT.

No ROMs, BIOS bytes or game art are read or included. Supply an explicit local
DAT and its reviewed upstream commit; keep the source download outside Git.
"""
import argparse
import hashlib
import re
import xml.etree.ElementTree as ET
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('dat', type=Path)
p.add_argument('--revision', required=True)
a = p.parse_args()
assert re.fullmatch('[0-9a-f]{40}', a.revision)
raw = a.dat.read_bytes()
games = {e.get('name'): e for e in ET.fromstring(raw).findall('game')}

def dependencies(name, trail=()):
    assert name in games and name not in trail
    target = games[name].get('romof')
    return [] if not target else [target] + dependencies(target, trail + (name,))

rows = []
for name in sorted(games):
    deps = list(dict.fromkeys(dependencies(name)))
    assert all(re.fullmatch('[a-zA-Z0-9_-]+', x) for x in [name] + deps)
    rows.append(name + ':' + ','.join(deps))
out = '''#pragma once
#include <QHash>
#include <QStringList>

// Generated factual ROM-set relationships; no game/firmware content.
// Source: https://github.com/libretro/FBNeo/tree/REVISION/dats
// DAT SHA-256: DIGEST
// Regenerate with tools/import-fbneo-dependencies.py; review core updates.
namespace trainer::retroarch {
inline const QHash<QString,QStringList>& fbneoRomSets() {
    static const auto sets=[] {
        const auto data=QString::fromLatin1(
ROWS
        );
        QHash<QString,QStringList> result;
        for(const auto& row:data.split('\\n',Qt::SkipEmptyParts)) {
            const auto end=row.indexOf(':');
            result.insert(row.left(end),row.mid(end+1).split(',',Qt::SkipEmptyParts));
        }
        return result;
    }();
    return sets;
}
}
'''.replace('REVISION', a.revision).replace('DIGEST', hashlib.sha256(raw).hexdigest()).replace('ROWS', '\n'.join('            "' + row + '\\n"' for row in rows))
target = Path(__file__).resolve().parents[1] / 'src/integrations/adventure/retroarch/FBNeoRomSets.h'
target.write_text(out, encoding='utf-8', newline='\n')
print('Exported', len(rows), 'ROM-set dependency records')
