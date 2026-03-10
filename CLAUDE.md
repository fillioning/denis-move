# Serge — Claude Code context

## What this is
Monophonic West Coast synthesizer inspired by Serge Modular La Bestia III, for Ableton Move.
Move Anything sound generator. API: plugin_api_v2_t. Language: C.
Voice architecture: monophonic, last-note priority, optional legato/glide.

## Repo structure
- `src/dsp/serge.c` — all DSP logic (5 modules, modulation matrix, ADSR, MIDI, render_block)
- `src/module.json` — module metadata and version (must match git tag on release)
- `scripts/build.sh` — Docker ARM64 cross-compile (always use this)
- `scripts/install.sh` — deploys to Move via scp + fixes ownership
- `.github/workflows/release.yml` — CI: verifies version, builds, releases, updates release.json

## DSP modules
1. **Complex Oscillator** — waveform morphing (sine→tri→saw→pulse), Enc1 pitch + Enc2 timbre
2. **NTO** — harmonic selection (odd→both→even/bell), Enc3 pitch + Enc4 harmonics
3. **Wave Multiplier** — harmonic wavefolding, Enc5 depth + Enc6 type
4. **VCFQ+** — 4-pole ladder filter, self-oscillates at Q=1.0, Enc7 cutoff + Enc8 Q
5. **GTO+** — dual-slope generator: smooth triangle LFO + stepped S&H random

## Modulation matrix (4×8 = 32 pads)
Pads (MIDI notes 68-99) are modulation routing, NOT note triggers.
Row 0: Dual Oscillators → audio-rate feedback
Row 1: GTO Smooth → triangle LFO (0.01-40Hz)
Row 2: GTO Stepped → S&H random
Row 3: Wave Multiplier → folded audio
Col 0-7: OSC1 Pitch, OSC1 Timbre, OSC2 Pitch, OSC2 Harmonics, Fold Depth, Fold Type, Filter Cutoff, Output Level
Pad velocity = modulation depth (0-127 → 0.0-1.0). Pad release = clear routing.

## Parameter categories (5 categories, jog-wheel navigation)
1. Oscillators — OSC1 Pitch, OSC1 Timbre, OSC2 Pitch, OSC2 Harmonics, OSC Mix
2. Wavefold — Fold Depth, Fold Type
3. Filter — Cutoff, Q
4. Envelope — Attack, Decay, Sustain, Release
5. Modulation — GTO Rate, LFO Waveform, Portamento, Legato

## Critical constraints
- NEVER write to `/tmp` — use `/data/UserData/` on device
- NEVER allocate memory in `render_block` — all state lives in the instance struct
- NEVER call printf/log/mutex in `render_block`
- Output path: `modules/sound_generators/serge/` (not audio_fx!)
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
Use the `/move-anything-release` skill.

## Design reference
See `design-spec.md` in the claude projects directory for full design intent and conversation history.
