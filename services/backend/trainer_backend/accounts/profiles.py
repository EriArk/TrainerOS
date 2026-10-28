# SPDX-License-Identifier: GPL-3.0-or-later
"""Public presentation, never authentication or inventory authority."""
import re
import unicodedata
from uuid import UUID
from psycopg.types.json import Jsonb
from .provider import AuthError


def display_name(user):
    for key in ('global_name', 'username'):
        value = user.get(key)
        if isinstance(value, str):
            value = unicodedata.normalize('NFC', value)
            value = ''.join(c for c in value if not unicodedata.category(c).startswith('C'))
            value = ' '.join(value.split())[:64].strip()
            if value:
                return value
    return 'Player'


def avatar_recipe(value):
    # Current free SDK pack only. No URLs, paths, meshes or item ownership claims.
    if value is None:
        return None
    valid = isinstance(value, dict) and set(value) == {'schema', 'packRevision', 'appearance'}
    valid = valid and type(value['schema']) is int and value['schema'] == 1
    valid = valid and isinstance(value['packRevision'], str) and re.fullmatch('[a-f0-9]{64}', value['packRevision'])
    a = value.get('appearance') if valid else None
    colors = {'skinColor', 'hairColor', 'eyeColor', 'shirtColor', 'shortsColor'}
    wardrobe = {'top': ('tank', 'polo-short-sleeve', 'polo-sleeveless'),
                'bottom': ('base-shorts', 'golf-shorts', 'pleated-skirt'),
                'socks': ('none', 'ankle-socks', 'crew-socks'),
                'shoes': ('sandals', 'golf-shoes-classic', 'golf-shoes-sport'),
                'headwear': ('none', 'golf-cap', 'golf-visor')}
    valid = valid and isinstance(a, dict) and type(a.get('schema')) is int and a['schema'] in (1, 2, 3)
    if valid and a['schema'] == 3:
        colors |= {'socksColor', 'shoesColor', 'headwearColor'}
    required = colors | {'schema', 'pack', 'hair', 'eyes'}
    if valid and a['schema'] >= 2:
        required |= wardrobe.keys()
    valid = valid and set(a) == required and a['pack'] == 'trainer-avatar-1'
    valid = valid and (a['schema'] == 1 or all(isinstance(a[k], str) and a[k] in values for k, values in wardrobe.items()))
    eyes = ('round', 'almond') + tuple(f'{style}-{variant}'
        for style in ('rogue', 'innocent', 'sly', 'calm', 'sleepy', 'determined')
        for variant in ('base', 'feminine'))
    valid = valid and a['hair'] in ('swept', 'ponytail', 'crop', 'bob') and a['eyes'] in eyes
    valid = valid and all(isinstance(a[k], str) and re.fullmatch('#[a-fA-F0-9]{6}', a[k]) for k in colors)
    if not valid:
        raise AuthError('invalid_avatar_recipe', 422)
    return value


def public_profiles(conn, players):
    ids = sorted({UUID(str(p)) for p in players})
    if not ids:
        return {}
    rows = conn.execute('''SELECT f.player_id,f.display_name,f.avatar,f.revision
        FROM trainer_backend.public_profiles f JOIN trainer_backend.principals p ON p.id=f.player_id
        WHERE f.player_id=ANY(%s) AND p.state='active' ''', (ids,)).fetchall()
    profiles = {str(r['player_id']): r for r in rows}
    return {str(p): profiles.get(str(p), {'player_id': p, 'display_name': 'Player', 'avatar': None, 'revision': 0}) for p in ids}


def update_name(conn, player, name):
    conn.execute('''INSERT INTO trainer_backend.public_profiles(player_id,display_name) VALUES(%s,%s)
        ON CONFLICT(player_id) DO UPDATE SET display_name=EXCLUDED.display_name,
        revision=public_profiles.revision+1,updated_at=clock_timestamp()
        WHERE public_profiles.display_name IS DISTINCT FROM EXCLUDED.display_name''', (player, name))


def update_avatar(conn, player, avatar):
    conn.execute('''INSERT INTO trainer_backend.public_profiles(player_id,display_name,avatar) VALUES(%s,'Player',%s)
        ON CONFLICT(player_id) DO UPDATE SET avatar=EXCLUDED.avatar,
        revision=public_profiles.revision+1,updated_at=clock_timestamp()
        WHERE public_profiles.avatar IS DISTINCT FROM EXCLUDED.avatar''', (player, Jsonb(avatar) if avatar is not None else None))
