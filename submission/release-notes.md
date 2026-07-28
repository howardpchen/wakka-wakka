# Version and release notes

## Version plan

- Public release candidate: `1.0.0`

The internal development history remains recorded in the review bundles.

## Draft first-release notes

Initial release of Wakka Wakka for Pebble Time 2 and Pebble Time Steel:

- Wrist-tilt maze controls with buffered turns
- Compact scrolling stage with wraparound tunnel
- Four rivals with distinct pursuit behaviors
- Energy-dot chase reversal and bonus scoring
- Three lives, death animation, ready prompt, and restart flow
- Optional gameplay backlight
- Fully offline play with no account or configuration required
- Release build with profiling displays removed
- Native Emery and Basalt layouts in one PBW

## Internal build note

Version `1.5.1` adds the original launcher/menu icon and submission-prep assets.

Version `1.5.2` replaces the repeated rectangular energy-dot rows with a connected,
single-width corridor network. It preserves the center wrap tunnel while
eliminating dead ends and 2×2 open or energy-dot-filled areas.

Version `1.5.3` adds a centered 8×4 hollow enclosure and a simple rectangular
route around it, while retaining the center wraparound entrances.

Versions `1.6.0` through `1.6.3` improve motion smoothness and separate the HUD
from the scrolling playfield. The public `1.0.0` release candidate removes the
development profiling display. The final public build adds a compact Basalt
layout and physically validated Time Steel support without changing Emery
gameplay.
