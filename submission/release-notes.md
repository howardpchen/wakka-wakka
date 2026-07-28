# Version and release notes

## Version plan

- Existing public release: `1.0.0`
- Prepared upgrade: `1.1.0`

The internal development history remains recorded in the review bundles.

## App Store upgrade notes — 1.1.0

Adds Pebble Time and Pebble Time Steel support while retaining Pebble Time 2:

- New compact 144×168 Basalt layout
- Time Steel-tuned home, HUD, and ready typography
- Physical Time Steel buttons, tilt gameplay, and smoothness validated
- One PBW now includes native Basalt and Emery payloads
- Existing Time 2 controls and presentation remain unchanged
- Fully offline play with no account or configuration required

## Original release — 1.0.0

Initial Pebble Time 2 release with wrist-tilt buffered turns, scrolling maze,
four distinct rivals, energy-dot chase reversal, three lives, optional
backlight, pause/recalibration, and fully offline play.

## Internal build note

Version `1.5.1` adds the original launcher/menu icon and submission-prep assets.

Version `1.5.2` replaces the repeated rectangular energy-dot rows with a connected,
single-width corridor network. It preserves the center wrap tunnel while
eliminating dead ends and 2×2 open or energy-dot-filled areas.

Version `1.5.3` adds a centered 8×4 hollow enclosure and a simple rectangular
route around it, while retaining the center wraparound entrances.

Versions `1.6.0` through `1.6.3` improve motion smoothness and separate the HUD
from the scrolling playfield. The public `1.0.0` release removes the development
profiling display. Version `1.1.0` adds a compact Basalt layout and physically
validated Time Steel support without changing Emery gameplay.
