# AbyssME directory extension (v2)

2026-10-24. Source implementation for #257, not a deployed service or a claim of
playable AbyssME multiplayer. No new bot, community, credentials or relay.

## Isolation

The current TrainerOS reader in `src/integrations/social/LobbyDirectory.h`
accepts exactly `v: 1`, checks the bot identity and expiry, then the UI filters
by topic/search/free seats. It does not perform a product/runtime filter in the
directory. Actual game preparation belongs to a later runtime stage.

AbyssME uses the existing prefix and channels, with `v: 2`, `product: abyssme`
and `protocol: 1`. Old TrainerOS clients ignore these messages by version; v1
projection remains unchanged. AbyssME must reject v1 and other product/protocol
values. Do not merely append product fields to a v1 advertisement: old clients
would still display it. Other v2 product/protocol combinations are rejected.

The owner explicitly retained **one active public room per account across both
products**. A competing UUID gets `already-listed`; it never replaces the
existing room. The room UUID is bound to its original product as well as owner,
including withdrawal/close tombstones. Mismatched-product operations return
`wrong-product`. This prevents accidental client crossover, not a claim that a
user cannot write a custom command for their own account.

## Wire fields

The command retains `op`, `lobby`, `revision`, `topic`, `status`, `access`,
`seats`, `available` and optional `streams` from v1. The new scope fields must
be present on **all** v2 commands, including withdraw/close. There is no arbitrary
join URL or token field. The server supplies owner/name/expiry as before.

`choosing` rooms have no public game and zero seats/available. Other states
require 2-16 seats and a game with:

- `id`: lowercase SHA-256 content identity, not a filename or title match.
- `title`: public bounded title; `platform`: `j2me`, `mophun` or `symbian`.
- `mode`: lowercase token (maximum 64 characters).
- `adapter`: case-sensitive adapter-version token (maximum 64 characters).

These fields advertise a capability; the service does not verify gameplay.
Readers must match platform/mode/adapter against their actual capability set.
No installed game is needed simply to display a compatible advertisement.
Unknown capabilities must not become a false Join/Ready promise. v2 streams
must be zero until a separately accepted media contract exists.

Only allowlisted fields are projected. Private/invite-only access remains
unlisted. Authenticated DM authorship, membership, revision, TTL, lost-create
recovery and host-authoritative admission retain the existing boundaries.
No automatic friendship or invitation handling is added by this service patch.

## Storage and rollout

Startup adds a `product` column with `traineros` as the default for existing
rows, including historical tombstones. No database deletion/reset or token
rotation is needed. Back up the service code/database before deployment.

Rollback must restore the matching code/database snapshot together: an old
service cannot safely update existing v2 records and would not understand their
ownership scope. Do not deploy over a live service just because unit tests pass.
Before production rollout, exercise v1/v2 publication and withdrawal with two
authorized identities, verify the one-account collision message and old-client
isolation, then keep service rollback available. Android publication is now bound
in unreleased source. Neither this branch nor the earlier v2 patch is deployed.

### Private command acknowledgements

An AbyssME command may include a UUID `request`. After authenticated DM and guild
membership checks and the durable database operation, the bot replies in that DM
with prefix `TrainerOS lobby directory result\n```json\n` and a closing code fence.
The allowlisted result has `v: 2`, `product: abyssme`, `protocol: 1`, authenticated
`owner`, original `request`, `lobby`, `revision`, `op`, a bounded `result` code and
`expires: now + 90`. Legacy v1 commands receive no new reply. No diagnostic body,
credential or arbitrary command field is reflected.

`accepted` means the command is stored, not that reconciliation has made it visible
or that the host admitted anyone. `already-listed` lets the client explain the
cross-product quota. Lost acknowledgement is not silently converted into success:
the Android client offers retry with a higher durable revision. Replayed commands
can receive `stale`; this is not an acknowledgement journal or exactly-once delivery.
The bot is still not a room authority, invitation relay or gameplay server.

## Verification

`py -3 -m unittest discover -s services/lobby-directory -p test_*.py`

19 tests pass: eight legacy cases plus seven v2 cases covering scope/private-field
projection, game validation, unknown versions, cross-product quota, persistent
ownership, out-of-order close and legacy database migration, plus four correlated
result tests (private allowlist, legacy isolation, invalid scope/correlation).
Local SQLite/API
fixtures only; no live bot, Fluxer account, device, media or gameplay acceptance.
