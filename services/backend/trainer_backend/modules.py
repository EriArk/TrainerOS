# SPDX-License-Identifier: GPL-3.0-or-later
"""Single composition inventory. Empty business capabilities are intentional."""
from dataclasses import dataclass


@dataclass(frozen=True)
class Module:
    name: str
    owner: str
    state: str = 'unavailable'


MODULES = tuple(Module(name, owner) for name, owner in (
    ('accounts', 'S01'), ('profiles', 'S01'), ('registry', 'S10'),
    ('results', 'S10'), ('boards', 'S10'), ('competitions', 'S10'),
    ('community_competitions', 'S10'), ('rewards', 'S11'), ('wallet', 'S11'),
    ('inventory', 'S11'), ('support', 'S11'), ('providers', 'S05/S11'),
)) + (Module('operations', 'S15', 'foundation'),)
MODULE_NAMES = frozenset(m.name for m in MODULES)
