# Denis

Monophonic West Coast synthesizer inspired by Serge Modular (La Bestia III), built for
[Ableton Move](https://www.ableton.com/move/) using the
[Schwung](https://github.com/charlesvestal/schwung) framework.

Denis brings Serge-style West Coast synthesis to the Move: two oscillators, a triple
wavefolder, a multi-mode resonant filter, an audio-rate LFO, sample-and-hold, and a
4x8 bipolar modulation matrix controlled by the Move's pads.

## Architecture

```
                    +-- Noise (White/Pink/Brown)
                    |
MIDI --> Complex Osc --+-- Mix --> Wave Multiplier --> SVF Filter --> VCA --> Out
         (morph)       |           (triple fold)      (LP/BP/HP/N)    ^
         NTO ----------+                                              |
         (harmonics)                                                ADSR
                                                                     ^
         GTO+ ---- Triangle LFO (0.01-500Hz) ----+                   |
                |-- S&H Random -------------------+--> Mod Matrix ---+
         Filtered Noise --------------------------+    (4x8 bipolar)
         Envelope --------------------------------+
```

### DSP Modules

1. **Complex Oscillator** -- Continuous waveform morphing: sine -> triangle -> sawtooth -> pulse. PolyBLEP anti-aliasing on saw and pulse. Knob 1 (pitch), Knob 2 (timbre/morph).

2. **NTO (New Timbral Oscillator)** -- Additive harmonic selection. Odd harmonics -> full spectrum -> even/bell partials. Independent pitch (+/-2 octaves from MIDI). Knob 3 (pitch), Knob 4 (harmonics).

3. **Wave Multiplier** -- Serge-style triple waveshaper with ADAA (anti-derivative anti-aliasing). Three cascaded stages: upper (soft saturation), middle (West Coast fold), lower (rectified fold). Knob 5 (depth), Knob 6 (odd/even harmonic balance).

4. **SVF Filter** -- Cytomic TPT (Topology-Preserving Transform) state-variable filter. Four modes: Lowpass, Bandpass, Highpass, Notch. Resonance from flat to near self-oscillation (Q 0-400%). Soft-clipping feedback via fast tanh. Knob 7 (cutoff), Knob 8 (Q).

5. **GTO+ (Dual-Slope Generator)** -- Smooth side: triangle LFO from 0.01 to 500 Hz (audio-rate capable). Stepped side: sample-and-hold random at independent rate. Both available as mod sources.

### Modulation Matrix

The 4x8 modulation matrix is the heart of Denis. Four modulation sources route to eight
synthesis targets, with bipolar depth (-100% to +100%).

```
              Pitch1  Timbre  Pitch2  Harm    Fold    FType   Cutoff  Level
Envelope:     [    ]  [    ]  [    ]  [    ]  [    ]  [    ]  [    ]  [    ]
LFO:          [    ]  [    ]  [    ]  [    ]  [    ]  [    ]  [    ]  [    ]
S&H:          [    ]  [    ]  [    ]  [    ]  [    ]  [    ]  [    ]  [    ]
Noise:        [    ]  [    ]  [    ]  [    ]  [    ]  [    ]  [    ]  [    ]
```

**Sources:**
- **Envelope** -- ADSR output (bipolar, scaled by Env Depth)
- **LFO** -- GTO+ triangle oscillator (0.01-500 Hz)
- **S&H** -- GTO+ sample-and-hold random (independent rate)
- **Noise** -- Filtered noise (smooth random, scaled by Nz Depth)

**Targets:**
- **Pitch1** -- Complex Oscillator pitch (per-sample, audio-rate)
- **Timbre** -- Complex Oscillator waveform morph
- **Pitch2** -- NTO pitch (per-sample, audio-rate)
- **Harm** -- NTO harmonic selection
- **Fold** -- Wave Multiplier depth
- **FType** -- Wave Multiplier odd/even balance
- **Cutoff** -- SVF filter frequency (per-sample, audio-rate)
- **Level** -- Output amplitude

Per-sample modulation on Pitch1, Pitch2, and Cutoff enables true audio-rate FM and
filter modulation for metallic, clangy, and vocal timbres.

### Pad Modes

Denis has two pad modes, selectable from the Mod & Patch menu:

- **Patch Mode** (default): The first pad you press plays a note. While holding it,
  subsequent pads route modulation in the matrix -- the pad's position maps to a
  [row, column] cell, and press velocity sets the modulation depth. Release the pads
  to clear the routing.

- **Play Mode**: All pads play notes normally (last-note priority, monophonic). The
  matrix is controlled only via the menu knobs on the Mat: pages.

## Parameters

### Page 1: Oscillators

| Knob | Parameter | Range | Description |
|------|-----------|-------|-------------|
| 1 | Osc1 Freq | 0-100% | Detune +/-1 octave from MIDI note |
| 2 | Timbre | 0-100% | Waveform morph: sine -> tri -> saw -> pulse |
| 3 | Osc2 Freq | -2 to +2 | Octave offset from MIDI note |
| 4 | Harmonics | 0-100% | Odd -> both -> even/bell partials |
| 5 | Osc Mix | 0-100% | Blend between Complex Osc and NTO |
| 6 | Noise Mix | 0-100% | Crossfade oscillators with noise |
| 7 | Noise Type | Enum | White, Pink, Brown |
| 8 | Cutoff | 0-100% | Filter cutoff (30 Hz - 20 kHz) |

### Page 2: Control

| Knob | Parameter | Range | Description |
|------|-----------|-------|-------------|
| 1 | Fold Depth | 0-100% | Wavefolding amount (off at 0) |
| 2 | Fold Type | 0-100% | Odd/even harmonic balance |
| 3 | Cutoff | 0-100% | Filter cutoff frequency |
| 4 | Q | 0-400% | Filter resonance, self-oscillates at max |
| 5 | Attack | 0.001-3s | Envelope attack time |
| 6 | Decay | 0.001-3s | Envelope decay time |
| 7 | Sustain | 0-100% | Envelope sustain level |
| 8 | Release | 0.001-3s | Envelope release time |

**Menu params:** Filter Type (LP/BP/HP/Notch), Vel>Filter (velocity to cutoff)

### Page 3: Mod & Patch

| Knob | Parameter | Range | Description |
|------|-----------|-------|-------------|
| 1 | LFO Rate | 0.01-500 Hz | Triangle LFO speed (audio-rate capable) |
| 2 | S&H Rate | 0.01-500 Hz | Sample-and-hold rate (independent) |
| 3 | Env Depth | 0-100% | Envelope modulation source scaling |
| 4 | Nz Depth | 0-100% | Noise modulation source scaling |
| 5 | Rnd Denis | Action | Randomize synth parameters |
| 6 | Rnd Mod | Action | Randomize modulation matrix |
| 7 | Reset Matrix | Action | Clear all matrix routings |
| 8 | Rnd Patch | Action | Randomize everything |

**Menu params:** Preset, Pad Mode (Play/Patch), Portamento (0-2s), Legato (Off/On)

### Pages 4-7: Modulation Matrix

Each page controls one row of the matrix (8 knobs = 8 targets):

- **Mat: Env** -- Envelope -> [Pitch1, Timbre, Pitch2, Harm, Fold, FType, Cutoff, Level]
- **Mat: LFO** -- LFO -> [Pitch1, Timbre, Pitch2, Harm, Fold, FType, Cutoff, Level]
- **Mat: S&H** -- S&H -> [Pitch1, Timbre, Pitch2, Harm, Fold, FType, Cutoff, Level]
- **Mat: Nz** -- Noise -> [Pitch1, Timbre, Pitch2, Harm, Fold, FType, Cutoff, Level]

All matrix knobs are bipolar: -100% to +100%.

## Presets

30 presets with Quebec joual names:

| # | Name | Character |
|---|------|-----------|
| 0 | Init | Clean default |
| 1 | Tabarnak | Heavy aggressive wavefold lead |
| 2 | Poutine Grasse | Fat detuned bass |
| 3 | Elvis Gratton | Cheesy over-the-top vibrato lead |
| 4 | Frette en Criss | Cold icy highpass pad |
| 5 | Slush Brune | Dirty muddy texture |
| 6 | Ti-Coune | Thin nasal bandpass lead |
| 7 | Grande Seduction | Massive full-spectrum drone |
| 8 | Bon Cop Bad Cop | Dual-osc aggressive |
| 9 | C.R.A.Z.Y. | Random chaos modulation |
| 10 | Depanneur | Quick convenience pluck |
| 11 | Caline de Bine | Sweet gentle pad |
| 12 | Maudite Toune | Catchy annoying lead |
| 13 | Les Boys | Hockey organ |
| 14 | Nanane | Candy bright sweet |
| 15 | Tiguidou | Balanced, everything's perfect |
| 16 | Pantoute | Nothing at all, minimal sine |
| 17 | Asteure | Punchy immediate percussive hit |
| 18 | Enfirouape | Complex deceptive modulation |
| 19 | Quetaine | Kitschy vibrato |
| 20 | Cruiser l'Main | Slow filter sweep pad |
| 21 | Broue | Excited fast LFO everything |
| 22 | J'me Souviens | Nostalgic warm pad |
| 23 | Chez Schwartz | Smoky warm tones |
| 24 | Ratoureux | Sneaky subtle modulation |
| 25 | Ostie d'Lead | Aggressive screaming lead |
| 26 | Niaiseux | Silly random bouncy |
| 27 | Guerre des Tuques | Battle: noise, harsh, fast |
| 28 | Magane | Worn out, broken, extreme |
| 29 | Starbuck | Rich abundant lush |

## Additional Features

- **Analog Drift** -- Subtle random pitch drift on both oscillators for analog warmth
- **Pitch Bend** -- +/-2 semitones via MIDI pitch bend
- **Portamento** -- Adjustable glide time (0-2 seconds)
- **Legato** -- When enabled, held notes don't retrigger the envelope
- **Velocity to Filter** -- MIDI velocity opens the filter cutoff
- **Soft Saturation** -- Output runs through a tanh saturator for analog character
- **Shift Detection** -- Reads Schwung shared memory for Shift button state

## Building

Requires Docker for ARM64 cross-compilation.

```bash
./scripts/build.sh          # Cross-compile for Move
./scripts/install.sh        # Deploy to move.local via SSH
```

## Installation

Install via the [Schwung Module Store](https://github.com/charlesvestal/schwung), or
download the latest release from the
[Releases](https://github.com/fillioning/denis-move/releases) page and extract to
`/data/UserData/schwung/modules/sound_generators/denis/` on your Move.

## Credits

- **Author:** Vincent Fillion
- **Inspiration:** Serge Modular (La Bestia III, Crocodile, Inori, Edelweiss III)
- **Framework:** [Schwung](https://github.com/charlesvestal/schwung) by Charles Vestal

## License

MIT
