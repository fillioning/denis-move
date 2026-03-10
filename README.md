# Serge

Monophonic West Coast synthesizer inspired by Serge Modular La Bestia III, for
[Ableton Move](https://www.ableton.com/move/), built for the
[move-anything](https://github.com/charlesvestal/move-anything) framework.

## Features

- Monophonic, last-note priority with optional legato and portamento
- Complex Oscillator with waveform morphing (sine/tri/saw/pulse)
- New Timbral Oscillator (NTO) with harmonic selection (odd/both/even)
- Wave Multiplier with harmonic wavefolding
- VCFQ+ 4-pole ladder filter with self-oscillation
- GTO+ dual-slope generator (smooth LFO + stepped S&H)
- 4x8 modulation matrix using Move's 32 pads as CV patch points
- ADSR amplitude envelope triggered by MIDI
- Works standalone or as a sound generator in Signal Chain patches

## Controls

The 32 pads form a modulation matrix — they do NOT trigger notes. MIDI notes
from external sources or Move's sequencer trigger the voice.

| Control | Function |
|---------|----------|
| Pads | Modulation matrix: row = source, col = target, velocity = depth |
| Knob 1 | OSC1 Pitch |
| Knob 2 | OSC1 Timbre (sine→tri→saw→pulse) |
| Knob 3 | OSC2 Pitch |
| Knob 4 | OSC2 Harmonics (odd→both→bell) |
| Knob 5 | Fold Depth |
| Knob 6 | Fold Type (odd→both→even) |
| Knob 7 | Filter Cutoff |
| Knob 8 | Filter Q (self-oscillates at max) |
| Jog wheel | Browse parameter categories |

## Modulation Matrix

```
              Col0    Col1    Col2    Col3    Col4    Col5    Col6    Col7
             OSC1-P  OSC1-T  OSC2-P  OSC2-H  Fold-D  Fold-T  Filt-C  Output
Row 0 OSC:   [  ]    [  ]    [  ]    [  ]    [  ]    [  ]    [  ]    [  ]
Row 1 LFO:   [  ]    [  ]    [  ]    [  ]    [  ]    [  ]    [  ]    [  ]
Row 2 S&H:   [  ]    [  ]    [  ]    [  ]    [  ]    [  ]    [  ]    [  ]
Row 3 Fold:  [  ]    [  ]    [  ]    [  ]    [  ]    [  ]    [  ]    [  ]
```

Press a pad to route that source to that target. Harder press = deeper modulation.

## Building

```
./scripts/build.sh
```

Requires Docker.

## Installation

```
./scripts/install.sh
```

Or install via the Module Store in Move Everything.

## Credits

- **Author:** Vincent Fillion
- **Inspiration:** Serge Modular (La Bestia III, Crocodile, Inori, Edelweiss III)
- **Framework:** [Move Anything](https://github.com/charlesvestal/move-anything) by Charles Vestal

## License

MIT — see [LICENSE](LICENSE)
