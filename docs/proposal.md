# Wakka Wakka: Pebble Time 2 maze-chase proposal

## Verdict

A scrolling maze-chase game is technically feasible on Pebble Time 2. Emery
provides a 200×228, 64-color display, a 240 MHz-class CPU, a 128 KB app/heap
allowance, and a 6-axis IMU. The main design risk is reliable wrist-tilt input,
not rendering or maze storage.

This should be an original maze-chase game rather than a public clone using
another game's names, characters, maze, sounds, or artwork.

## Product direction

- Use a stage larger than the screen and render a 13–15 tile-wide bird's-eye
  viewport.
- Let the player see several intersections in each direction.
- Keep the player within a central camera safe box rather than rigidly centered.
- Bias the view toward the current direction and ease camera movement.
- Indicate threats just outside the viewport with colored edge warnings.
- Design for short one-to-three-minute watch sessions.

## Controls

Movement continues automatically. Tilt selects a desired direction and buffers
it until the next legal intersection. An opposite-direction request may reverse
immediately.

Sensor processing should include:

- neutral calibration at launch and on demand;
- low-pass filtering;
- a neutral dead zone;
- separate activation and release thresholds;
- 80–150 ms of sustained tilt before accepting a direction;
- a brief lockout after accepting a command;
- rejection of samples marked as contaminated by vibration.

An on-screen arrow should show the buffered turn. Buttons remain available as
an accessibility and emulator fallback.

## Camera

The camera should use a central safe box. It moves only after the player crosses
that box, looks roughly two tiles ahead, eases instead of snapping, and clamps
at stage edges. This reduces motion while preserving useful forward visibility.

The prototype may use a simpler centered easing camera while input feel is
validated, then graduate to the complete safe-box behavior.

## Rendering and architecture

Native Pebble C is preferred for deterministic sensor handling and frame timing.

- Store the maze as a compact tile map.
- Use fixed-point positions rather than floating point.
- Draw through one custom layer.
- Run game logic on a fixed timer and render around 15–20 frames per second.
- Use primitives and a small high-contrast palette.
- Keep game state independent from drawing where practical.
- Persist high score and calibrated preferences in a later milestone.

## Rivals and progression

Begin with one rival and add behavior incrementally:

1. direct pursuit;
2. targeting several tiles ahead;
3. alternating pursuit and retreat;
4. alternating chase timing and energy-dot vulnerable state.

Rivals need only choose direction at intersections. Slightly slower enemies
compensate for the restricted field of view.

## Haptics and feedback

Do not vibrate during normal play because motor vibration contaminates
accelerometer samples. Reserve haptics for paused moments, death, and level
completion, while discarding any marked sensor samples.

## Prototype milestones

1. Scrolling maze, automatic movement, energy dots, and button controls.
2. Calibrated and filtered accelerometer turns with visible buffered direction.
3. Physical-device control test across at least ten consecutive intersections.
4. One rival, collision/restart, and off-screen threat cues.
5. Camera and speed tuning on hardware.
6. Multiple enemies, power state, score persistence, original art, and polish.

The decisive proof is whether buffered tilt produces intentional turns at ten
consecutive intersections on physical hardware. Emulator sensor injection can
verify logic, but not wrist ergonomics.

## Prototype acceptance checks

- Target: Pebble Time 2 / Emery only.
- Launch: game begins paused with calibration guidance.
- SELECT: starts/restarts and pauses/resumes with recalibration.
- UP: toggles the backlight.
- DOWN: south-direction emulator fallback.
- BACK: exits normally.
- Persistence: intentionally absent in the first prototype.
- Phone/network/configuration: none required.
- Layout: player, several intersections, score, lives, and buffered direction
  remain legible on the 200×228 display.
- Evidence: zero-exit build and PBW, Emery emulator screenshot/runtime log, then
  separate physical hardware validation.
