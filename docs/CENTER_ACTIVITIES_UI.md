# Center activities presentation

P1 #54/#55/#45 adds separate Playroom, Practice and Link Counter screen flows. These are presentation rehearsals, not the P5 animation provider, battle engine, Bluetooth transport or P8 save transactions.

## Entry and controls

- Center Party/Storage has an attached Activities control. Down from the last grid row (or the production unavailable action) selects it; A opens the three-entry menu. Up returns to the remembered management slot. Existing X Party/Storage and Select backup shortcuts remain.
- Up/Down chooses an activity; A opens it. B from an activity unwinds its local preview before returning to the menu. B from the menu restores the Activities control and the prior Party/Storage section/slot.
- Sample Playroom uses two fixed actor controls: Left/Right selects, A calls, X pets. The neutral numbered tokens and portrait fallback are original placeholders, not illustration art masquerading as animation sprites. Reactions never modify HP, friendship or any saved field. Call motion is a short finite transition, respects Reduced Motion and uses no continuous update loop.
- Sample Practice has setup and layout-preview states. A enters/leaves the preview; B returns setup → menu. No battle runs, and no damage, turn outcome or reward is fabricated.
- Sample Link has peer, proposal and interruption views. A advances the explicitly labelled rehearsal; interrupted A resets it. B returns to peer selection, then the menu. There is no scan, connection, confirmation of a real trade or claimed transaction success.
- Shared Y keeps the normal selector priority. A different Adventure clears actor/reaction/stage data and returns to the activities menu; cancelling preserves it. L1/R1 remains global and L2/R2 remains Pokedex/Center. Start and other shell/storage overlays retain priority.

## Runtime boundary

`CenterActivities` is a transient QObject owned by PartyPresentation. It depends only on input actions and Qt values; it has no repository, file, network, Bluetooth, save-writer or battle interface. Only the existing non-editable development library enables samples. Production presents each feature's honest unavailable state and a working Back action; input cannot activate sample stages or actors.

Management, backup/restore and activity state remain separate. Entering or leaving an activity cannot create a Pokemon, heal a Party, create a Champion record, write a save or award an achievement. No new persistence schema or owned records are introduced.

## Remaining functional gates

P5 retains real licensed sprite/portrait loading and credits, finite-state Party animation, hidden/in-game pause and sustained Flip performance. P8 retains exact Party observations and identity, generation/hack-specific battle semantics and ARM feasibility, and independent save-write capabilities. Real Link still needs two fake endpoints exercising durable transaction/recovery behavior, then two physical devices and verified exact-build pairs. The interruption screen here is only a layout rehearsal and does not close those recovery gates. In-game currency sales, transfer/evolution rules and all earlier optional commitments remain in the roadmap.

## Acceptance

Check controller entry and management-focus restoration, scene selection/call/pet, Practice setup/preview/cancel, Link review/interruption/reset, Adventure changes and production sample isolation. Inspect 960×540 and native Flip layouts, fixed focus, Reduced Motion and no sprite-art dependency. Existing backup/restore tests must still protect and compare their fixture bytes; these activities introduce no external writes to test.

## Verification — 2026-09-20

Windows native build and all 35 tests passed. ARM64 build on Flip passed 38 tests plus the separately run persistence-process test. The native Gamescope SDL scenario passed menu/actor focus, reactions, Practice preview, Link interruption and restoration of the prior Storage slot, with no QML warnings. Rendered 960×540 and 1920×1080 layouts were inspected. This verifies the isolated presentation, not real Party animation, battles, Bluetooth discovery or save transfers.

The non-testing ARM64 build was installed with a binary/database rollback copy. InputPlumber controller events exercised the production menu, unavailable activity routes and Back flow on Flip. These injected events do not replace an owner's physical-button check.
