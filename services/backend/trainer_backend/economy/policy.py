# SPDX-License-Identifier: GPL-3.0-or-later
"""Owner-authorized first balance. Applied amounts are immutable ledger facts."""
from math import isqrt

VERSION = 'sparks-2026-10-v1'


def achievement(points):
    if type(points) is not int or not 0 < points <= 100:
        raise ValueError('invalid_points')
    p = min(points, 50)
    return 10 + 4 * p, 5 + 2 * p


def completion(points):
    if type(points) is not int or points <= 0:
        raise ValueError('invalid_set_points')
    p = min(points, 1000)
    return 500 + 4 * p, 250 + 2 * p


def mastery(xp):
    if type(xp) is not int or xp < 0:
        raise ValueError('invalid_xp')
    # Total XP at level L = 50*(L-1)*(L+4). No arbitrary maximum level.
    n = max(0, (isqrt(25 + 4 * (xp // 50)) - 5) // 2)
    floor = 50 * n * (n + 5)
    step = 300 + 100 * n
    return {'xp': xp, 'level': n + 1, 'level_xp': xp - floor,
            'next_level_xp': step, 'policy': VERSION}
