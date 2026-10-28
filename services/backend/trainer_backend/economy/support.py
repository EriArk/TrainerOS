# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only support estimates from complete, trusted reward history.

No membership proof or payout is implied. The current ledger has one immutable
credit per RA reward. A future supplemental-grant writer must extend this reader
to include its per-reward receipts before enabling membership activation.
"""

POLICY = 'support-preview-2026-10-v1'
MAX_SAFE_INTEGER = 9007199254740991
TIERS = ((2, 'Support x2'), (3, 'Support x3'))


def preview(conn, player, balance):
    totals = conn.execute('''SELECT count(*) AS rewards,
        COALESCE(sum(base_sparks),0) AS base,
        COALESCE(sum(sparks),0) AS credited,
        COALESCE(sum(greatest(base_sparks::numeric*2-sparks,0)),0) AS topup2,
        COALESCE(sum(greatest(base_sparks::numeric*3-sparks,0)),0) AS topup3,
        count(*) FILTER (WHERE sparks<base_sparks OR sparks>base_sparks::numeric*3) AS invalid
        FROM trainer_backend.reward_ledger WHERE player_id=%s
        AND source IN ('ra.achievement','ra.completion')''', (player,)).fetchone()
    values = {key: int(value) for key, value in totals.items()}
    if values['invalid'] or any(value < 0 or value > MAX_SAFE_INTEGER for value in values.values()):
        return None
    options = []
    for multiplier, title in TIERS:
        topup = values['topup'+str(multiplier)]
        if balance + topup > MAX_SAFE_INTEGER:
            return None
        options.append({'id': 'support-x'+str(multiplier), 'title': title,
                        'multiplier': multiplier, 'top_up': topup,
                        'potential_balance': balance + topup})
    return {'policy': POLICY, 'state': 'calculation_only', 'activation_available': False,
            'eligible_rewards': values['rewards'], 'base_sparks': values['base'],
            'credited_reward_sparks': values['credited'], 'options': options}
