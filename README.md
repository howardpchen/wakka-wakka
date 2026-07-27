# Wakka Wakka

A scrolling maze-chase game for Pebble Time 2, written in native Pebble C.

![Wakka Wakka gameplay](submission/screenshots/02-gameplay.png)

The runner moves automatically while four rivals pursue through the scrolling
maze. Tilt the watch to buffer a turn at the next legal intersection. You have
three lives; SELECT restarts after game over. Large energy dots temporarily
turn the rivals blue so the runner can chase them for bonus points.

The rivals have distinct targeting styles: direct pursuit, forward ambush,
vector pursuit, and a chase/retreat personality.

## Controls

- Tilt: buffer a turn in the dominant direction
- Home / game over SELECT: start or restart
- UP: toggle the gameplay backlight
- DOWN: buffer south (emulator fallback)
- SELECT: pause or resume and recalibrate neutral tilt
- BACK: exit

The arrow above the runner appears only when a buffered turn differs from the
current movement direction. Accelerometer input uses filtering, a dead zone,
hysteresis, and a short dwell to avoid accidental turns.

## Building & running

Requirements:

- Pebble SDK 4.17
- Pebble Tool 5.x
- Emery emulator or a Pebble Time 2 connected through CloudPebble

```sh
python scripts/validate-maze.py
pebble build
pebble install --emulator emery
pebble install --cloudpebble
```

## Target platforms

This release intentionally targets only **emery** (Pebble Time 2).

## Privacy

Wakka Wakka works entirely offline. It has no account, analytics, advertising,
phone-side service, or network access. Accelerometer samples are processed
locally for controls and are neither stored nor transmitted.

## Release artifact

The audited `1.0.0` PBW, store copy, screenshots, and SHA-256 checksums are in
[`submission/`](submission/). Generated local SDK output under `build/` is not
tracked.

## Project layout

```
src/c/           C source for the watchapp
src/pkjs/        PebbleKit JS (phone-side) source, if any
worker_src/c/    Background worker source, if any
resources/       Images, fonts, and other bundled resources
package.json     Project metadata (UUID, platforms, resources, message keys)
wscript          Build rules — usually no need to edit
```

The complete feasibility and product proposal is in
[`docs/proposal.md`](docs/proposal.md).

## Documentation

Full SDK docs, tutorials, and API reference: <https://developer.repebble.com>

## License

Wakka Wakka is available under the [MIT License](LICENSE).
