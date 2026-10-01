# Public Fluxer user-client spike (#99)

Evidence date: **2026-10-01**. The native text/session gate is proven; this is
not an installed messenger or a completed voice/netplay integration. Continue
#100/#101 with the existing Social/Home surfaces, then invitations in ROADMAP.

## Reproduced against the public service

Two owner-authorized, email-verified test accounts used the official browser
handoff approval. Initiation and one-use polling used `api_public`; browser
storage/cookies were never read. The new sessions successfully accessed `@me`.
The official browser identified itself as Stable Web **2026.930.191718**.
The deployed server commit is not disclosed; do not equate it with Git main.

The independent C++20/Qt harness then passed:

- HTTPS discovery, both expected verified identities, Gateway v1 READY/RESUMED.
- Reciprocal friendship, opening the pair's DM, sending one synthetic message.
- Recipient-side MESSAGE_CREATE/UPDATE/DELETE, bounded history, reconnect.
- Deletion of the experiment's own message after each successful pair run.
- Private-call eligibility, a muted/deafened voice placement and a returned
  endpoint/token grant, followed by confirmed leave. No media connection,
  microphone, camera, ring request or audio playback was started.
- Explicit logout, Gateway invalidation and HTTP 401 for that revoked session;
  the other account remained accessible. Both new test sessions were then revoked.

The text pair passed on **Windows x64 (Qt 6.11.1)** and in the **Fedora 44
aarch64 build container on Flip (Qt 6.11.2)**. Windows also ran voice-contract
and logout probes. This proves native ARM64 protocol feasibility, not a released
handheld messenger, host-session packaging or two-device online acceptance.
Qt WebSockets was added to the development toolchains, not to the installed
TrainerOS binary or immutable Armada image. No device shell restart was needed.

## Capability matrix

| Capability | Evidence / remaining boundary |
| --- | --- |
| User login/session | **Reproduced:** official browser handoff → new public API user session. Prefer this route in #100. |
| MFA/challenges | **Documented, not exercised:** browser owns its normal MFA/CAPTCHA/IP checks. Direct password flow is unnecessary for this first implementation. Never disable provider checks. |
| Expiry/revocation/logout | **Reproduced:** explicit logout, Gateway invalidation, HTTP 401. Natural expiry, MFA accounts and administrative revocation not exercised. |
| Friends / pending / block | **Reproduced:** request, reciprocal acceptance, friend lists. Block/unblock and rejected/pending UI remain documented-only. |
| DM / group membership | **Reproduced:** ordinary pair DM. Group creation/membership remains documented-only and can require a challenge. |
| Text / history / realtime | **Reproduced:** send, recipient create/edit/delete, 10-message history, Gateway resume. Multi-page history, offline replay gaps and uncertain sends remain #100/#108 tests. |
| Unread / presence | **Documented, not exercised:** read-state routes and Gateway events. No zero-unread/offline fallback on provider errors. |
| Images / uploads / downloads | **Conditional:** public discovery advertises presigned uploads; plan/upload/message attachment contract reviewed. No upload/download probe in this slice. |
| Recorded voice | **Documented, not exercised:** voice-message attachment/flag contract. Recording, codec, waveform, cancellation and playback remain #102. |
| Private voice / group calls | **Partially reproduced:** DM eligibility + voice grant + leave. LiveKit media and group calls remain #103; no audible call is claimed. |
| Small invitations/signalling | **Documented candidate:** normal authenticated messages work. No dedicated invisible application metadata channel is established; normal-client presentation, retention and abuse limits remain #104/#105. |
| Gameplay packets | **Not investigated:** chat or voice grants prove neither permission nor suitability for frame-rate traffic. Keep #105/#106/#107 separate. |

Sources: [authentication](https://docs.fluxer.app/authentication/),
[handoff and session lifecycle](https://docs.fluxer.app/http-api/authentication/),
[relationships](https://docs.fluxer.app/http-api/users/relationships/),
[private channels](https://docs.fluxer.app/http-api/users/private-channels/),
[messages](https://docs.fluxer.app/http-api/messages/),
[Gateway commands](https://docs.fluxer.app/gateway/commands/),
[Gateway overview](https://docs.fluxer.app/gateway/overview/),
[uploads](https://docs.fluxer.app/topics/uploads/),
[voice](https://docs.fluxer.app/voice/),
[calls](https://docs.fluxer.app/http-api/calls/).

## Native integration decision

Use Qt Network + Qt WebSockets with provider objects behind owner-scoped
services. No Node, embedded Chromium, web messenger or bot credential is needed.
Discovery's `api_client` is first-party infrastructure; third-party clients use
`api_public`. The spike explicitly follows only the public discovery redirect,
never credential-bearing redirects. See
[instance discovery](https://docs.fluxer.app/http-api/instance/).

In #100, own the handoff countdown/cancellation and single-use token consumption,
then bind the verified remote user to the selected local Trainer. Keep tokens
out of QML, logs, shared profile exports and plain SQLite fields. Define platform
secret storage and owner-switch/logout cleanup before persistent login. Gateway
resume has a bounded retained history; resynchronise through REST when rejected.
HTTP 429 needs the provider delay; authentication failures must not retry blindly.
The spike is a synchronous disposable console program, not the shell's event-loop
or reconnect implementation. Production needs asynchronous requests, generation
guards, bounded caches and retry policy under #108.

Voice documentation currently specifies LiveKit. The live probe returned the
documented grant shape, but did not negotiate media or establish server versions.
Do not implement a future QUIC protocol from roadmap prose. A candidate is the
[official LiveKit C++ SDK](https://github.com/livekit/client-sdk-cpp), reviewed at
`e6e191b57e1209f2065068f697214235372fb842`: Apache-2.0, native C++ with Rust FFI.
Its build guide describes Linux GCC/Clang and Windows Visual Studio; TrainerOS's
MinGW ABI, ARM64 build, audio devices and actual Fluxer grant/E2EE compatibility
are **unproven**. Review/build this only for #103, not as a text-chat dependency.

## Provider terms and source boundaries

[Public terms](https://fluxer.app/terms) apply to service/API use: ordinary
communication is distinct from unlimited storage or load testing. This spike
used only the owner's consenting accounts and tiny synthetic messages; it
did not enumerate users, bypass account checks or test service capacity.
No SLA, production relay entitlement or unlimited backend capacity is inferred.

Fluxer source reviewed at `7c9564bcadf7e306ae2ddc3ab0d471359c3c029d` carries
[AGPL-3.0](https://github.com/fluxerapp/fluxer/blob/7c9564bcadf7e306ae2ddc3ab0d471359c3c029d/LICENSE).
No app/SDK source was copied or linked. The independently written harness uses
the documented protocol. This does not select a TrainerOS project license;
review notices and dependency terms again before distributing additional SDKs.
Public third-party client endpoints are explicitly documented; this is not a
claim of a separate contractual partnership or approval from Fluxer.

## Reproduction

Build independently of the shell:

```sh
cmake -S tools/research/fluxer-spike -B build/fluxer-spike -G Ninja
cmake --build build/fluxer-spike
build/fluxer-spike/trainer-fluxer-spike --discovery-only
```

Dependencies are C++20, Qt6 Core/Network/WebSockets. Windows needs the matching
Qt runtime/plugin directory on PATH; Fedora uses `qt6-qtwebsockets-devel` in the
build container. No service credentials belong in build flags or command args.

For two **explicitly designated test accounts**, initiate
`POST /v1/auth/handoff/initiate`, privately retain code/poll_secret, and open the
official webapp's `/login?handoff=1`. Select the correct account, enter its code,
inspect the initiating device and approve. Poll the documented POST status route
with the poll secret; retain the token only on first completion. Five-minute
expiry and cancellation are part of the contract. Do not extract browser tokens.

Provide this JSON through a private stdin pipe (placeholders shown):

```json
{"accounts":[{"user_id":"FIRST_TEST_ID","token":"FIRST_SESSION"},
             {"user_id":"SECOND_TEST_ID","token":"SECOND_SESSION"}]}
```

- No flag: validate identities, READY and RESUMED; no friend/message changes.
- `--pair-test`: creates/accepts the test friendship and DM; creates, edits and
  deletes one synthetic message. Friendship/empty DM remain for subsequent tests.
  A failed run can leave its synthetic message; inspect only the test DM before
  retrying. No bulk history cleanup is authorized by this harness.
- `--voice-contract`: uses that pair's DM, verifies eligibility, obtains a muted
  grant and leaves; opens no audio stream.
- `--logout-test`: last step; revokes both supplied sessions, verifies revocation.

The program prints allowlisted step names and numeric HTTP/transport failures,
never tokens, provider bodies, user/channel/message IDs or signed media URLs.
Run against public Fluxer manually, never in unattended CI or with real user
accounts. This is a small connectivity experiment, not a load/reliability test.

## Next acceptance

Implement the native owner-scoped browser-handoff consumer and compact real
Friends/Chats in #100/#101. Preserve origin and game process in Home overlays.
Do not require live voice, media uploads or gameplay relay before text chat.
The unproven matrix cells retain their own gates; #99 is not blanket completion
of #100–110. People search and TrainerOS capability recognition follow the owner
addition in [the acceptance register](EXPANSION_98_112.md#people-search-and-client-compatibility).
