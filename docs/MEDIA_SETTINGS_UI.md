# Media and feedback settings — P1

Start → Settings → Media & feedback contains five controller-selected categories:
pictures/video, music/interface sounds, charger vibration, device lighting and
Reduced Motion. Up/Down selects a row. A opens availability details for the first
four; A/B closes details with selection preserved. B from the category list
restores the Media & feedback entry in Settings. L1/R1 remains global navigation.

Reduced Motion uses the existing device-wide preferences repository, including
its pending/error/retry behavior. It is the same setting on both screens, not a
new preference. Theme, Credits, Trainer/accounts and device controls retain their
existing routes. Seven root settings fit within the handheld content rectangle.

The other categories describe actual implementation boundaries. No audio player,
video decoder, pack installer, vibration sender, RGB writer or sensor polling is
introduced. Missing runtime support cannot be mistaken for an enabled toggle or
a successful preview. Static exit pictures and optional Pokédex illustrations
remain separate media identities; this screen does not claim an illustration
pack is installed. Existing per-asset source/credit screens remain authoritative.

P5 retains audio packs/licensing/category levels and playback arbitration,
bounded video, power-edge haptics, verified Armada lighting and gyro/parallax
capabilities. Loading/error/cancellation and reversible hardware previews belong
to those real services when implemented. Pack validation/atomic installation and
the final Pack Studio remain at their existing roadmap gates. This synchronous
availability screen does not invent loading, hardware detection or downloads.

Acceptance: controller list/detail/Back focus, global shoulders, persisted shared
Reduced Motion and failed-write recovery; rendered root/category/detail screens
at handheld dimensions; no hardware or external save changes from browsing.

## Verification — 2026-09-20

Windows passed all 35 tests; ARM64 passed 38 plus the separate persistence-process
test. The extended SDL scenario visits all media categories, details, the shared
motion setting, Back restoration and global shoulders. It also passed in native
Gamescope on Flip without QML warnings. Root, category and detail layouts were
inspected at 960×540 and native 1920×1080. Follow-up spacing changes passed a
focused rendered check; this evidence does not certify the deferred media or
hardware providers.

The final non-testing build was installed with a binary/database rollback copy.
InputPlumber-injected controller events verified production entry, detail and
Back; final spacing was inspected on Flip. Database integrity, Trainer profile,
686 Adventure registrations and the installed binary hash were checked. This
does not substitute for an owner's physical-button test.
