# Interactive website demo — 2026-10-07

The owner requested a mouse-only interactive tour for the website, with a panel
above the interface explaining each screen and optional short animated examples.
This is an explicitly approved, bounded website deliverable before resuming
MP-02. It does not replace or close native runtime work, Pack Studio or the image.

Delivered files and deployment instructions: [web-demo](../web-demo/README.md).
The bundle is standalone static HTML/CSS/JavaScript with local fonts/assets.
It is intentionally independent of QML, devices, game files and communication
accounts. A site can serve it in a subdirectory or iframe without a build step.

## Composition and behavior

- Retain the five primary tabs and peer faces, landscape material chassis,
  current fonts/palette, gold selection, circular Home launch and Choose Adventure.
- Put explanations outside the simulated device; keep routine actions direct.
- Use entirely fictional library/species/account fixtures with original artwork.
- Reflect demo edits across Home, Guide, Party/Boxes, Playroom, basket and profile.
- Show complex flows through user-started scenes with Stop and cancellation on
  manual navigation; never auto-start a tour or require a tour before browsing.
- Keep expected simulation limits visible. No actual emulator, save writer,
  Fluxer call, multiplayer transport or host-system configuration is exercised.

## Evidence and remaining work

The browser smoke check is `tools/test-web-demo.py`; it passed the linked flows,
scene completion/cancellation, persistence/reset, HTML escaping and four viewport
sizes, with no browser errors or failed asset requests. The built-in AbyssDeck
browser was also used for visual inspection. Website captures are kept separately
under `screenshots/web-demo-2026-10-07/`; they are not handheld proof.

Public-site integration/publication is separate: this pass prepares the complete
bundle rather than silently modifying the owner's live site. Native code and
unfinished MP-02 edits are preserved. No console deployment is needed for this
browser-only artifact. Resume the previously accepted MP-02 queue afterward;
all distinct-network, human acceptance and later roadmap gates remain intact.
