# Denis — Claude Code context

## What this is
Monophonic West Coast synthesizer inspired by Serge Modular La Bestia III, for Ableton Move.
Schwung sound generator. API: plugin_api_v2_t. Language: C.
Voice architecture: monophonic, last-note priority, optional legato/glide.

## Repo structure
- `src/dsp/denis.c` — all DSP logic (5 modules, modulation matrix, presets, ADSR, MIDI, render_block)
- `src/module.json` — module metadata and version (must match git tag on release)
- `scripts/build.sh` — Docker ARM64 cross-compile (docker create + docker cp pattern)
- `scripts/install.sh` — deploys to Move via scp + fixes ownership
- `.github/workflows/release.yml` — CI: verifies version, builds, releases, updates release.json

## DSP modules
1. **Complex Oscillator** — waveform morphing (sine→tri→saw→pulse), Enc1 pitch + Enc2 timbre
2. **NTO** — harmonic selection (odd→both→even/bell), Enc3 pitch + Enc4 harmonics
3. **Wave Multiplier** — harmonic wavefolding with ADAA, Enc5 depth + Enc6 type
4. **SVF Filter** — Cytomic TPT (LP/BP/HP/Notch), Q 0-4, Enc7 cutoff + Enc8 Q
5. **GTO+** — dual-slope generator: smooth triangle LFO (0.01-500Hz) + stepped S&H random

## Modulation matrix (4×8 = 32 pads, bipolar -1..+1)
Pads (MIDI notes 68-99) are modulation routing, NOT note triggers (in Patch mode).
Row 0: Envelope (ADSR output 0-1, mapped to bipolar with mod_depth_env)
Row 1: GTO Smooth → triangle LFO (0.01-500Hz, audio-rate capable)
Row 2: GTO Stepped → S&H random
Row 3: Filtered Noise → smooth random movement (with mod_depth_noise)
Col 0-7: OSC1 Pitch, OSC1 Timbre, OSC2 Pitch, OSC2 Harmonics, Fold Depth, Fold Type, Filter Cutoff, Output Level
Menu matrix is bipolar (-1..+1). Pad matrix adds positive (velocity-based) on top.

## Per-sample vs per-block modulation
- **Per-sample** (audio-rate LFO support): OSC1 Pitch, OSC2 Pitch, Filter Cutoff
- **Per-block** (128 samples): Timbre, Harmonics, Fold Depth, Fold Type, Output Level

## Presets
30 presets with Québécois joual names. Preset 0 = Init (defaults).
Accessible as enum on Modulation page knob 7, or via menu.
Each preset sets all params + full 4×8 matrix.

## Parameter categories (7 pages, jog-wheel navigation)
1. Oscillators — OSC1 Pitch, Timbre, OSC2 Pitch, Harmonics, Osc Mix, Noise Mix, Noise Type, Cutoff
2. Control — Fold Depth, Fold Type, Cutoff, Q, ADSR + menu: Filter Type, Vel>Filter
3. Modulation — LFO Rate, S&H Rate, Env/Nz Depth, Rnd Serge, Rnd Mod, Preset, Rnd Patch + menu: Matrix Reset, Pad Mode, Portamento, Legato
4-7. Matrix pages — Env, LFO, S&H, Noise → 8 targets each

## Critical constraints
- NEVER write to `/tmp` — use `/data/UserData/` on device
- NEVER allocate memory in `render_block` — all state lives in the instance struct
- NEVER call printf/log/mutex in `render_block`
- Output path: `modules/sound_generators/denis/` (not audio_fx!)
- Files on Move must be owned by `ableton:users` — `scripts/install.sh` handles this
- `release.json` is auto-updated by CI — never edit manually
- Git tag `vX.Y.Z` must match `version` in `src/module.json` exactly
- Block size: 128 frames at 44100 Hz
- Audio format: int16 stereo interleaved
- No FTZ on ARM — denormal guards required

## Build & deploy
```bash
./scripts/build.sh          # Docker ARM64 cross-compile
./scripts/install.sh        # Deploy to move.local
```

## Release
Use the `/schwung-release` skill.

## Design reference
See `design-spec.md` in the claude projects directory for full design intent and conversation history.
