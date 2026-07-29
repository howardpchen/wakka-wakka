# Wakka Wakka App Store upgrade prep

This directory preserves the original 1.0.0 release artifact and the exact
1.1.0 upgrade materials published to the RePebble App Store.

## Contents

- `listing.md` — short/long descriptions, controls, and compatibility
- `release-notes.md` — release history and 1.1.0 upgrade notes
- `support.md` — support fields requiring public contact confirmation
- `privacy.md` — privacy statement
- `branding-review.md` — originality and IP-risk review
- `assets/` — original 512×512 artwork plus 144×144 large and 80×80 current
  small App Store icons; a 48×48 legacy small icon is also included
- `screenshots/` — existing Emery listing screenshots
- `artifacts/wakka-wakka-1.0.0.pbw` — preserved original Emery release
- `artifacts/wakka-wakka-1.1.0.pbw` — published dual-target upgrade

## Publication

Version `1.1.0` is published at
<https://apps.repebble.com/d7730c67fb89412e9c655cfd> with native `emery` and
`basalt` payloads. The Basalt listing uses a screenshot captured from the exact
release candidate running on a physical Pebble Time Steel.

The published PBW is byte-for-byte identical to
`artifacts/wakka-wakka-1.1.0.pbw`; its SHA-256 checksum is recorded alongside
the artifact.
