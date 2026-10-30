# Oxide — tape saturator (Audio Unit for Logic Pro)

A two-knob tape saturation plugin in the spirit of Massey TapeHead:
**Drive**, **Trim**, a three-position **Normal / Medium / Bright** tone switch, and
IN/OUT needle VU meters (true VU ballistics: 300 ms integration, 0 VU = −18 dBFS,
scale linear in voltage) with a peak LED on the output (lights at ≥ −0.3 dBFS).
Built with JUCE 8 and CMake; produces an AU (plus a Standalone app for testing,
and optionally a VST3).

```
Source/TapeSaturator.h   pure C++ DSP core (no JUCE dependency, unit-testable)
Source/PluginProcessor.* AU/host glue, parameters, 4x oversampling, state
Source/PluginEditor.*    custom-drawn GUI (knobs, switch, faceplate)
CMakeLists.txt           JUCE project
```

## Building on macOS

Requirements: Xcode (or Command Line Tools), CMake ≥ 3.22.

```bash
# 1. Get JUCE next to the project (or let CMake fetch it automatically)
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git

# 2. Configure + build (universal arm64/x86_64, Release)
cmake -B build -G Xcode
cmake --build build --config Release --target Oxide_AU
```

`COPY_PLUGIN_AFTER_BUILD` is on, so the component is copied straight into
`~/Library/Audio/Plug-Ins/Components/Oxide.component`.

To build a VST3 as well: add `-DOXIDE_BUILD_VST3=ON` to the configure step.

## Getting Logic Pro to see it

```bash
# Validate (Logic only lists AUs that pass this)
auval -v aufx Oxid Ankm

# If Logic was already open, make it rescan:
killall -9 AudioComponentRegistrar
```
Then relaunch Logic. It appears under **Audio Units → Ankenman Audio → Oxide**
(Logic Pro → Settings → Plug-in Manager if you need to force a rescan).

Code signing: a local build runs fine ad-hoc signed on your own machine. If
Logic refuses to load it on Apple Silicon, sign it once:
```bash
codesign --force --deep -s - ~/Library/Audio/Plug-Ins/Components/Oxide.component
```

## Signal path

```
in → drive gain → pre-emphasis shelf → asymmetric tanh → DC block
   → de-emphasis shelf → HF roll-off → makeup (−¼ of drive dB) → trim → out
```
All of that runs at 4× oversampling (JUCE half-band polyphase IIR), and the
oversampler latency is reported to the host so Logic compensates it.

| Tone   | Pre-emphasis into saturator | De-emphasis after | HF roll-off |
|--------|-----------------------------|-------------------|-------------|
| Normal | +2 dB @ 2.5 kHz             | full (flat)       | 16.5 kHz    |
| Medium | +5 dB @ 3.2 kHz             | 85 %              | 20 kHz      |
| Bright | +8 dB @ 4 kHz               | 65 % (net +2.8 dB @ 16k) | none  |

Drive maps 0–100 % onto 0–36 dB of gain with a soft curve (more resolution in
the subtle range). The asymmetric bias term adds even harmonics and grows with
drive, so low Drive is mostly 3rd-harmonic "tape", high Drive gets fuller and
grittier.

Measured with a −18 dBFS sine (161 Hz), Normal tone:

| Drive | gain    | output    | H2       | H3       |
|-------|---------|-----------|----------|----------|
| 0 %   | 0 dB    | −18.0 dBFS| −114 dBc | −70 dBc  |
| 25 %  | +4.6 dB | −14.5     | −59      | −60      |
| 50 %  | +12 dB  | −8.7      | −39      | −45      |
| 75 %  | +23 dB  | −2.1      | −28      | −25      |
| 100 % | +36 dB  | −1.1      | −31      | −11      |

## Tweaking the sound

Everything voicing-related lives in `Source/TapeSaturator.h`:

- `toneProfile()` — the three tone settings (shelf gain/frequency, how much
  de-emphasis, roll-off).
- `kKnee` — how hot the signal must be before it bends (0.5 = a 0 dBFS sine at
  Drive 0 only just rounds over). Lower = cleaner.
- `saturate()` — the transfer curve itself; swap the tanh for something else
  (e.g. a Langevin/arctan or a hysteresis model) and nothing else needs to change.
- `driveToDb()` / `kMaxDriveDb` — the Drive knob taper and range.
- `makeup` in `process()` — the `-0.25f` exponent sets how much louder Drive gets.
- `VuMeter::kZeroVuDbfs` in `PluginEditor.h` — meter calibration (−18 dBFS default; use −14 or −20 if you prefer).

## Testing the DSP core without JUCE

`TapeSaturator.h` has no dependencies, so a harmonic-analysis test compiles in
one line:
```bash
g++ -std=c++17 -O2 -ISource test/dsp_test.cpp -o dsp_test && ./dsp_test
```

## License note

JUCE is dual-licensed (GPLv3 / commercial). For personal use under GPLv3 you
can add `JUCE_DISPLAY_SPLASH_SCREEN=0` to the compile definitions; otherwise
JUCE shows its splash on first GUI open.
