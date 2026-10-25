# Public player profiles

Accounts owns public presentation keyed by permanent Player UUID. Migration 0006
adds public_profiles. External subjects, email, credentials and local paths never
enter this projection. Verified OAuth userinfo supplies global_name, falling back
to username; NFC normalization, control removal and a 64-codepoint limit apply.
[Fluxer identify contract](https://fluxerapp-fluxer.mintlify.app/authentication).
Provider credentials remain one-shot: renames refresh at the next OAuth sign-in,
not at TrainerOS session refresh. Existing accounts show Player until sign-in.
Names are not unique identifiers or authentication inputs.

GET /v1/me/profile requires an active session. PUT /v1/me/profile/avatar accepts
exactly `{avatar: recipe | null}` for that session's player. Identical requests
do not increment revision; null clears appearance. Only the current free SDK
trainer-avatar-1 schema is accepted: hair, eyes and five RGB colors. Unknown keys,
paid/custom packs, URLs and paths are rejected. The SHA-256 packRevision references
an installed pack, not entitlement; SDK rejects mismatches. No image proxy exists.

Host composition supplies the saved local appearance with an owner guard. One
request is in flight, edits coalesce, failures retry after 60 seconds and account
changes fence callbacks. Missing local assets do not delete online appearance.
The latest synchronized device supplies it; no cloud wardrobe merge or remote
replacement of the local model. Games have no write API.

Boards include a profiles map for page members: player_id, display_name, nullable
avatar recipe and independent decimal revision. Profile changes do not alter score
revision, ordering or team identity. Inactive/missing principals return Player and
no avatar; historical scores remain. Reads share the repeatable-read board snapshot
and one batch query; responses are no-store. Full account erasure, a privacy editor
and automatic provider deletion detection remain unimplemented. Clearing appearance
is not account deletion.

Checks: 68 real PostgreSQL/API tests, native account/SDK tests and a four-player
Golf SDK/TLS/HTTP/worker chain. Evidence: docs/evidence/backend-profiles-20261026.
