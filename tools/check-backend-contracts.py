#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Validate draft artifacts and boundary examples, not a running Backend."""
import base64
import copy
import json
from pathlib import Path
import subprocess
import sys

from jsonschema import Draft202012Validator, FormatChecker
from openapi_spec_validator import validate

ROOT = Path(__file__).resolve().parents[1]
CONTRACTS = ROOT / 'services/backend/contracts'
subprocess.run([sys.executable, str(ROOT / 'tools/build-backend-contracts.py'), '--check'], check=True)
load = lambda name: json.loads((CONTRACTS / name).read_text(encoding='utf-8'))
schemas = load('schemas.json')
api = load('openapi.json')
modules = load('modules.json')
model = load('data-model.json')
sdk = load('sdk.json')
gates = load('policy-gates.json')
Draft202012Validator.check_schema(schemas)
validate(api)


def schema_refs(value):
    if isinstance(value, dict):
        if '$ref' in value:
            target = value['$ref']
            assert target.startswith('#/$defs/'), target
            assert target.removeprefix('#/$defs/') in schemas['$defs'], target
        for child in value.values():
            schema_refs(child)
    elif isinstance(value, list):
        for child in value:
            schema_refs(child)


schema_refs(schemas)
assert not api.get('servers'), 'Draft must not advertise a production URL'
operation_ids = set()
for op in modules['operations']:
    assert op['module'] in modules['owners']
    assert op['operation'] not in operation_ids
    operation_ids.add(op['operation'])
    actual = api['paths'][op['path']][op['method']]
    assert actual['operationId'] == op['operation']
    assert actual['x-implemented'] is False
    if op['audience'] in ('shell', 'game', 'operator', 'adapter'):
        assert actual['security'], op['operation']
    if op['audience'] == 'operator':
        assert actual['security'] == [{'OperatorBearer': []}]
    if op['audience'] == 'adapter':
        assert actual['security'] == [{'AdapterBearer': []}]
    if op['response'] in ('SessionTokens', 'RecoveryCodes', 'ReauthGrant'):
        response = actual['responses'][str(op['success'])]
        assert response['headers']['Cache-Control']['schema']['const'] == 'no-store'
    if actual.get('x-retry-class') == 'idempotent-command':
        assert any(p['name'] == 'Idempotency-Key' and p['required'] for p in actual['parameters'])
    if op['sensitive']:
        assert any(p['name'] == 'X-Reauth-Grant' and p['required'] for p in actual['parameters'])
assert set(modules['owners']) == {op['module'] for op in modules['operations']}
for event in modules['events'].values():
    assert event['producer'] in modules['owners']
    assert all(c in modules['owners'] for c in event['consumers'])
    assert event['payload_schema'] in schemas['$defs']
assert len({gate['id'] for gate in gates['gates']}) == len(gates['gates'])
for gate in gates['gates']:
    assert gate['owner'] in modules['owners']
    assert gate['status'] == 'unresolved-before-activation'
for name, table in model['tables'].items():
    assert table['owner'] in modules['owners'], name
    assert set(table['primary_key']) <= table['columns'].keys(), name
    for unique in table['unique']:
        assert set(unique) <= table['columns'].keys(), (name, unique)
    for column, target in table['foreign_keys'].items():
        other, field = target.split('.')
        assert column in table['columns'], (name, column)
        assert other in model['tables'], (name, target)
        assert field in model['tables'][other]['columns'], (name, target)
        assert table['columns'][column].removeprefix('nullable_') == model['tables'][other]['columns'][field], (name, target)
assert set(modules['owners']) == {table['owner'] for table in model['tables'].values()}
for method in sdk['methods'].values():
    assert method['owner'] in modules['owners']
    assert method['params_schema'] in schemas['$defs']
    assert method['result_schema'] in schemas['$defs']
    assert method['implemented'] is False
    properties = schemas['$defs'][method['params_schema']]['properties']
    assert 'context_revision' in properties
    assert not {'player_id', 'token', 'refresh_token'} & properties.keys()

ID = '11111111-1111-4111-8111-111111111111'
OTHER = '22222222-2222-4222-8222-222222222222'
HASH = 'a' * 64
cases = []


def case(label, schema, value, valid):
    validator = Draft202012Validator(schemas | {'$ref': '#/$defs/' + schema}, format_checker=FormatChecker())
    errors = list(validator.iter_errors(value))
    assert bool(errors) != valid, (label, [error.message for error in errors[:3]])
    cases.append(label)


purchase = {'item_id': ID, 'offer_revision': '1', 'expected_price': 25}
case('purchase valid typed request', 'PurchaseWrite', purchase, True)
case('negative purchase price', 'PurchaseWrite', purchase | {'expected_price': -1}, False)
case('fractional coins', 'PurchaseWrite', purchase | {'expected_price': 0.5}, False)
case('client-selected payer', 'PurchaseWrite', purchase | {'player_id': OTHER}, False)
case('client-selected wallet balance', 'PurchaseWrite', purchase | {'balance': 100000}, False)
case('invalid UUID', 'PurchaseWrite', purchase | {'item_id': 'another-player'}, False)

submission = {'evidence': {'evidence_id': ID, 'sha256': HASH, 'byte_length': 3,
    'format': 'golf.journal.v1'}, 'expected_match_revision': '1'}
case('submission valid reference', 'Submission', submission, True)
case('self-declared verified result', 'Submission', submission | {'verified': True}, False)
case('self-declared score', 'Submission', submission | {'score': 1}, False)
case('evidence over total limit', 'EvidenceBegin', {'byte_length': 1048577, 'sha256': HASH,
    'format': 'golf.journal.v1'}, False)
case('encoded chunk over envelope bound', 'EvidenceChunk', {'data_base64': base64.b64encode(b'x' * 16387).decode()}, False)
case('invalid base64 alphabet', 'EvidenceChunk', {'data_base64': '!!!!'}, False)

chunk = {'context_revision': '1', 'operation_id': ID, 'match_id': ID, 'upload_id': OTHER,
    'chunk_index': 0, 'body': {'data_base64': base64.b64encode(b'x' * 16384).decode()}}
case('SDK maximum valid evidence chunk', 'SdkResultsEvidenceChunk', chunk, True)
frame = {'id': '1', 'method': 'results.evidence.chunk', 'params': chunk}
assert len((json.dumps(frame) + '\n').encode('utf-8')) < sdk['max_frame_bytes']
case('SDK missing context generation', 'SdkResultsEvidenceChunk',
    {key: value for key, value in chunk.items() if key != 'context_revision'}, False)
case('SDK foreign profile claim', 'SdkResultsEvidenceChunk', chunk | {'player_id': OTHER}, False)
case('SDK chunk index overflow', 'SdkResultsEvidenceChunk', chunk | {'chunk_index': 64}, False)
case('SDK oversized page', 'SdkPageQuery', {'limit': 100}, False)
case('SDK bounded page', 'SdkPageQuery', {'limit': 20}, True)

recipe = {'schema': 1, 'packRevision': HASH, 'appearance': {'schema': 1,
    'pack': 'trainer-avatar-1', 'hair': 'swept', 'eyes': 'round',
    'hairColor': '#62402c', 'eyeColor': '#98602d', 'skinColor': '#f0c4a5',
    'shirtColor': '#ec9e8d', 'shortsColor': '#577c87'}}
case('existing SDK avatar recipe including all colors', 'AvatarPeerRecipeV1', recipe, True)
case('avatar local path forbidden', 'AvatarPeerRecipeV1', recipe | {'glb': 'C:/private/avatar.glb'}, False)
case('duplicate team identity', 'Subject', {'kind': 'roster', 'players': [ID, ID]}, False)
case('individual cannot claim team roster', 'Subject', {'kind': 'player', 'players': [ID, OTHER]}, False)

result = {'receipt_id': ID, 'match_id': OTHER, 'state': 'verified', 'proof': 'server_replay',
    'decision_revision': '1', 'outcomes': [{'subject': {'kind': 'player', 'players': [ID]},
    'placement': 1, 'metrics': [24], 'completion': 'completed'}]}
event = {'event_id': ID, 'type': 'result.verified', 'schema_version': 1,
    'aggregate_id': OTHER, 'aggregate_revision': '1', 'operation_id': ID,
    'occurred_at': '2026-10-25T12:00:00Z', 'payload': result}
case('verified event typed payload', 'DomainEvent', event, True)
invalid = copy.deepcopy(event)
invalid['payload']['state'] = 'pending'
case('pending result masquerading as verified event', 'DomainEvent', invalid, False)
invalid = copy.deepcopy(event)
invalid['payload']['proof'] = 'unverified'
case('unverified proof in verified event', 'DomainEvent', invalid, False)
case('unknown event version', 'DomainEvent', event | {'schema_version': 2}, False)
case('invalid UTC event time', 'DomainEvent', event | {'occurred_at': 'yesterday'}, False)
case('undeclared credential in event', 'DomainEvent', event | {'access_token': 'secret'}, False)

provider = {'provider': 'patreon', 'instance': 'https://provider.example',
    'provider_event_id': 'example-1', 'identity_link_id': ID, 'kind': 'support_period_verified',
    'provider_evidence_id': OTHER, 'effective_from': '2026-10-25T12:00:00Z',
    'effective_until': '2026-11-25T12:00:00Z', 'provider_product_ref': 'example-tier'}
case('typed support period fact', 'ProviderEvent', provider, True)
case('support period without expiry', 'ProviderEvent',
    {k: v for k, v in provider.items() if k != 'effective_until'}, False)
case('support period from wrong provider family', 'ProviderEvent', provider | {'provider': 'fluxer'}, False)
case('membership fact without community', 'ProviderEvent', provider | {'provider': 'fluxer', 'kind': 'membership_verified'}, False)
case('membership fact with target community', 'ProviderEvent', provider | {'provider': 'fluxer', 'kind': 'membership_verified', 'community_ref': ID}, True)

# Demonstrate boundaries of shape validation: these require backend semantics.
case('structurally valid forged evidence needs ownership check', 'Submission', submission, True)
case('base64 length alone does not prove decoded size', 'EvidenceChunk',
    {'data_base64': base64.b64encode(b'x' * 16385).decode()}, True)
case('valid shape does not prove actual balance or offer price', 'PurchaseWrite', purchase, True)

print(f'PASS: OpenAPI 3.1, JSON Schema, {len(model["tables"])} logical entities/FKs, '
      f'{len(modules["owners"])} module owners, all operations/events/SDK references; {len(cases)} boundary examples.')
print('NOT PROVEN: authentication, DB transactions, provider calls, semantic authorization, actual server/gameplay, deployment.')
