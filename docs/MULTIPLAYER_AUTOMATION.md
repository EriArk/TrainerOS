# Automatic multiplayer connections: feasibility decision

2026-10-03. Source/API audit against the owner's
[zero-administration requirement](MULTIPLAYER_EXPERIENCE.md#connection-setup-requirement).
This completes the provisioning feasibility review, not a deployed service or
fresh-device/gameplay acceptance. No accounts, networks, infrastructure or device
settings were changed. [ROADMAP](ROADMAP.md) remains the execution queue.

## Recommendation

Keep a single Invite / Accept / Join experience, using each emulator's supported
native transport first. Do not make a consumer VPN account a multiplayer
prerequisite. For a runtime that actually needs a virtual LAN, evaluate a
TrainerOS-operated Headscale service with an isolated client integration as the
first explicit-address overlay pilot. Keep ZeroTier as a conditional alternative
only if Ethernet discovery is necessary. Neither overlay is selected for release.

This is an engineering recommendation from the contracts below, not evidence that
Headscale is faster, universally compatible or already invisible to the user.
There is no reason to replace the working RetroArch relay while making parties
support several players. Overlay adoption still requires comparative gameplay.

## Candidate decisions

| Candidate | Can normal setup be automated? | What the project must operate/resolve | Decision |
| --- | --- | --- | --- |
| Existing native RetroArch LAN/relay | TrainerOS already prepares temporary settings and the guest bridge; no extra VPN enrollment. Existing evidence has explicit limits. | Multi-member coordination, controller slots, recovery and remaining distinct-network proof; service availability still matters. | Retain as the implementation baseline. |
| PPSSPP native relay | Official PPSSPP setup selects the same relay on peers. TrainerOS can supply those settings in the existing isolated config. | Prove an actual match; public relay selection alone does not isolate company parties or guarantee admission control. | Prefer native relay investigation before mandatory overlay. |
| User-managed hosted Tailscale | Auth keys can remove device browser login, but somebody still owns the network and its admission policy. Current OAuth apps are same-tailnet tools, not automatic connectivity between arbitrary users' personal networks. | A managed account/network arrangement, provisioning service, policy and appropriate hosted terms/quotas. | Not the default user journey. Existing developer enrollment is only a lab convenience. |
| Project-operated Headscale + client + relay | OIDC enrollment or one-use preauth keys and administration APIs provide the required building blocks. | Wizard identity/enrollment, authenticated party admission, policy updates, client lifecycle, public control/relay endpoint and operations. | Feasible source-level candidate for one bounded overlay pilot; not a finished integration. |
| Project-operated ZeroTier controller | Controller API can create networks and authorize/revoke nodes without player dashboards. | Broker, device identity, per-party network lifecycle, reachability/relay, component distribution terms and operating costs. | Reserve for demonstrated L2/broadcast need; do not install another daemon speculatively. |

Sources reviewed today: [Tailscale auth keys](https://tailscale.com/docs/features/access-control/auth-keys),
[OAuth apps](https://tailscale.com/docs/features/oauth-apps),
[PPSSPP quickstart](https://www.ppsspp.org/docs/multiplayer/quickstart/),
[Headscale registration](https://headscale.net/stable/ref/registration/),
[OIDC](https://headscale.net/stable/ref/oidc/), and
[ZeroTier controller API](https://docs.zerotier.com/controller/).
Local native-route evidence remains in [RetroArch](EMULATOR_MULTIPLAYER.md) and
[PSP](PSP_MULTIPLAYER.md); no source claim upgrades their acceptance.

## Concrete Headscale integration boundary

Pinned release `v0.29.4`, commit `8106636c7f8d0cf8c9d4fb0beae94415069b768b`:

| Operation | Verified source contract | TrainerOS responsibility |
| --- | --- | --- |
| Create an enrollment identity | `POST /api/v1/user` | Bind to the wizard's authenticated identity; a Trainer display name is not authentication. |
| Enroll without another browser/admin step | `POST /api/v1/preauthkey`; request supports user, reusable, ephemeral and expiration | Server issues a short-lived, single-use key to the intended client. Never embed a reusable administrator credential in the image. |
| Apply admitted-party access | `PUT /api/v1/policy`; `policy.mode: database` is available | One server writer composes current accepted memberships into policy; concurrent parties must not overwrite each other's access. Verify propagation and removal on real clients. |
| Remove a device | `DELETE /api/v1/node/{node_id}` | End the correct enrollment, without touching the owner's unrelated Tailscale installation. Ending a party normally removes its access, not the user's whole account. |

Pinned [API definitions](https://github.com/juanfont/headscale/blob/8106636c7f8d0cf8c9d4fb0beae94415069b768b/proto/headscale/v1/headscale.proto),
[preauth schema](https://github.com/juanfont/headscale/blob/8106636c7f8d0cf8c9d4fb0beae94415069b768b/proto/headscale/v1/preauthkey.proto),
and [configuration](https://github.com/juanfont/headscale/blob/8106636c7f8d0cf8c9d4fb0beae94415069b768b/config-example.yaml).
The moving `stable` docs use some different endpoint/config examples; implement
against the selected release's schema, not copied unversioned commands.

Proposed user flow: wizard account -> automatic device enrollment -> ordinary
Invite/Accept or permitted Join -> server grants that party's game connectivity ->
TrainerOS launches/prepares the runtime -> access is released after departure.
Registration must not grant reachability to every other device. Company chat,
game membership and save-affecting transactions remain separate.

The project still needs a supported account/admission bridge. Public Fluxer user
tokens are not Headscale credentials or a proven OIDC integration; do not send
them to a new service or claim existing Social login solves enrollment. Choose a
supported wizard identity flow and bind it to the consenting activity participants.
Only the product operator configures the provider; the family does not.

Persist the device's own identity across ordinary restart and handle expiration
through the wizard/account recovery path. Shared handheld Trainer switching must
release/re-evaluate party access; the machine's network identity is not proof of
which Trainer is playing. No forced logout, exit-node routing or DNS takeover of
an existing personal Tailscale setup on Odin is acceptable.

An embedded [tsnet](https://tailscale.com/docs/features/tsnet) node could avoid
reconfiguring that existing daemon, but its userspace sockets are not a transparent
network interface for arbitrary unmodified emulators. Prove required TCP/UDP and
address behavior before choosing a bridge or isolated interface. A working tsnet
demo alone does not qualify general emulator networking.
Pin and check the Headscale/client version pair; the earlier inventory's
Tailscale `1.98.8` is not automatically proven against Headscale `0.29.4`.

## Infrastructure and cost boundary

- Native public relays can avoid operating an extra VPN service, but their
  availability, room behavior and acceptable use remain external dependencies.
  A private PSP relay is an option, not something deployed in this pass. Upstream
  [hosting guidance](https://github.com/Kethen/aemu_postoffice/blob/a41cdcaa6ef3e621ea8f1c31de3762da4cd06e64/hosting.md)
  specifies separate TCP coordination/relay ports (27312/27313 by default).
- Headscale needs an operator-maintained control endpoint, identity integration,
  database/config backup and a relay path for failed direct connections. Its
  [DERP documentation](https://headscale.net/stable/ref/derp/) describes an embedded
  relay and STUN; it is disabled by default and needs public reachability. A sole
  relay is also a sole fallback failure point. Free software does not eliminate
  server, traffic and maintenance costs.
- Headscale's own [scope](https://headscale.net/stable/) is small deployments, not
  a demonstrated internet-scale game service. That suits a bounded initial pilot;
  a growing public release needs separate capacity evidence. Its pinned
  [license](https://github.com/juanfont/headscale/blob/8106636c7f8d0cf8c9d4fb0beae94415069b768b/LICENSE)
  is BSD-3-Clause; retain component notices when distributing software.
- Do not assume the previously reported double-router home server is a ready
  public relay. No current reachability was measured here. Headscale's
  [community reverse-proxy guidance](https://headscale.net/stable/ref/integration/reverse-proxy/)
  specifically excludes Cloudflare Proxy/Tunnel for its control protocol. It is
  community-maintained guidance, so a supported direct public endpoint is the
  baseline instead of promising that an ordinary web tunnel solves this.
- Hosted [Tailscale pricing](https://tailscale.com/pricing) has account/plan limits;
  do not treat a personal plan as an unlimited product backend. ZeroTier component
  terms/controller distinctions remain in the [transport audit](MULTIPLAYER_TRANSPORT_AUDIT.md).
  No paid subscription, server purchase or claimed fixed monthly price is part of
  this decision. Budget depends on concurrent sessions, relay traffic and regions;
  collect those measurements before choosing hosting capacity.

## Bounded pilot and exit criteria

No further general VPN survey is needed before implementation. First extend the
existing native-route party foundation; the overlay is conditional, not a blocker
for that work. Before adopting it, one isolated pilot must demonstrate:

1. Two clean clients enroll through the intended wizard/account path with no
   copied tokens, admin approval or player-entered network settings. Record action
   count and elapsed time; administrator-operated API calls alone are insufficient.
2. Admission grants only the intended party/game; a second parallel party remains
   independent. Cancel, departure, membership change and stale requests remove or
   deny access without interrupting another party or the current call.
3. Actual emulator traffic works with the isolated client route. Record direct
   versus relay, connection time, gameplay stalls and background voice; preserve
   existing local DNS, personal VPN, emulator configs and saves.
4. Repeat enrollment/reconnect across process restart and an ordinary network
   change. Compare gameplay against the existing native route on the same title.
   Distinct internet networks, 3-4 players and physical checks remain separate gates.

Local Docker CLI is installed but its Linux daemon was unavailable and WSL listed
no distribution during this pass. No lab was silently substituted with mocked
responses, no Docker installation was started, and no remote service was deployed.
This is source-backed automation feasibility, with live acceptance still pending.
