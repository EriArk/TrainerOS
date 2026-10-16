# Genre cover placeholders

18 original backgrounds generated with the built-in image_gen tool, 2026-10-16.
Exact prompts are in [PROMPTS.json](PROMPTS.json). No third-party image inputs
or recognizable franchise characters/logos were supplied. These are decorative
fallbacks, not official box art or claims about a game's story/content.

The owner requested colourful fantasy motifs with bold silhouettes readable at
small cover size, then extended the set to genre-specific imagery. Detailed
landscapes and plain geometric gradients were superseded before integration.
All final outputs were inspected together at thumbnail size.

- fantasy: six island/castle/moon/arch/crystal/tree motifs;
- racing, sports, scifi, puzzle, action: two alternatives each;
- neutral: two alternatives for absent/unrecognized genre metadata.

`MultiversePresentation` maps catalogue genre strings to broad visual groups.
SHA-256 of immutable Adventure ID and group selects a stable variant, independent
of sorting, filtering, filename and session. No genre is guessed from a title.
Changing genre can change group/variant. Neighbouring games can share a variant.
Real box art always takes precedence; QML supplies title and platform separately.

The PNGs are embedded, decoded asynchronously at bounded display size, with only
visible shelf rows instantiated. This record does not clear the separate #116
artwork review or change the project's existing licensing boundaries.
