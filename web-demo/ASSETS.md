# Demo asset provenance

This website bundle uses no game ROMs, extracted sprites, commercial screenshots,
game audio, official character art or franchise logos. All game/creature names
and demo dialogue in `data.js` were written for this demonstration. They are not
a statement of trademark availability or legal clearance.

- `assets/companions.png`: a four-character atlas generated with the built-in
  image_gen tool on 2026-10-07, without reference images. The exact prompt and
  source filename are in `assets/companions-generation.json`. CSS selects the four cells;
  a narrow left inset on the top-right cell hides a neighboring atlas edge.
- `valley.webp`, `coast.webp`, `garden.webp`, `library.webp`, `orbit.webp`:
  web-sized encodings of the original environment illustrations generated for
  TrainerOS on 2026-10-07. Source mapping, prompts and hashes are retained in
  the generation record. No game imagery was supplied to their generation.
- `assets/favicon.svg`, interface geometry, crystals, caregiver and animation:
  original code-created UI graphics for this demo.
- `assets/fonts/Fredoka.ttf`, `ChakraPetch-Bold.ttf`, `Bungee-Regular.ttf`:
  existing project fonts, under the SIL Open Font License. All three notices
  are included beside the font files.

The artwork was visually inspected for this demo, but no legal clearance is
claimed. The repository's broader asset review (#116) stays open. The project's
software license does not purport to relicense separately supplied artwork.
