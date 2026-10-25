# Live Fluxer OAuth — 25 October 2026

The owner confirmed the client-secret operation in Fluxer. The separate
**TrainerOS** application is owned by the permanent owner account; its sole
registered callback is
`https://traineros-api.abysstail.art/v1/auth/callback/fluxer`.
Public bot invitations are disabled. Actual consent requested only `identify`;
the existing Lobby Directory application was not changed.

Client secret and an independent random Backend key were installed through SSH
stdin in the protected server secret directory, without a local credential file.
The operator's Fluxer password was neither read nor transferred. Server
`deploy/.env` now selects both Compose files with
`COMPOSE_FILE=compose.yaml:compose.oauth.yaml`, preserving OAuth on ordinary
Compose updates. No schema migration was required.

## Browser failure and correction

The first live HTML form submission was rejected before provider authorization.
Database inspection showed `confirming`, zero code failures. The form's
`no-referrer` policy suppresses the Origin of navigation-mode POST requests.
The successful start page now uses `same-origin`; all other responses retain
`no-referrer`. The explicit Origin/cookie/proof checks were preserved; `null`
and foreign origins remain rejected. See [MDN's Origin behavior](https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/Referrer-Policy#effect_on_the_origin_header).

Regression assertions cover the form policy, null/foreign rejection and the
redirect's no-referrer policy. All **34 integration tests passed** on the server
with isolated PostgreSQL and no external network (`tests.txt`). One existing
Starlette/httpx deprecation warning remains. No handheld checks were performed.

## Live acceptance, separate from fixtures

Using the built-in WebView2 browser and the real public HTTPS endpoints:

- Cancel on Fluxer's account chooser returned `login_cancelled`; claim was
  rejected and no session was issued for that attempt.
- Owner account selection and explicit `identify` consent returned successfully
  to the Backend. Real code exchange, PKCE, userinfo and one-time claim succeeded.
- Authenticated `/v1/me` and the own-session list succeeded.
- Refresh rotated credentials; the old access token was rejected, the new one worked.
- Explicit revocation of the temporary Backend session rejected both its access
  and refresh credentials. Social/Fluxer browser sessions were not revoked.
- A second login returned the same internal player identity. Its temporary
  Backend session was also revoked after verification.

`live-check.json` contains only booleans; no user/attempt/session IDs or tokens.
Temporary server verification scripts/state were removed. The stable identity
and audit history remain in the service database. A private server backup was
created after acceptance; off-host encrypted backup remains unconfigured.

`public-check.json` separately records seven read-only ingress checks; its
`live_oauth_proven: false` means that script alone does not prove OAuth. The live
browser acceptance above supplies that evidence. `server-check.json` records the
installed image and unchanged Lobby/Crates service availability.

## Remaining product work

This is Backend OAuth/session acceptance using the interim manual-code bridge.
Secure automatic native return, persistent shell credential storage, SDK grants
and revocation delivery, existing Social/local-Trainer binding and account
recovery/deletion remain open. The owner rejected mandatory manual code entry
for ordinary player login; it is not delivered player UX. No SDK/game/handheld
integration or broad network/load acceptance is implied.
