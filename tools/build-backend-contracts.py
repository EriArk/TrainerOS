#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build reviewable Backend draft contracts. Does not start or implement a service."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'services/backend/contracts'
DRAFT = 'architecture-draft.4'
MODULES = {
    'accounts': 'S01', 'profiles': 'S01', 'registry': 'S10', 'results': 'S10',
    'boards': 'S10', 'competitions': 'S10', 'community_competitions': 'S10',
    'rewards': 'S11', 'wallet': 'S11', 'inventory': 'S11', 'support': 'S11',
    'providers': 'S05/S11', 'operations': 'S15',
}
DEFS: dict = {}


def ref(name):
    return {'$ref': '#/$defs/' + name}


def string(limit=128, minimum=1, **kwargs):
    return {'type': 'string', 'minLength': minimum, 'maxLength': limit, **kwargs}


def enum(*values):
    return {'type': 'string', 'enum': list(values)}


def integer(low=0, high=9007199254740991):
    return {'type': 'integer', 'minimum': low, 'maximum': high}


def array(item, maximum=100, minimum=0, unique=False):
    return {'type': 'array', 'items': item, 'minItems': minimum,
            'maxItems': maximum, 'uniqueItems': unique}


def obj(fields, optional=()):
    return {'type': 'object', 'properties': fields, 'additionalProperties': False,
            'required': [key for key in fields if key not in optional]}


def define(name, fields, optional=(), **annotations):
    DEFS[name] = obj(fields, optional) | annotations
    return ref(name)


def page(name, item):
    return define(name, {'items': array(ref(item)), 'revision': ref('Revision'),
                         'next_cursor': string(512)}, ('next_cursor',))


DEFS.update({
    'Id': string(36, 36, format='uuid', pattern=r'^[0-9a-f-]{36}$'),
    'Revision': string(19, pattern=r'^[1-9][0-9]{0,18}$'),
    'Digest': string(64, 64, pattern=r'^[0-9a-f]{64}$'),
    'Code': string(64, pattern=r'^[a-z][a-z0-9_.-]{0,63}$'),
    'Time': string(32, 20, format='date-time', pattern=r'Z$'),
    'Token': string(2048, 32, writeOnly=True),
    'Reason': string(500),
    'Empty': obj({}),
})
define('Error', {'code': enum('unauthenticated', 'forbidden', 'not_found', 'conflict',
    'invalid_request', 'unsupported_ruleset', 'invalid_evidence', 'rate_limited',
    'provider_unavailable', 'provider_not_configured', 'service_unavailable', 'stale_context', 'policy_unresolved',
    'login_restart_required', 'authorization_pending', 'login_cancelled', 'session_limit',
    'confirmation_rejected', 'body_too_large', 'request_timeout'),
    'message': string(500), 'request_id': ref('Id'), 'retryable': {'type': 'boolean'},
    'retry_after_seconds': integer(1, 86400)}, ('retry_after_seconds',))
define('OperationReceipt', {'operation_id': ref('Id'), 'state': enum('pending',
    'completed', 'rejected'), 'revision': ref('Revision'), 'resource_id': ref('Id')},
    ('resource_id',))
define('ExpectedRevision', {'expected_revision': ref('Revision')})
define('ReasonedRevision', {'expected_revision': ref('Revision'), 'reason': ref('Reason')})

# Accounts: attempt possession and backend session credentials are separate.
define('LoginBegin', {'provider': enum('fluxer'), 'device_label': string(64),
    'claim_challenge': string(43, 43, pattern=r'^[A-Za-z0-9_-]{43}$'),
    'claim_challenge_method': {'const': 'S256'},
    'return_uri': string(128, pattern=r'^http://127\.0\.0\.1:[1-9][0-9]{3,4}/traineros/auth/[A-Za-z0-9_-]{43}$'),
    'expected_subject': string(32,pattern=r'^[0-9]{1,32}$')}, optional=('return_uri','expected_subject'))
define('LoginAttempt', {'attempt_id': ref('Id'), 'authorization_url': string(2048,
    format='uri', pattern=r'^https://'), 'expires_at': ref('Time'),
    'verification_code': string(8,8,pattern=r'^[0-9]{8}$')},
    description='Native return_uri skips manual code and requires a browser-only handoff_token at claim. Without return_uri, verification_code is an interim explicit device confirmation; never auto-submit it.')
define('LoginClaim', {'attempt_id': ref('Id'), 'claim_verifier': string(128, 43,
    pattern=r'^[A-Za-z0-9._~-]{43,128}$'), 'handoff_token': string(43,43,pattern=r'^[A-Za-z0-9_-]{43}$')},optional=('handoff_token',))
define('SessionTokens', {'player_id': ref('Id'), 'session_id': ref('Id'),
    'access_token': string(2048, 32), 'refresh_token': string(2048, 32),
    'expires_at': ref('Time'), 'fluxer_subject': string(32,pattern=r'^[0-9]{1,32}$')}, description='Secret host-only response; never log or expose to games.')
define('Refresh', {'refresh_token': ref('Token')})
define('ReauthGrant', {'reauth_grant': string(2048, 32), 'expires_at': ref('Time'),
    'purpose': ref('Code')}, description='One-shot grant bound to actor, current session and sensitive operation.')
define('ReauthBegin', {'purpose': enum('identity_link', 'identity_unlink', 'recovery_rotate',
    'account_export', 'account_delete', 'support_link'),
    'claim_challenge': string(43, 43, pattern=r'^[A-Za-z0-9_-]{43}$')})
define('OperatorReauthBegin', {'purpose': enum('definition_publish', 'result_correct',
    'dispute_resolve', 'job_retry'), 'claim_challenge': string(43, 43,
    pattern=r'^[A-Za-z0-9_-]{43}$')})
define('Me', {'player_id': ref('Id'), 'status': enum('active', 'suspended',
    'deletion_pending'), 'revision': ref('Revision')})
define('Session', {'session_id': ref('Id'), 'device_label': string(64),
    'created_at': ref('Time'), 'expires_at': ref('Time'), 'current': {'type': 'boolean'}})
page('Sessions', 'Session')
define('IdentityLink', {'id': ref('Id'), 'provider': ref('Code'),
    'display_label': string(100), 'verified_at': ref('Time')})
page('IdentityLinks', 'IdentityLink')
define('RecoveryCodes', {'codes': array(string(128, 32), 10, 1, True)},
    description='One-time secret delivery to shell. Hashes only at rest; never game-visible.')
define('RecoveryBegin', {'recovery_code': string(128, 32, writeOnly=True),
    'new_provider': enum('fluxer'), 'claim_challenge': string(43, 43,
    pattern=r'^[A-Za-z0-9_-]{43}$')},
    description='Binds a fresh provider flow. Code alone does not return a session.')

define('PublicProfile', {'player_id': ref('Id'), 'display_name': string(64),
    'avatar_revision': ref('Revision'), 'revision': ref('Revision')})
define('ProfileWrite', {'display_name': string(64), 'expected_revision': ref('Revision')})
define('AvatarSelection', {'slot': ref('Code'), 'item_id': ref('Id'),
    'item_revision': ref('Revision'), 'variant': ref('Code')}, ('variant',))
define('AvatarAppearanceV1', {'schema': {'const': 1}, 'pack': ref('Code'),
    'hair': ref('Code'), 'eyes': ref('Code'), **{name: string(7, 7, pattern=r'^#[0-9a-fA-F]{6}$')
    for name in ('hairColor', 'eyeColor', 'skinColor', 'shirtColor', 'shortsColor')}})
define('AvatarPeerRecipeV1', {'schema': {'const': 1}, 'packRevision': ref('Digest'),
    'appearance': ref('AvatarAppearanceV1')})
define('AvatarRecipe', {'recipe': ref('AvatarPeerRecipeV1'),
    'selections': array(ref('AvatarSelection'), 32), 'revision': ref('Revision')},
    description='Preserves existing SDK v1 recipe verbatim. Selections map future inventory; no local paths.')
define('AvatarWrite', {'recipe': ref('AvatarRecipe'), 'expected_revision': ref('Revision')},
    description='Cloud storage of existing SDK recipe; wire adapter must preserve local avatar contract.')

define('Game', {'game_id': ref('Code'), 'title': string(100), 'revision': ref('Revision')})
page('Games', 'Game')
define('Ruleset', {'ruleset_id': ref('Id'), 'game_id': ref('Code'),
    'content_digest': ref('Digest'), 'rules_digest': ref('Digest'),
    'validator_digest': ref('Digest'), 'status': enum('draft', 'active', 'retired',
    'revoked'), 'policy_id': ref('Id'), 'revision': ref('Revision')})
page('Rulesets', 'Ruleset')
define('Metric', {'name': ref('Code'), 'unit': ref('Code'),
    'direction': enum('ascending', 'descending')})
define('Board', {'board_id': ref('Id'), 'game_id': ref('Code'), 'ruleset_id': ref('Id'),
    'mode': ref('Code'), 'category': ref('Code'), 'metrics': array(ref('Metric'), 8, 1),
    'proof': enum('participant_consensus', 'server_replay', 'verified_provider'),
    'subject_kind': enum('player', 'roster'), 'policy_id': ref('Id'),
    'revision': ref('Revision'), 'competition_id': ref('Id')}, ('competition_id',))
page('Boards', 'Board')
define('Subject', {'kind': enum('player', 'roster'),
    'players': array(ref('Id'), 16, 1, True)},
    allOf=[{'if': {'properties': {'kind': {'const': 'player'}}},
            'then': {'properties': {'players': {'maxItems': 1}}},
            'else': {'properties': {'players': {'minItems': 2}}}}])
define('BoardEntry', {'rank': integer(1), 'subject': ref('Subject'),
    'metrics': array(integer(-9007199254740991), 8, 1), 'result_id': ref('Id'),
    'decision_revision': ref('Revision')})
page('BoardEntries', 'BoardEntry')
define('Statistics', {'player_id': ref('Id'), 'game_id': ref('Code'),
    'matches': integer(), 'wins': integer(), 'losses': integer(), 'draws': integer(),
    'unique_defeated_players': integer(), 'revision': ref('Revision')})

# Match registration fixes roster separately from membership in the existing Lobby.
define('MatchCreate', {'game_id': ref('Code'), 'ruleset_id': ref('Id'),
    'mode': ref('Code'), 'origin': enum('online', 'offline_import'),
    'competition_id': ref('Id')}, ('competition_id',))
define('Admission', {'expected_revision': ref('Revision'), 'team_slot': integer(0, 15)},
    ('team_slot',))
define('Participant', {'player_id': ref('Id'), 'team_slot': integer(0, 15),
    'status': enum('admitted', 'withdrawn')}, ('team_slot',))
define('Match', {'match_id': ref('Id'), 'game_id': ref('Code'), 'ruleset_id': ref('Id'),
    'state': enum('preparing', 'active', 'submitted', 'settled', 'aborted', 'disputed'),
    'participants': array(ref('Participant'), 16), 'revision': ref('Revision'),
    'submission_deadline': ref('Time')}, ('submission_deadline',))
define('EvidenceBegin', {'byte_length': integer(1, 1048576), 'sha256': ref('Digest'),
    'format': ref('Code')})
define('EvidenceUpload', {'upload_id': ref('Id'), 'chunk_bytes': {'const': 16384},
    'expires_at': ref('Time')})
define('EvidenceChunk', {'data_base64': string(21848, 4, pattern=r'^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$')},
    description='Decode <=16384 bytes; fixed offsets and total length checked semantically.')
define('EvidenceRef', {'evidence_id': ref('Id'), 'sha256': ref('Digest'),
    'byte_length': integer(1, 1048576), 'format': ref('Code')})
define('Submission', {'evidence': ref('EvidenceRef'), 'expected_match_revision': ref('Revision')},
    description='No caller-selected player, score, verified flag or reward; derived by validator.')
define('ResultOutcome', {'subject': ref('Subject'), 'placement': integer(1, 16),
    'metrics': array(integer(-9007199254740991), 8, 1),
    'completion': enum('completed', 'stroke_cap', 'dnf', 'aborted')})
define('ResultReceipt', {'receipt_id': ref('Id'), 'match_id': ref('Id'),
    'state': enum('pending', 'verified', 'rejected', 'disputed', 'voided'),
    'proof': enum('unverified', 'participant_consensus', 'server_replay', 'verified_provider'),
    'decision_revision': ref('Revision'), 'reason_code': ref('Code'),
    'outcomes': array(ref('ResultOutcome'), 16)}, ('reason_code',))
page('Results', 'ResultReceipt')

define('Competition', {'competition_id': ref('Id'), 'kind': enum('daily', 'tournament',
    'community'), 'title': string(100), 'state': enum('draft', 'published',
    'registration', 'running', 'provisional', 'finalized', 'cancelled'),
    'policy_id': ref('Id'), 'ruleset_id': ref('Id'), 'opens_at': ref('Time'),
    'closes_at': ref('Time'), 'finalizes_at': ref('Time'), 'revision': ref('Revision')})
page('Competitions', 'Competition')
define('CompetitionEntry', {'subject': ref('Subject'), 'expected_revision': ref('Revision')},
    description='All roster members must independently consent; delegation validated server-side.')
define('Entry', {'entry_id': ref('Id'), 'subject': ref('Subject'),
    'status': enum('pending_consents', 'admitted', 'withdrawn', 'rejected'),
    'revision': ref('Revision')})
define('DisputeWrite', {'result_id': ref('Id'), 'decision_revision': ref('Revision'),
    'reason': ref('Reason')})
define('Dispute', {'dispute_id': ref('Id'), 'result_id': ref('Id'),
    'state': enum('open', 'reviewing', 'resolved', 'dismissed'), 'revision': ref('Revision')})
define('CommunityRoster', {'community_ref': ref('Id'), 'subject': ref('Subject'),
    'membership_evidence_id': ref('Id'), 'verified_at': ref('Time'),
    'valid_until': ref('Time'), 'revision': ref('Revision')})
page('CommunityRosters', 'CommunityRoster')

define('Reward', {'reward_id': ref('Id'), 'rule_id': ref('Id'), 'source_event_id': ref('Id'),
    'state': enum('pending', 'issued', 'reversed'), 'revision': ref('Revision')})
page('Rewards', 'Reward')
define('Trophy', {'trophy_id': ref('Id'), 'award_id': ref('Id'), 'title': string(100),
    'state': enum('active', 'expired', 'revoked'), 'expires_at': ref('Time')}, ('expires_at',))
page('Trophies', 'Trophy')
define('Wallet', {'currency': {'const': 'trainer_coins'}, 'available': integer(),
    'revision': ref('Revision')})
define('LedgerEntry', {'transaction_id': ref('Id'), 'delta': integer(-9007199254740991),
    'source_operation_id': ref('Id'), 'posted_at': ref('Time')})
page('Ledger', 'LedgerEntry')
define('ShopItem', {'item_id': ref('Id'), 'title': string(100), 'price': integer(),
    'offer_revision': ref('Revision'), 'item_revision': ref('Revision'),
    'compatibility': array(ref('Code'), 32, 1)})
page('ShopItems', 'ShopItem')
define('PurchaseWrite', {'item_id': ref('Id'), 'offer_revision': ref('Revision'),
    'expected_price': integer()}, description='Shell-owned explicit confirmation required.')
define('Purchase', {'purchase_id': ref('Id'), 'item_id': ref('Id'),
    'state': enum('committed', 'already_owned', 'rejected'), 'charged': integer(),
    'wallet_revision': ref('Revision')})
define('OwnedItem', {'item_id': ref('Id'), 'grant_id': ref('Id'),
    'item_revision': ref('Revision'), 'acquired_at': ref('Time'),
    'asset_status': enum('available', 'retired', 'quarantined')})
page('Inventory', 'OwnedItem')
define('Entitlement', {'entitlement_id': ref('Id'), 'kind': ref('Code'),
    'valid_from': ref('Time'), 'valid_until': ref('Time'), 'verified_until': ref('Time'),
    'state': enum('active', 'expired', 'revoked', 'unconfirmed')})
page('Entitlements', 'Entitlement')
define('SupportLinkBegin', {'provider': enum('patreon'),
    'expected_revision': ref('Revision')})
define('ProviderEvent', {'provider': enum('fluxer', 'retroachievements', 'patreon'),
    'instance': string(256, format='uri', pattern=r'^https://'),
    'provider_event_id': string(256), 'identity_link_id': ref('Id'),
    'kind': enum('achievement_verified', 'membership_verified', 'support_period_verified',
    'support_period_revoked'), 'provider_evidence_id': ref('Id'),
    'effective_from': ref('Time'), 'effective_until': ref('Time'),
    'provider_product_ref': string(256), 'community_ref': ref('Id'),
    'achievement_ref': string(256)},
    ('effective_until', 'provider_product_ref', 'community_ref', 'achievement_ref'),
    allOf=[
        {'if': {'properties': {'kind': {'const': 'support_period_verified'}}},
         'then': {'required': ['provider_product_ref', 'effective_until'],
                  'properties': {'provider': enum('patreon')}}},
        {'if': {'properties': {'kind': {'const': 'support_period_revoked'}}},
         'then': {'required': ['provider_product_ref'],
                  'properties': {'provider': enum('patreon')}}},
        {'if': {'properties': {'kind': {'const': 'membership_verified'}}},
         'then': {'required': ['community_ref', 'effective_until'],
                  'properties': {'provider': {'const': 'fluxer'}}}},
        {'if': {'properties': {'kind': {'const': 'achievement_verified'}}},
         'then': {'required': ['achievement_ref'],
                  'properties': {'provider': {'const': 'retroachievements'}}}},
    ],
    description='Internal normalized adapter output, never an unverified public webhook body.')

define('PolicyGate', {'key': ref('Code'), 'value_revision': ref('Revision'),
    'decision_ref': string(256)}, description='References reviewed server-owned policy; not executable config.')
define('DefinitionPublish', {'definition_id': ref('Id'), 'expected_revision': ref('Revision'),
    'policy_gates': array(ref('PolicyGate'), 32, 1), 'reason': ref('Reason')})
define('Correction', {'expected_revision': ref('Revision'), 'reason': ref('Reason'),
    'replacement_evidence_id': ref('Id')}, ('replacement_evidence_id',))
define('Job', {'job_id': ref('Id'), 'kind': ref('Code'), 'state': enum('queued', 'leased',
    'completed', 'dead_letter'), 'attempt': integer(0, 100), 'revision': ref('Revision')})
page('Jobs', 'Job')
define('AuditEvent', {'audit_id': ref('Id'), 'actor_id': ref('Id'), 'action': ref('Code'),
    'resource_id': ref('Id'), 'reason': ref('Reason'), 'created_at': ref('Time')})
page('Audit', 'AuditEvent')

# Internal event variants: consumers are fixed, payloads never contain credentials.
EVENTS = {
    'account.session_revoked': ('accounts', ['operations'], 'ExpectedRevision'),
    'account.deletion_requested': ('accounts', ['profiles', 'operations'], 'ExpectedRevision'),
    'result.verified': ('results', ['boards', 'competitions', 'rewards'], 'ResultReceipt'),
    'result.voided': ('results', ['boards', 'competitions', 'rewards'], 'ResultReceipt'),
    'competition.finalized': ('competitions', ['rewards'], 'Competition'),
    'competition.corrected': ('competitions', ['boards', 'rewards'], 'Competition'),
    'reward.approved': ('rewards', ['wallet', 'inventory'], 'Reward'),
    'reward.reversed': ('rewards', ['wallet', 'inventory'], 'Reward'),
    'purchase.committed': ('wallet', ['inventory'], 'Purchase'),
    'support.period_changed': ('support', ['inventory', 'rewards'], 'ProviderEvent'),
    'avatar.recipe_changed': ('profiles', ['operations'], 'AvatarRecipe'),
    'inventory.ownership_changed': ('inventory', ['profiles'], 'OwnedItem'),
}
event_variants = []
for event_type, (producer, consumers, payload) in EVENTS.items():
    name = ''.join(word.title() for word in re.split(r'[._]', event_type)) + 'Event'
    payload_ref = ref(payload)
    state = {'result.verified': 'verified', 'result.voided': 'voided',
             'competition.finalized': 'finalized', 'competition.corrected': 'finalized',
             'reward.approved': 'issued', 'reward.reversed': 'reversed',
             'purchase.committed': 'committed'}.get(event_type)
    if state:
        constraints = {'properties': {'state': {'const': state}}}
        if event_type == 'result.verified':
            constraints['properties'].update({'proof': enum('participant_consensus',
                'server_replay', 'verified_provider'), 'outcomes': {'minItems': 1}})
        payload_ref = {'allOf': [payload_ref, constraints]}
    if event_type == 'support.period_changed':
        payload_ref = {'allOf': [payload_ref, {'properties': {'kind': enum(
            'support_period_verified', 'support_period_revoked')}}]}
    event_variants.append(define(name, {'event_id': ref('Id'), 'type': {'const': event_type},
        'schema_version': {'const': 1}, 'aggregate_id': ref('Id'),
        'aggregate_revision': ref('Revision'), 'operation_id': ref('Id'),
        'occurred_at': ref('Time'), 'payload': payload_ref}))
DEFS['DomainEvent'] = {'oneOf': event_variants}

# Endpoint table is deliberately independent of transport implementation.
OPS = []


def endpoint(module, method, path, operation, response, request=None, audience='shell',
             success=200, rule='', sensitive=False):
    OPS.append({'module': module, 'method': method, 'path': '/v1' + path,
        'operation': operation, 'request': request, 'response': response,
        'audience': audience, 'success': success, 'rule': rule, 'sensitive': sensitive})


endpoint('accounts', 'post', '/auth/login-attempts', 'login_begin', 'LoginAttempt', 'LoginBegin', 'anonymous', 201)
endpoint('accounts', 'post', '/auth/login-attempts/claim', 'login_claim', 'SessionTokens', 'LoginClaim', 'attempt', rule='Possession proof, expiry, consumed state; provider exchange must be complete.')
endpoint('accounts', 'post', '/auth/refresh', 'session_refresh', 'SessionTokens', 'Refresh', 'refresh', rule='Rotate once; concurrent replay revokes family; never blindly retry a consumed refresh.')
endpoint('accounts', 'post', '/auth/reauth-attempts', 'reauth_begin', 'LoginAttempt', 'ReauthBegin', rule='Fresh provider proof; bind purpose/current session/player; no game access.')
endpoint('accounts', 'post', '/auth/reauth-attempts/claim', 'reauth_claim', 'ReauthGrant', 'LoginClaim', rule='One-shot possession claim after fresh proof, same actor/session as begin.')
endpoint('accounts', 'get', '/auth/sessions', 'sessions_list', 'Sessions')
endpoint('accounts', 'delete', '/auth/sessions/{session_id}', 'session_revoke', 'OperationReceipt', rule='Only own session; revokes associated SDK grants.')
endpoint('accounts', 'get', '/me', 'account_get', 'Me')
endpoint('accounts', 'get', '/me/identities', 'identity_list', 'IdentityLinks')
endpoint('accounts', 'post', '/me/identities', 'identity_link_begin', 'LoginAttempt', 'LoginBegin', sensitive=True, rule='Bind existing player; reject external identity already owned by another player.')
endpoint('accounts', 'delete', '/me/identities/{identity_id}', 'identity_unlink', 'OperationReceipt', sensitive=True, rule='Cannot remove last login path without tested recovery.')
endpoint('accounts', 'post', '/me/recovery', 'recovery_rotate', 'RecoveryCodes', 'Empty', sensitive=True)
endpoint('accounts', 'post', '/auth/recovery-attempts', 'recovery_begin', 'LoginAttempt', 'RecoveryBegin', 'anonymous', 201, rule='Rate limit; require code plus fresh provider proof before account rebind.')
endpoint('accounts', 'post', '/me/export', 'account_export', 'OperationReceipt', 'Empty', success=202, sensitive=True)
endpoint('accounts', 'post', '/me/deletion', 'account_delete', 'OperationReceipt', 'ReasonedRevision', success=202, sensitive=True)
endpoint('operations', 'get', '/me/operations/{operation_id}', 'operation_get', 'OperationReceipt', rule='Only actor-scoped receipt, never operation existence oracle.')
endpoint('profiles', 'get', '/players/{player_id}/profile', 'profile_get', 'PublicProfile', audience='public')
endpoint('profiles', 'patch', '/me/profile', 'profile_write', 'PublicProfile', 'ProfileWrite')
endpoint('profiles', 'get', '/me/avatar', 'avatar_get', 'AvatarRecipe')
endpoint('profiles', 'put', '/me/avatar', 'avatar_write', 'AvatarRecipe', 'AvatarWrite', rule='Existing SDK recipe adapter, owned compatible items, expected revision; reject slot duplicates.')
endpoint('registry', 'get', '/games', 'games_list', 'Games', audience='public')
endpoint('registry', 'get', '/games/{game_id}/rulesets', 'rulesets_list', 'Rulesets', audience='game')
endpoint('boards', 'get', '/boards', 'boards_list', 'Boards', audience='public')
endpoint('boards', 'get', '/boards/{board_id}/entries', 'board_entries', 'BoardEntries', audience='game', rule='Cursor bound to board/filter/revision; ties share rank; revoked decisions excluded.')
endpoint('boards', 'get', '/players/{player_id}/statistics', 'statistics_get', 'Statistics', audience='public')
endpoint('results', 'post', '/matches', 'match_create', 'Match', 'MatchCreate', 'game', 201, rule='Caller actor derived from grant; offline_import allowed only by board policy.')
endpoint('results', 'get', '/matches/{match_id}', 'match_get', 'Match', audience='game')
endpoint('results', 'post', '/matches/{match_id}/admissions', 'match_admit', 'Match', 'Admission', 'game', rule='Only own identity; match admission independent of public Lobby announcement.')
endpoint('results', 'post', '/matches/{match_id}/start', 'match_start', 'Match', 'ExpectedRevision', 'game', rule='Host scope plus all admitted identities; freeze roster/rules/policy.')
endpoint('results', 'post', '/matches/{match_id}/abort', 'match_abort', 'Match', 'ReasonedRevision', 'game', rule='Policy checks host or own withdrawal; never unilaterally assign other players a loss.')
endpoint('results', 'post', '/matches/{match_id}/evidence/uploads', 'evidence_begin', 'EvidenceUpload', 'EvidenceBegin', 'game', 201)
endpoint('results', 'put', '/matches/{match_id}/evidence/uploads/{upload_id}/chunks/{chunk_index}', 'evidence_chunk', 'OperationReceipt', 'EvidenceChunk', 'game', rule='Owner+match+upload; index 0..63; same chunk hash idempotent, different bytes conflict.')
endpoint('results', 'post', '/matches/{match_id}/evidence/uploads/{upload_id}/finalize', 'evidence_finalize', 'EvidenceRef', 'Empty', 'game', rule='All lengths/offsets/hash checked; server-approved format; immutable after finalize.')
endpoint('results', 'post', '/matches/{match_id}/submissions', 'result_submit', 'ResultReceipt', 'Submission', 'game', 202, rule='Admission and finalized evidence owner; compare expected match revision; atomic job+receipt.')
endpoint('results', 'get', '/results/{receipt_id}', 'result_get', 'ResultReceipt', audience='game', rule='Participant only; raw evidence not publicly returned.')
endpoint('results', 'get', '/me/results', 'results_list', 'Results')
endpoint('competitions', 'get', '/competitions', 'competitions_list', 'Competitions', audience='public')
endpoint('competitions', 'get', '/competitions/{competition_id}', 'competition_get', 'Competition', audience='public')
endpoint('competitions', 'post', '/competitions/{competition_id}/entries', 'competition_enter', 'Entry', 'CompetitionEntry', rule='Entry pending until every roster member consents and provider/community checks pass.')
endpoint('competitions', 'post', '/competitions/{competition_id}/entries/{entry_id}/consent', 'competition_consent', 'Entry', 'ExpectedRevision', rule='Authenticated member consents only for self; no silent roster inclusion.')
endpoint('competitions', 'delete', '/competitions/{competition_id}/entries/{entry_id}', 'competition_withdraw', 'OperationReceipt', rule='Own entry/delegation; freeze and withdrawal policy.')
endpoint('competitions', 'get', '/competitions/{competition_id}/standings', 'competition_standings', 'BoardEntries', audience='public')
endpoint('competitions', 'post', '/competitions/{competition_id}/disputes', 'dispute_create', 'Dispute', 'DisputeWrite', success=201, rule='Participant, submission window and current decision revision.')
endpoint('community_competitions', 'get', '/communities/{community_id}/competitions', 'community_competitions', 'Competitions', audience='game', rule='Internal community UUID maps to provider+instance+external subject.')
endpoint('community_competitions', 'get', '/competitions/{competition_id}/rosters', 'community_rosters', 'CommunityRosters', rule='Only visible/authorized roster snapshots, not private social graph.')
endpoint('rewards', 'get', '/me/rewards', 'rewards_list', 'Rewards')
endpoint('rewards', 'get', '/me/trophies', 'trophies_list', 'Trophies')
endpoint('wallet', 'get', '/me/wallet', 'wallet_get', 'Wallet')
endpoint('wallet', 'get', '/me/ledger', 'ledger_list', 'Ledger')
endpoint('wallet', 'post', '/me/purchases', 'purchase_create', 'Purchase', 'PurchaseWrite', rule='Host confirmation, exact offer and price; debit+ownership+outbox in one transaction.')
endpoint('inventory', 'get', '/shop/items', 'shop_list', 'ShopItems', audience='public')
endpoint('inventory', 'get', '/me/inventory', 'inventory_list', 'Inventory')
endpoint('inventory', 'get', '/me/entitlements', 'entitlements_list', 'Entitlements')
endpoint('support', 'post', '/me/support-links', 'support_link_begin', 'OperationReceipt', 'SupportLinkBegin', success=202, sensitive=True, rule='Provider-specific verification required; unavailable adapter returns unsupported, not linked.')
endpoint('providers', 'post', '/internal/provider-events', 'provider_event_ingest', 'OperationReceipt', 'ProviderEvent', 'adapter', 202, rule='Normalized verified adapter fact; external webhook shape/signature is provider-owned and not this route.')
for module, resource in [('registry', 'rulesets'), ('boards', 'boards'),
                         ('competitions', 'competitions'), ('rewards', 'reward-rules'),
                         ('inventory', 'shop-offers')]:
    endpoint(module, 'post', '/ops/' + resource + '/{definition_id}/publish',
             module + '_publish', 'OperationReceipt', 'DefinitionPublish', 'operator',
             202, rule='Reviewed immutable definition; all policy gates resolved; DB unique/version checks.', sensitive=True)
endpoint('results', 'post', '/ops/results/{receipt_id}/corrections', 'result_correct',
    'OperationReceipt', 'Correction', 'operator', 202, rule='Append decision; corrections cascade via outbox, never edit scoreboard directly.', sensitive=True)
endpoint('competitions', 'post', '/ops/disputes/{dispute_id}/resolve', 'dispute_resolve',
    'OperationReceipt', 'Correction', 'operator', 202, rule='Authorized reviewer; affected result revision required.', sensitive=True)
endpoint('operations', 'get', '/ops/jobs', 'jobs_list', 'Jobs', audience='operator')
endpoint('operations', 'post', '/ops/jobs/{job_id}/retry', 'job_retry', 'OperationReceipt',
    'ReasonedRevision', 'operator', 202, rule='Bounded retry with lease fencing and domain dedup.', sensitive=True)
endpoint('operations', 'get', '/ops/audit', 'audit_list', 'Audit', audience='operator')
endpoint('operations', 'post', '/ops/auth/reauth-attempts', 'operator_reauth_begin',
    'LoginAttempt', 'OperatorReauthBegin', 'operator', rule='Separate operator identity provider, session and role checks.')
endpoint('operations', 'post', '/ops/auth/reauth-attempts/claim', 'operator_reauth_claim',
    'ReauthGrant', 'LoginClaim', 'operator', rule='Grant only for same operator/session/purpose; never usable with player credentials.')


def openapi():
    def component_refs(value):
        if isinstance(value, dict):
            return {key: (item.replace('#/$defs/', '#/components/schemas/')
                    if key == '$ref' else component_refs(item)) for key, item in value.items()}
        if isinstance(value, list):
            return [component_refs(item) for item in value]
        return value

    paths = {}
    for op in OPS:
        audience = op['audience']
        scheme = 'OperatorBearer' if audience == 'operator' else (
                 'AdapterBearer' if audience == 'adapter' else 'SessionBearer')
        security = [] if audience in ('anonymous', 'public', 'attempt', 'refresh') else [{scheme: []}]
        content = lambda name: {'application/json': {'schema': component_refs(ref(name))}}
        entry = {'operationId': op['operation'], 'tags': [op['module']],
            'summary': op['operation'].replace('_', ' '),
            'description': op['rule'] or 'Draft contract; not an implemented endpoint.',
            'security': security, 'x-audience': audience, 'x-implemented': False,
            'x-sensitive': op['sensitive'], 'parameters': [],
            'responses': {str(op['success']): {'description': 'Draft successful response',
                'content': content(op['response'])}, 'default': {'description': 'Structured error',
                'content': content('Error')}}}
        for name in re.findall(r'\{(\w+)\}', op['path']):
            schema = integer(0, 63) if name == 'chunk_index' else ref('Code' if name == 'game_id' else 'Id')
            entry['parameters'].append({'name': name, 'in': 'path', 'required': True,
                'schema': component_refs(schema)})
        if op['method'] not in ('get', 'head'):
            # Refresh/code exchange are single-use; no receipt stores raw credentials.
            one_shot = op['operation'] in ('login_claim', 'reauth_claim', 'operator_reauth_claim', 'session_refresh', 'recovery_rotate', 'recovery_begin')
            entry['x-retry-class'] = 'one-shot-secret' if one_shot else 'idempotent-command'
            if not one_shot:
                entry['parameters'].append({'name': 'Idempotency-Key', 'in': 'header',
                    'required': True, 'schema': component_refs(ref('Id'))})
        if op['sensitive']:
            entry['parameters'].append({'name': 'X-Reauth-Grant', 'in': 'header',
                'required': True, 'schema': component_refs(ref('Token'))})
        if op['request']:
            entry['requestBody'] = {'required': True, 'content': content(op['request'])}
        if op['response'] in ('SessionTokens', 'RecoveryCodes', 'ReauthGrant'):
            entry['responses'][str(op['success'])]['headers'] = {'Cache-Control': {
                'required': True, 'schema': {'type': 'string', 'const': 'no-store'}}}
        if op['response'] in DEFS and 'next_cursor' in DEFS[op['response']].get('properties', {}):
            entry['parameters'].extend([
                {'name': 'limit', 'in': 'query', 'schema': integer(1, 100)},
                {'name': 'cursor', 'in': 'query', 'schema': string(512)},
            ])
        if op['operation'] in ('statistics_get', 'boards_list', 'competitions_list'):
            entry['parameters'].append({'name': 'game_id', 'in': 'query',
                'required': op['operation'] == 'statistics_get', 'schema': component_refs(ref('Code'))})
        if op['operation'] == 'board_entries':
            entry['parameters'].append({'name': 'around_self', 'in': 'query',
                'schema': {'type': 'boolean'}, 'description': 'Cannot be combined with cursor.'})
        paths.setdefault(op['path'], {})[op['method']] = entry
    return {'openapi': '3.1.1', 'info': {'title': 'TrainerOS Backend — architecture draft',
            'version': DRAFT, 'description': 'Unimplemented design. No production server URL. Provider callbacks and normalized adapter ingress are different contracts.'},
        'jsonSchemaDialect': 'https://json-schema.org/draft/2020-12/schema',
        'x-implemented': False, 'paths': paths,
        'components': {'securitySchemes': {name: {'type': 'http', 'scheme': 'bearer',
            'description': description} for name, description in {
                'SessionBearer': 'Host-owned session or scoped game grant; server derives actor.',
                'OperatorBearer': 'Separate operator audience and roles; no game token.',
                'AdapterBearer': 'Internal per-provider adapter identity after external verification.',
            }.items()}, 'schemas': component_refs(DEFS)}}


# Proposed SDK methods use existing JSON-lines envelope, never HTTP tokens in game calls.
SDK = {
    'account.online.get': ('accounts', 'Empty', 'Me', 'online.account'),
    'results.match.create': ('results', 'MatchCreate', 'Match', 'online.results'),
    'results.match.admit': ('results', 'Admission', 'Match', 'online.results'),
    'results.match.start': ('results', 'ExpectedRevision', 'Match', 'online.results'),
    'results.match.abort': ('results', 'ReasonedRevision', 'Match', 'online.results'),
    'results.evidence.begin': ('results', 'EvidenceBegin', 'EvidenceUpload', 'online.results'),
    'results.evidence.chunk': ('results', 'EvidenceChunk', 'OperationReceipt', 'online.results'),
    'results.evidence.finalize': ('results', 'Empty', 'EvidenceRef', 'online.results'),
    'results.submit': ('results', 'Submission', 'ResultReceipt', 'online.results'),
    'results.get': ('results', 'Empty', 'ResultReceipt', 'online.results'),
    'leaderboards.list': ('boards', 'Empty', 'Boards', 'online.leaderboards'),
    'leaderboards.entries': ('boards', 'Empty', 'BoardEntries', 'online.leaderboards'),
    'competitions.list': ('competitions', 'Empty', 'Competitions', 'online.competitions'),
    'inventory.get': ('inventory', 'Empty', 'Inventory', 'online.inventory'),
}


def sdk_contract():
    methods = {}
    define('SdkPageQuery', {'limit': integer(1, 20), 'cursor': string(512),
        'game_id': ref('Code')}, ('limit', 'cursor', 'game_id'))
    define('SdkBoardQuery', {'limit': integer(1, 20), 'cursor': string(512),
        'around_self': {'type': 'boolean'}}, ('limit', 'cursor', 'around_self'),
        description='cursor and around_self cannot be combined; server/host semantic check')
    for name, (module, params, result, capability) in SDK.items():
        if 'next_cursor' in DEFS[result].get('properties', {}):
            original = DEFS[result]
            paged_fields = dict(original['properties'])
            paged_fields['items'] = dict(paged_fields['items'], maxItems=20)
            sdk_result = 'Sdk' + result
            define(sdk_result, paged_fields, ('next_cursor',))
            result = sdk_result
            params = 'SdkBoardQuery' if name == 'leaderboards.entries' else 'SdkPageQuery'
        context = {'context_revision': ref('Revision')}
        if name.startswith('results.') and name != 'results.match.create':
            context['match_id'] = ref('Id')
        if name in ('results.evidence.chunk', 'results.evidence.finalize'):
            context['upload_id'] = ref('Id')
        if name == 'results.evidence.chunk':
            context['chunk_index'] = integer(0, 63)
        if name == 'results.get':
            context['receipt_id'] = ref('Id')
        if name == 'leaderboards.entries':
            context['board_id'] = ref('Id')
        mutation = name.startswith('results.') and name != 'results.get'
        if mutation:
            context['operation_id'] = ref('Id')
        request_fields = context | {'body': ref(params)}
        request_schema = 'Sdk' + ''.join(part.title() for part in name.split('.'))
        define(request_schema, request_fields)
        methods[name] = {'owner': module, 'capability': capability + ':1',
            'params_schema': request_schema, 'result_schema': result,
            'implemented': False, 'actor': 'active host-owned Trainer, never params',
            'profile_change': 'revoke connection/grants; discard stale callbacks; isolate queued owner'}
    return {'version': DRAFT, 'implemented': False, 'transport': 'existing Platform SDK v1 JSON-lines',
        'max_frame_bytes': 65536, 'max_evidence_chunk_bytes': 16384,
        'methods': methods, 'host_only': ['login', 'refresh', 'link', 'recovery',
        'purchase', 'profile-write', 'avatar-write', 'operator-commands'],
        'unavailable': 'Capability absent until real implementation; unsupported_method, not fake success.'}


def data_model():
    tables = {}

    def table(name, owner, fields, unique=(), fk=None, invariants=()):
        columns = {'id': 'uuid', **fields}
        tables[name] = {'owner': owner, 'columns': columns, 'primary_key': ['id'],
            'unique': [list(key) for key in unique], 'foreign_keys': fk or {},
            'delete_policy': 'explicit-owner-command; no cascading historical erasure',
            'invariants': list(invariants)}

    table('players', 'accounts', {'status': 'player_status', 'created_at': 'utc', 'revision': 'positive_int64'})
    table('external_identities', 'accounts', {'player_id': 'uuid', 'provider': 'code',
        'instance': 'https_uri', 'subject': 'bounded_text', 'verified_at': 'utc'},
        [('provider', 'instance', 'subject')], {'player_id': 'players.id'})
    table('device_sessions', 'accounts', {'player_id': 'uuid', 'token_hash': 'sha256',
        'refresh_hash': 'sha256', 'family_id': 'uuid', 'expires_at': 'utc', 'revoked_at': 'nullable_utc'},
        [('token_hash',), ('refresh_hash',)], {'player_id': 'players.id'},
        ['refresh rotation is atomic; reused credential revokes family', 'never store raw access/refresh tokens'])
    table('login_attempts', 'accounts', {'claim_challenge': 'sha256', 'oauth_state_hash': 'sha256',
        'return_uri': 'nullable_loopback_uri', 'handoff_hash': 'nullable_sha256', 'expected_subject': 'nullable_bounded_text',
        'target_player_id': 'nullable_uuid', 'expires_at': 'utc', 'consumed_at': 'nullable_utc'},
        [('oauth_state_hash',)], {'target_player_id': 'players.id'},
        ['callback bound to provider instance and intended login/link/recovery purpose',
         'no session before provider proof and possession claim; expiry checked server-side',
         'native claim requires original verifier AND one-shot browser handoff; only its hash persists'])
    table('recovery_credentials', 'accounts', {'player_id': 'uuid', 'credential_hash': 'sha256',
        'consumed_at': 'nullable_utc'}, [('credential_hash',)], {'player_id': 'players.id'})
    table('public_profiles', 'profiles', {'player_id': 'uuid', 'display_name': 'text_64',
        'visibility': 'profile_visibility', 'revision': 'positive_int64'}, [('player_id',)], {'player_id': 'players.id'})
    table('avatar_recipes', 'profiles', {'player_id': 'uuid', 'recipe': 'AvatarRecipe',
        'revision': 'positive_int64'}, [('player_id',)], {'player_id': 'players.id'},
        ['expected revision checked atomically', 'preserve SDK v1 colors/presets/pack hash; no local paths'])
    table('games', 'registry', {'game_id': 'code', 'title': 'text_100'}, [('game_id',)])
    table('policy_definitions', 'registry', {'kind': 'code', 'content_digest': 'sha256',
        'document': 'reviewed_policy_document', 'status': 'definition_status', 'revision': 'positive_int64'},
        [('kind', 'content_digest')], invariants=['policy body schema owned by kind', 'unresolved mandatory policy gates prevent publication'])
    table('validators', 'registry', {'artifact_digest': 'sha256', 'runtime': 'code',
        'status': 'definition_status', 'resource_policy_id': 'uuid'}, [('artifact_digest',)],
        {'resource_policy_id': 'policy_definitions.id'}, ['operator-approved binary only; no submitted code/URL'])
    table('game_rulesets', 'registry', {'game_ref': 'uuid', 'rules_digest': 'sha256',
        'content_digest': 'sha256', 'validator_id': 'uuid', 'policy_id': 'uuid', 'status': 'definition_status'},
        [('game_ref', 'rules_digest', 'content_digest')],
        {'game_ref': 'games.id', 'validator_id': 'validators.id', 'policy_id': 'policy_definitions.id'})
    table('matches', 'results', {'ruleset_id': 'uuid', 'host_player_id': 'uuid',
        'competition_id': 'nullable_uuid', 'state': 'match_state', 'revision': 'positive_int64',
        'origin': 'online_or_offline_import', 'deadline': 'utc'},
        fk={'ruleset_id': 'game_rulesets.id', 'host_player_id': 'players.id', 'competition_id': 'competitions.id'},
        invariants=['frozen roster/rules after start', 'admission requires each participant proof',
                    'offline import eligibility checked independently of client timestamp'])
    table('match_participants', 'results', {'match_id': 'uuid', 'player_id': 'uuid',
        'team_slot': 'nullable_uint16', 'status': 'admission_status'}, [('match_id', 'player_id')],
        {'match_id': 'matches.id', 'player_id': 'players.id'})
    table('evidence_refs', 'results', {'match_id': 'uuid', 'player_id': 'uuid',
        'digest': 'sha256', 'byte_length': 'uint32', 'format': 'code',
        'state': 'upload_state', 'expires_at': 'utc'},
        fk={'match_id': 'matches.id', 'player_id': 'players.id'},
        invariants=['<=1048576 bytes, <=64 decoded chunks of <=16384 bytes',
                    'finalize only after all offsets, lengths and digest verify'])
    table('evidence_chunks', 'results', {'upload_id': 'uuid', 'chunk_index': 'uint16',
        'digest': 'sha256', 'byte_length': 'uint32'}, [('upload_id', 'chunk_index')],
        {'upload_id': 'evidence_refs.id'}, ['same slot/different digest conflicts; immutable finalized upload'])
    table('submissions', 'results', {'match_id': 'uuid', 'player_id': 'uuid', 'evidence_id': 'uuid',
        'operation_id': 'uuid', 'payload_digest': 'sha256', 'state': 'result_state'},
        [('player_id', 'operation_id')], {'match_id': 'matches.id', 'player_id': 'players.id', 'evidence_id': 'evidence_refs.id'},
        ['evidence owner and match must equal submission owner and match', 'submission + job + receipt one transaction'])
    table('result_decisions', 'results', {'submission_id': 'uuid', 'match_id': 'uuid',
        'decision_revision': 'positive_int64', 'state': 'result_state', 'proof': 'proof_level',
        'reason': 'text_500', 'outcomes': 'ResultOutcome[]'}, [('match_id', 'decision_revision')],
        {'submission_id': 'submissions.id', 'match_id': 'matches.id'},
        ['append only; match-level final decision reconciles all submissions', 'no client-authored verified state'])
    table('board_definitions', 'boards', {'ruleset_id': 'uuid', 'policy_id': 'uuid',
        'competition_id': 'nullable_uuid', 'definition': 'Board', 'revision': 'positive_int64'},
        fk={'ruleset_id': 'game_rulesets.id', 'policy_id': 'policy_definitions.id', 'competition_id': 'competitions.id'})
    table('best_records', 'boards', {'board_id': 'uuid', 'subject_key': 'sha256',
        'decision_id': 'uuid', 'metrics': 'int64[]', 'watermark': 'positive_int64'},
        [('board_id', 'subject_key')], {'board_id': 'board_definitions.id', 'decision_id': 'result_decisions.id'},
        ['subject is canonical roster or player; no display-name identity', 'voiding best exposes next eligible historical result'])
    table('statistics', 'boards', {'player_id': 'uuid', 'game_ref': 'uuid',
        'counts': 'Statistics', 'watermark': 'positive_int64'}, [('player_id', 'game_ref')],
        {'player_id': 'players.id', 'game_ref': 'games.id'}, ['bot wins do not increase unique human opponents'])
    table('rating_events', 'boards', {'player_id': 'uuid', 'decision_id': 'uuid',
        'policy_id': 'uuid', 'calculation_revision': 'positive_int64', 'before': 'int64', 'after': 'int64'},
        [('player_id', 'decision_id', 'policy_id', 'calculation_revision')],
        {'player_id': 'players.id', 'decision_id': 'result_decisions.id', 'policy_id': 'policy_definitions.id'},
        ['deterministic replay ordering and checkpoints; corrections invalidate downstream affected calculations'])
    table('competitions', 'competitions', {'ruleset_id': 'uuid', 'policy_id': 'uuid',
        'state': 'competition_state', 'opens_at': 'utc', 'closes_at': 'utc',
        'finalizes_at': 'utc', 'revision': 'positive_int64'},
        fk={'ruleset_id': 'game_rulesets.id', 'policy_id': 'policy_definitions.id'},
        invariants=['opens <= closes <= finalizes; state guards server-owned', 'policy immutable after publish'])
    table('competition_entries', 'competitions', {'competition_id': 'uuid', 'subject_key': 'sha256',
        'subject': 'Subject', 'status': 'entry_status'}, [('competition_id', 'subject_key')],
        {'competition_id': 'competitions.id'})
    table('entry_consents', 'competitions', {'entry_id': 'uuid', 'player_id': 'uuid',
        'consented_at': 'utc'}, [('entry_id', 'player_id')],
        {'entry_id': 'competition_entries.id', 'player_id': 'players.id'}, ['each member signs for self; changes invalidate consent set'])
    table('fixtures', 'competitions', {'competition_id': 'uuid', 'round_number': 'uint16',
        'slot': 'uint16', 'match_id': 'nullable_uuid', 'state': 'fixture_state'},
        [('competition_id', 'round_number', 'slot')], {'competition_id': 'competitions.id', 'match_id': 'matches.id'})
    table('disputes', 'competitions', {'competition_id': 'uuid', 'decision_id': 'uuid',
        'claimant_player_id': 'uuid', 'state': 'dispute_state', 'reason': 'text_500'},
        fk={'competition_id': 'competitions.id', 'decision_id': 'result_decisions.id', 'claimant_player_id': 'players.id'})
    table('community_refs', 'community_competitions', {'provider': 'code', 'instance': 'https_uri',
        'subject': 'bounded_text'}, [('provider', 'instance', 'subject')])
    table('roster_snapshots', 'community_competitions', {'community_id': 'uuid',
        'entry_id': 'uuid', 'subject': 'Subject', 'provider_evidence_id': 'uuid',
        'verified_at': 'utc', 'valid_until': 'utc'}, fk={'community_id': 'community_refs.id',
        'entry_id': 'competition_entries.id', 'provider_evidence_id': 'provider_events.id'},
        invariants=['frozen for competition; live social role changes do not rewrite past fixtures'])
    table('delegations', 'community_competitions', {'community_id': 'uuid', 'player_id': 'uuid',
        'competition_id': 'uuid', 'valid_until': 'utc', 'revoked_at': 'nullable_utc'},
        fk={'community_id': 'community_refs.id', 'player_id': 'players.id', 'competition_id': 'competitions.id'})
    table('reward_claims', 'rewards', {'rule_id': 'uuid', 'source_event_id': 'uuid',
        'player_id': 'uuid', 'state': 'reward_state'}, [('rule_id', 'source_event_id', 'player_id')],
        {'rule_id': 'policy_definitions.id', 'player_id': 'players.id', 'source_event_id': 'event_index.id'})
    table('trophies', 'rewards', {'claim_id': 'uuid', 'kind': 'code', 'expires_at': 'nullable_utc',
        'state': 'trophy_state'}, [('claim_id', 'kind')], {'claim_id': 'reward_claims.id'})
    table('wallet_accounts', 'wallet', {'player_id': 'nullable_uuid', 'kind': 'wallet_kind',
        'balance': 'int64', 'revision': 'positive_int64'}, [('player_id', 'kind')], {'player_id': 'players.id'},
        ['one player coin wallet; unique system issuance/sink accounts via partial unique index',
         'player available >=0; pending reversal debt policy unresolved, cannot silently overdraw'])
    table('ledger_transactions', 'wallet', {'source_kind': 'code', 'source_id': 'uuid',
        'created_at': 'utc', 'reverses_id': 'nullable_uuid'}, [('source_kind', 'source_id')],
        {'reverses_id': 'ledger_transactions.id'}, ['append-only; reversal references original'])
    table('ledger_entries', 'wallet', {'transaction_id': 'uuid', 'account_id': 'uuid', 'delta': 'int64'},
        fk={'transaction_id': 'ledger_transactions.id', 'account_id': 'wallet_accounts.id'},
        invariants=['sum delta per transaction ==0 enforced transactionally/deferred constraint, never a row CHECK',
                    'short account lock; debit+credit+grant atomic'])
    table('cosmetic_items', 'inventory', {'item_key': 'code', 'revision': 'positive_int64',
        'asset_digest': 'sha256', 'compatibility': 'code[]', 'status': 'asset_status'}, [('item_key',)])
    table('shop_offers', 'inventory', {'item_id': 'uuid', 'price': 'uint64_safe_json',
        'revision': 'positive_int64', 'status': 'definition_status'}, [('item_id', 'revision')], {'item_id': 'cosmetic_items.id'})
    table('purchases', 'wallet', {'player_id': 'uuid', 'item_id': 'uuid', 'offer_id': 'uuid',
        'operation_id': 'uuid', 'transaction_id': 'nullable_uuid', 'state': 'purchase_state'},
        [('player_id', 'operation_id')], {'player_id': 'players.id', 'item_id': 'cosmetic_items.id',
        'offer_id': 'shop_offers.id', 'transaction_id': 'ledger_transactions.id'})
    table('ownership_grants', 'inventory', {'player_id': 'uuid', 'item_id': 'uuid',
        'source_id': 'uuid', 'source_kind': 'code', 'acquired_at': 'utc'}, [('player_id', 'item_id')],
        {'player_id': 'players.id', 'item_id': 'cosmetic_items.id'},
        ['source_kind+source_id resolved by owner command; never unvalidated polymorphic FK',
         'permanent ownership independent of subscription status'])
    table('support_links', 'support', {'player_id': 'uuid', 'identity_id': 'uuid',
        'status': 'link_status'}, [('identity_id',)], {'player_id': 'players.id', 'identity_id': 'external_identities.id'})
    table('provider_events', 'providers', {'provider': 'code', 'instance': 'https_uri',
        'external_event_id': 'bounded_text', 'link_id': 'uuid', 'payload': 'ProviderEvent',
        'received_at': 'utc'}, [('provider', 'instance', 'external_event_id')], {'link_id': 'external_identities.id'},
        ['normalized event only after provider-specific verification; unsupported adapter never fabricates proof'])
    table('support_periods', 'support', {'support_link_id': 'uuid', 'provider_event_id': 'uuid',
        'valid_from': 'utc', 'valid_until': 'utc', 'verified_until': 'utc'},
        fk={'support_link_id': 'support_links.id', 'provider_event_id': 'provider_events.id'})
    table('entitlement_grants', 'inventory', {'player_id': 'uuid', 'source_period_id': 'uuid',
        'kind': 'code', 'valid_from': 'utc', 'valid_until': 'utc'}, [('player_id', 'source_period_id', 'kind')],
        {'player_id': 'players.id', 'source_period_id': 'support_periods.id'})
    table('operation_receipts', 'operations', {'actor_namespace': 'code', 'actor_id': 'uuid',
        'operation_kind': 'code', 'operation_id': 'uuid', 'payload_digest': 'sha256',
        'state': 'operation_state', 'resource_id': 'nullable_uuid'},
        [('actor_namespace', 'actor_id', 'operation_kind', 'operation_id')],
        invariants=['actor resolved by credential audience, never request ID',
                    'same key/different digest conflicts; no tokens/recovery secrets cached in receipt'])
    table('jobs', 'operations', {'kind': 'code', 'resource_id': 'uuid', 'attempt': 'uint16',
        'fence': 'positive_int64', 'lease_until': 'nullable_utc', 'state': 'job_state'},
        invariants=['job creation atomic with domain command; fenced completion only by current lease'])
    table('event_index', 'operations', {'aggregate_id': 'uuid', 'aggregate_revision': 'positive_int64',
        'event_type': 'code', 'created_at': 'utc'},
        invariants=['minimal dedup tombstone survives payload retention; privacy cleanup separately governed'])
    table('outbox', 'operations', {'event_id': 'uuid', 'payload': 'DomainEvent', 'created_at': 'utc'},
        [('event_id',)], {'event_id': 'event_index.id'},
        invariants=['insert in same transaction as source mutation', 'no provider side effects in database transaction'])
    table('consumer_receipts', 'operations', {'consumer': 'code', 'event_id': 'uuid',
        'applied_at': 'utc'}, [('consumer', 'event_id')], {'event_id': 'event_index.id'},
        ['receipt and projection effect atomic; preserve dedup tombstones beyond outbox retention'])
    table('audit_events', 'operations', {'actor_namespace': 'code', 'actor_id': 'uuid',
        'action': 'code', 'resource_id': 'uuid', 'reason': 'text_500', 'created_at': 'utc'},
        invariants=['append only; no raw credentials/evidence'])
    return {'version': DRAFT, 'implemented': False, 'kind': 'logical-schema-not-migration',
        'tables': tables, 'cross_table_invariants': [
            'actor membership, evidence ownership and source-kind relation checked in domain transaction',
            'no foreign-key ownership alone proves current authorization',
            'deletion anonymizes retained history, not cascade deletion of ledger',
            'wire bounded integers must be checked before int64 persistence',
            'provider and result correction revisions are monotonic; stale events cannot restore revoked rights',
        ]}


# Keep the already published wardrobe recipes in generated contracts as well.
define('AvatarAppearanceV2', {
    **DEFS['AvatarAppearanceV1']['properties'], 'schema': {'const': 2},
    'top': {'enum': ['tank', 'polo-short-sleeve', 'polo-sleeveless', 'everyday-tee',
                     'sundress', 'racing-suit', 'rose-racing-suit']},
    'bottom': {'enum': ['base-shorts', 'golf-shorts', 'pleated-skirt', 'denim-breeches']},
    'socks': {'enum': ['none', 'ankle-socks', 'crew-socks']},
    'shoes': {'enum': ['sandals', 'golf-shoes-classic', 'golf-shoes-sport',
                       'canvas-shoes', 'racing-boots', 'rose-racing-boots']},
    'headwear': {'enum': ['none', 'golf-cap', 'golf-visor', 'racing-helmet', 'rose-racing-helmet']},
})
define('AvatarAppearanceV3', {
    **DEFS['AvatarAppearanceV2']['properties'], 'schema': {'const': 3},
    **{name: string(7, 7, pattern=r'^#[0-9a-fA-F]{6}$')
       for name in ('socksColor', 'shoesColor', 'headwearColor')},
})
DEFS['AvatarPeerRecipeV1']['properties']['appearance'] = {
    'oneOf': [ref(f'AvatarAppearanceV{version}') for version in (1, 2, 3)]}


def documents():
    sdk = sdk_contract()
    # Preserve the published definition order after SDK adds its schemas.
    for name in ('AvatarAppearanceV2', 'AvatarAppearanceV3'):
        DEFS[name] = DEFS.pop(name)
    schemas = {'$schema': 'https://json-schema.org/draft/2020-12/schema',
        'title': 'TrainerOS Backend contract definitions — unimplemented draft', '$defs': DEFS}
    return {'schemas.json': schemas, 'openapi.json': openapi(), 'sdk.json': sdk,
        'data-model.json': data_model(),
        'policy-gates.json': {'version': DRAFT, 'implemented': False,
            'default': 'A missing/unresolved applicable gate prevents activation, never fabricates success.',
            'gates': [
                {'id': key, 'owner': owner, 'decision': decision, 'blocks': blocks,
                 **({'status': 'verified-provider-only', 'evidence': 'docs/evidence/backend-oauth-live-20261025/README.md'}
                    if key == 'auth.provider_contract' else {'status': 'unresolved-before-activation'})} for key, owner, decision, blocks in [
                    ('auth.provider_contract', 'accounts', 'Exact Fluxer instance endpoints, registered callbacks and PKCE behavior verified.', ['online-login']),
                    ('auth.recovery_policy', 'accounts', 'Lost-all-factors recovery, identity relink and conflict procedure.', ['account-recovery']),
                    ('results.validator_policy', 'results', 'Game-specific evidence, resource budget, determinism and trust level.', ['official-results']),
                    ('boards.rating_formula', 'boards', 'Versioned skill algorithm, provisional period and repeated-opponent abuse policy.', ['skill-rating']),
                    ('competitions.rules', 'competitions', 'Format, attempts, deadlines, ties, no-show/disconnect and dispute window.', ['competition-publication']),
                    ('communities.membership', 'community_competitions', 'Provider membership freshness, delegation, freeze and substitution rules.', ['community-competition-entry']),
                    ('rewards.rates', 'rewards', 'Reward sources, rates, repeat limits, offline timing and subscription multiplier applicability.', ['reward-issuance']),
                    ('wallet.reversal', 'wallet', 'Resolution of revoked rewards already spent; no invented automatic debt/asset seizure.', ['economic-awards']),
                    ('inventory.catalogue', 'inventory', 'Item prices, asset rights, compatibility and exact SDK recipe/inventory mapping.', ['shop-publication']),
                    ('support.provider_contract', 'support', 'Patreon verification and reconciliation; no other support provider is planned.', ['support-entitlements']),
                    ('support.closed_artifacts', 'support', 'Protected artifact source and per-release access grant, separate from public Releases.', ['private-release-channel']),
                    ('operations.retention', 'operations', 'Retention per data class, deletion/export and backup deletion reconciliation.', ['public-account-service']),
                    ('operations.recovery', 'operations', 'Deployment topology, backup destination and measured RPO/RTO.', ['production-deployment']),
                ]]},
        'modules.json': {'version': DRAFT, 'implemented': False, 'owners': MODULES,
            'operations': OPS, 'events': {name: {'producer': producer, 'consumers': consumers,
                'payload_schema': payload, 'delivery': 'at-least-once',
                'dedup': 'consumer+event_id; reject stale aggregate_revision'}
                for name, (producer, consumers, payload) in EVENTS.items()}}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    stale = []
    for name, data in documents().items():
        rendered = json.dumps(data, ensure_ascii=False, indent=2) + '\n'
        target = DEST / name
        if args.check:
            if not target.exists() or target.read_text(encoding='utf-8') != rendered:
                stale.append(name)
        else:
            DEST.mkdir(parents=True, exist_ok=True)
            target.write_text(rendered, encoding='utf-8', newline='\n')
    if stale:
        raise SystemExit('Stale generated contracts: ' + ', '.join(stale))
    print(f'{len(DEFS)} schemas, {len(OPS)} operations, {len(EVENTS)} events, {len(SDK)} SDK methods; draft only')


if __name__ == '__main__':
    main()
