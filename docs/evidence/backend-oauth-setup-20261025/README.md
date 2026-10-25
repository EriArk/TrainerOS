# OAuth application and public ingress — 25 October 2026

The owner signed into the built-in Fluxer browser with the permanent EriArk
account. A new application named **TrainerOS** was created through its
Applications UI; **TrainerOS Lobby Directory** was not edited. Fluxer requires
password confirmation to regenerate/reveal the client secret. That confirmation
is pending in the owner's browser; no password was requested in chat or read.
The callback is not yet saved, no OAuth secret is installed, and provider login
remains disabled. Application creation does not prove a live OAuth round trip.

Dedicated server tunnel and user service now serve
`https://traineros-api.abysstail.art`. Only `/v1/*` and `/health/live` reach the
loopback API. Readiness, OpenAPI and unknown non-API paths return 404 externally.
The separate backend, Crates tunnel and Lobby directory services were active
after setup. No changes to the existing Lobby application/tunnel configuration,
runtime image, database schema, SDK, games or handhelds were made.

Validation: `python3 deploy/check-public.py https://traineros-api.abysstail.art`
passed seven read-only requests; output is retained in `public-check.json`.
An initial probe using Python's default User-Agent received edge HTTP 403;
the explicit TrainerOS client User-Agent and curl reached the API. No Cloudflare
security policy was relaxed. This is one ingress check, not broad network/device
acceptance. Cloudflared configuration validation also passed.

Deployment templates: `services/backend/deploy/cloudflared.example.yaml` and
`traineros-backend-tunnel.service`. Real tunnel credentials/configuration remain
on the server and are not included here. The source runtime remains at
`6c5ec8f1b06da10936c23e3f542d83ae5d0f5f50`; this increment adds deployment files/docs.

Accepted player UX is Fluxer sign-in/consent followed by native return and shared
shell-managed sessions. The existing manual-code confirmation is interim, not
the accepted ordinary UX. Secure native return, persistence/SDK binding and live
consent/cancel/relogin remain unimplemented or unverified respectively.
