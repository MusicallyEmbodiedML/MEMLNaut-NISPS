# MEMLNaut MIDI Control Modes — Implementation Notes

## Erica Synths Resonant Filterbank (`MEMLNautModeEricaFBank`)

**File:** `modes/MEMLNautModeEricaFBank.hpp`  
**Enable:** `#define MODE_ERICA_FBANK` in `MEMLNaut-NISPS.ino`

### Device Overview
10-band analog stereo filterbank. Each band is a bandpass filter at a fixed frequency. Bands can self-oscillate at high resonance (no-input mixer style). Physical MIDI channel is configured on the device via CONFIG → MIDI.

Filter frequencies: 29 Hz, 61 Hz, 115 Hz, 218 Hz, 411 Hz, 777 Hz, 1.5 kHz, 2.8 kHz, 5.2 kHz, 11 kHz

### MIDI Implementation
Source: official V1 manual PDF (downloaded from product page, file_id 650).

CCs **70–83** are **active-mode dependent** — the same CC number controls a different parameter depending on which mode the Erica device is currently in.

| CC | Filterbank (FB) mode | Filter mode | CLK MOD mode | DYN EQ mode |
|----|----------------------|-------------|--------------|-------------|
| 70 | Band 1 (29 Hz)       | Cutoff      | Mod Gain     | Threshold   |
| 71 | Band 2 (61 Hz)       | Slope       | Waveshape    | Ctrl Decay  |
| 72 | Band 3 (115 Hz)      | Width       | Frequency    | Ctrl Attack |
| 73 | Band 4 (218 Hz)      | Type        | Step Dir     | Env Delay   |
| 74 | Band 5 (411 Hz)      | Gain        | BPM/Scale    | Filter Freq |
| 75 | Band 6 (777 Hz)      | Offset      | Band Offset  | Audio Src   |
| 76 | Band 7 (1.5 kHz)     | —           | R Ch Invert  | Band Offset |
| 77 | Band 8 (2.8 kHz)     | —           | —            | —           |
| 78 | Band 9 (5.2 kHz)     | —           | —            | —           |
| 79 | Band 10 (11 kHz)     | —           | —            | —           |
| 80 | Band Low Gain        | —           | —            | —           |
| 81 | Band Mid Gain        | —           | —            | —           |
| 82 | Band High Gain       | —           | —            | —           |
| 83 | Band All Gain        | —           | —            | —           |

**Universal CCs (always active, both stereo channels):**

| CC | Parameter      |
|----|----------------|
| 84 | Resonance Level |
| 85 | Feedback CH 1  |
| 86 | Feedback CH 2  |
| 87 | Feedback CH 3  |
| 88 | Feedback CH 4  |
| 89 | Feedback CH 5  |
| 90 | Feedback CH 6  |
| 91 | Feedback CH 7  |
| 92 | Feedback CH 8  |
| 93 | Feedback CH 9  |
| 94 | Feedback CH 10 |
| 95 | Feedback CH All |
| 96 | Mix (Dry/Wet)  |
| 97 | Spread         |

The device also supports separate MIDI channels for left and right stereo channels (MIDI IN L CH / MIDI IN R CH), configurable in the device menu.

### Mode Implementation
- **Params:** 16 (`TRxSAudioApp<16>`)
- **Default CCs:** 70–79 (10 individual bands), 80–82 (Band Low/Mid/High grouped), 84 (Resonance), 96–97 (Mix/Spread)
- **MIDI channel:** 1 (default; change on device via CONFIG → MIDI)
- **Notes:** not passed through (FX unit, not a pitch instrument)
- **CC throttle:** 30 ms
- **Flash save file:** `/erica_fbank_cc.bin`

### Focus Groups
| Group | CCs | Frequency range |
|-------|-----|-----------------|
| LOW   | 70–72 | 29–115 Hz |
| MID   | 73–76 | 218 Hz – 1.5 kHz |
| HIGH  | 77–79 | 2.8–11 kHz |
| GRP   | 80–83 | Grouped band gains |
| RESO  | 84–95 | Resonance + feedback channels |
| GLOB  | 96–97 | Mix + spread |

Plus **HOLD** button (freezes CC output).

### Usage Notes
- The CC options list includes all four modes (FB / Filter / CLK MOD / DYN EQ) with mode prefix labels (e.g. `"FB B1 29Hz"`, `"Flt: Cutoff"`, `"CLK: Mod Gain"`, `"DYN: Threshold"`). Since CCs 70–83 are mode-dependent on the device, pick the labels matching the mode the Erica unit is currently in.
- The device MIDI OUT port can be set to MIDI THRU or MIDI OUT (sends CC values as you tweak hardware controls). Useful for recording automation into a DAW.

---

## Erica Synths × 112dB Steampipe (`MEMLNautModeSteampipe`)

> **Superseded (Oct 2026):** the CC tables below were inferred and do not match the device's own MIDI CC MAP list. See "Erica Steampipe CC map" in the Paramsite section for the current table, which both this mode and Paramsite now use.

**File:** `modes/MEMLNautModeSteampipe.hpp`  
**Enable:** `#define MODE_STEAMPIPE` in `MEMLNaut-NISPS.ino`

### Device Overview
8-voice polyphonic physical modelling synthesizer (Karplus–Strong variant). Models wind and string instruments. 32 CC-controllable parameters organised in four sections: Envelope, Steam (generator), Pipe (resonator), Reverberator.

### MIDI Implementation
Source: V3 manual PDF + firmware UF2 binary analysis.

Factory default CC assignments are **sequential from CC 70**. The Envelope section (CCs 70–74) is **confirmed** from a device OLED screenshot in the manual. All remaining CCs are **inferred** from the sequential pattern and the parameter name list extracted from the firmware string table.

CCs can be remapped on the device via Settings → MIDI → MIDI CC MAP.

#### Envelope section (CCs 70–74) — CONFIRMED
| CC | Parameter |
|----|-----------|
| 70 | Attack *(inferred — row 0 was selected showing "Learn" instead of its number)* |
| 71 | Decay |
| 72 | Sustain |
| 73 | Release |
| 74 | Scaling (note keytrack for envelope times) |

#### Steam / generator section (CCs 75–81) — inferred
| CC | Parameter |
|----|-----------|
| 75 | Velocity (envelope velocity sensitivity) |
| 76 | DC/Noise mix |
| 77 | LPF Cutoff |
| 78 | LPF Resonance |
| 79 | LPF Keytrack |
| 80 | LPF Envelope Amount |
| 81 | Output Gain |

#### Pipe / resonator section (CCs 82–90) — inferred
| CC | Parameter |
|----|-----------|
| 82 | Push (push-pull feedback mechanism) |
| 83 | SoftHard (saturation type) |
| 84 | Symmetry (saturation even/odd harmonics) |
| 85 | Fine Tune (cents) |
| 86 | Harmonics (stiffness / harmonic spread) |
| 87 | Split Point |
| 88 | Feedback Decay |
| 89 | Feedback Release |
| 90 | Patch Volume |

#### Reverberator section (CCs 91–97+) — inferred
| CC | Parameter |
|----|-----------|
| 91 | Dry/Wet |
| 92 | Master Volume |
| 93 | Spread |
| 94 | Room Size |
| 95 | Spin (reverb detuning/richness) |
| 96 | HF Damping |
| 97 | LF Damping |
| 98–101 | Additional params (device has 32 total; these 4 are unconfirmed) |

### Mode Implementation
- **Params:** 16 (`TRxSAudioApp<16>`)
- **Default CCs:** 70, 71, 73 (Env), 77, 78, 80, 81 (Steam), 82, 86, 88, 89 (Pipe), 90, 91, 93, 94, 95 (Pipe Vol + Reverb)
- **MIDI channel:** 1 (default; configurable on device via Settings → MIDI → MIDI IN CH)
- **Notes:** passed through (polyphonic synth — notes sent to device)
- **CC throttle:** 30 ms
- **Flash save file:** `/steampipe_cc.bin`

### Focus Groups
| Group | CCs |
|-------|-----|
| ENV   | 70–74 |
| STEAM | 75–81 |
| PIPE  | 82–90 |
| REVERB| 91–101 |

Plus **HOLD** button (freezes CC output).

### Usage Notes
- **Verify CC numbers on the device:** scroll the Steampipe's MIDI CC MAP menu (Settings → MIDI → MIDI CC MAP) to confirm all 32 assignments. The confirmed range is CCs 70–74 for the Envelope section. Update `kSteampipeCCOptions` if the remaining CCs differ from the sequential pattern.
- **Wind controller MPE setup (from official FAQ):** in the Steampipe MIDI DEVICE menu, set Source A to CC 74, Source B to poly. Then route Source A in the MOD MATRIX to desired destinations (e.g. LPF Modulation, Generator Amount). This is separate from the CC MAP.
- **Note passthrough** is active — the MEMLNaut passes note-on/note-off messages to the Steampipe so it can be played from the joystick-triggered note source.
- **Preset backup:** device supports USB drag-and-drop preset backup (`.spp` files). 32 factory presets included; 192 user slots available.

---

## Waldorf Blofeld (`MEMLNautModeBlofeld`)

**File:** `modes/MEMLNautModeBlofeld.hpp`  
**Enable:** `#define MODE_BLOFELD` in `MEMLNaut-NISPS.ino`

### Device Overview
Waldorf's wavetable/virtual-analogue synth (desktop and keyboard). Three oscillators, two filters, four envelopes, three LFOs, two effects. The desktop model has MIDI **in** only (plus USB), so the MEMLNaut cannot read the current patch back.

### MIDI Implementation
Source: Blofeld User's Manual rev 1.1, Appendix "Controller Numbers" (pp. 113–116). All numbers are **confirmed** from that table.

| CC | Section |
|----|---------|
| 1, 2, 4, 5, 7, 10 | Mod wheel, breath, foot, glide rate, volume, pan |
| 12–14 | Arp range, length, active |
| 15–26 | LFO 1–3: shape, speed, sync, delay |
| 27–34 | OSC 1: octave, semitone, detune, FM, shape, (32 = bank LSB), PW, PWM |
| 35–41 | OSC 2: octave, semitone, detune, FM, shape, PW, PWM |
| 42–48 | OSC 3: octave, semitone, detune, FM, shape, PW, PWM |
| 49–51 | Sync, pitch mod, glide mode |
| 52–62 | Mixer: OSC 1–3, ring mod and noise level/balance, noise colour |
| 65, 67 | Glide active, filter routing |
| 68–78 | Filter 1: type, cutoff, resonance, drive, keytrack, env amount, env velocity, cutoff mod, FM, pan, pan mod |
| 79–89 | Filter 2: same order as filter 1 |
| 90–94 | Amp volume, amp velocity, amp mod, FX 1 mix, FX 2 mix |
| 95–100 | Filter envelope: attack, decay, sustain, decay 2, sustain 2, release |
| 101–106 | Amp envelope: same order |
| 107–118 | Envelopes 3 and 4: same order |

Left out of the options: bank select (0, 32), sustain/sostenuto (64, 66) and 120–127, which change program or hang notes.

Octave CCs only accept 16, 28, 40 … 112 and semitone CCs 52–76; other values are clamped by the Blofeld, so these work but have dead zones.

### Mode Implementation
Follows the TR-8S mode (home values, fade, per-model mapping) plus note pass-through.
- **Params:** 32 (`TRxSAudioApp<32>`)
- **Default CCs:** LFO 1/2 speed; OSC detunes, FM, PW, PWM; OSC 1–3, ring mod and noise levels, noise colour; filter 1 cutoff/res/drive/env; filter 2 cutoff/res/env; FX 1/2 mix; filter envelope ADSR; amp envelope attack/decay/release.
- **Home values:** each CC has a home. **Knob Y** fades from home (0) to the network's output (full). **Knob G** sets the home of the CC under the cursor while the CC Assign page has control. Defaults: centre for bipolar parameters, filters part open, OSC 1/2 at 100.
- **Mapping saved** to `/blofeld_cc.bin` on leaving the CC Assign page, and inside each saved model.
- **MIDI channel:** 1. Set the Blofeld's Global → MIDI Channel to 1 (or omni).
- **Notes:** passed through (a keyboard on the MEMLNaut's MIDI in plays the Blofeld).
- **CC rate:** at most every 30 ms (32 CCs take about 30 ms at 31250 baud); the last values are always flushed.

### Focus Groups
| Group | CCs |
|-------|-----|
| OSC  | 27–49 |
| MIX  | 52–62 |
| FILT | 67–89 |
| FENV | 95–100 |
| AENV | 101–106 |
| MOD  | 15–26 (LFOs), 107–118 (envelopes 3, 4) |
| FX   | 90–94 (amp and effects) |

Performance and arp CCs are in no group, so they are always active.

### Blofeld setup
- Global → **Ctrl Receive = On**.
- Keep Global → **Control W…Z** off the CCs you assign: a CC used by Control W…Z stops reaching its Sound parameter.
- The Blofeld shows CC changes on screen only when you are on the matching edit page.
- The MEMLNaut overwrites the assigned parameters of the current patch (with the home values at boot). Save the patch on the Blofeld if you want to keep a result.

---

## Paramsite — MIDI CC firmware with user setups and synth presets (`MEMLNautModeParamsite`)

**Files:** `modes/MEMLNautModeParamsite.hpp` (engine), `modes/paramsite/ParamsitePresets.hpp` (presets)  
**Enable:** `#define MODE_PARAMSITE` in `MEMLNaut-NISPS.ino`

### Overview
One firmware for MIDI CC control. The standard NISPS controls and input sources (joystick, MIDI CC 1/3/8, machine listening, combined) drive **32 network outputs**, sent as MIDI CCs. No sound of its own.

### Modes
Chosen on the **MIDI Mode** page (after MIDI Channel). Press the encoder, turn to a mode, press again: the choice is saved to `/paramsite_mode.bin` and the MEMLNaut restarts into it. A **Cancel** entry at each end of the list, or leaving the page without the second press, changes nothing.

Knob Y never scales the reward in Paramsite (reward scale stays 1.0); in every mode it is the fade (below). There is no Messages screen.

| Mode | Channel | Focus groups | Keyboard thru |
|---|---|---|---|
| User 1–4 | 1 | 1-8, 9-16, 17-24, 25-32 (by output) + HOLD | no |
| Roland TR-8S | 10 | BD SD LT MT HT RS HC CH OH CC RC FX | no |
| Roland TR-6S | 10 | BD SD LT HC CH OH FX | no |
| Waldorf microQ | 1 | OSC MIX FILT ENV LFO FX + HOLD | yes |
| Waldorf Blofeld | 1 | OSC MIX FILT FENV AENV MOD FX | yes |
| Novation Supernova II | 1 | OSC MIX FILT ENV LFO FX | yes |
| DSI Mopho | 1 | OSC MIX FILT FENV AENV ENV3 + HOLD | yes |
| Erica Steampipe | 1 | ENV STEAM PIPE REVERB + HOLD | yes |
| Erica Filterbank | 1 | LOW MID HIGH GRP RESO GLOB + HOLD | no |
| minilogue xd | 1 | OSC MIX FILT ENV LFO FX + HOLD | yes |
| Bass Station II | 1 | OSC MIX FILT ENV LFO FX + HOLD | yes |
| Prophet Rev2 | 1 | OSC MIX FILT FENV AENV ENV3 FX + HOLD | yes |
| Digitone | 1 | FM FMENV FILT AMP LFO + HOLD | yes |
| MicroFreak | 1 | OSC FILT ENV CYC LFO + HOLD | yes |
| Melbourne Instruments Nina | 1 | OSC MIX FILT ENV LFO FX + HOLD | yes |

Synth presets use the CC tables, defaults and groups of the single-synth modes above, copied unchanged.

### Per mode
- **CC Assign** page: up to 32 CCs. User modes offer CC 1–119 and start on CC 14–31 and 102–115, which have no standard MIDI meaning (so a DAW or plugin does not treat them as volume, pan, expression or mod wheel); presets offer their synth's list. The list is sorted and that is the output order. Outputs without a CC are switched off for training. Saved to flash on leaving the page, and inside each saved model.
- **MIDI Learn** page: turn to choose one of the assigned CCs, press to start. While the page has control, every other CC is frozen and the chosen CC sweeps 0–127 every 2 seconds, so a DAW or plugin's MIDI learn picks up that CC alone. Press again (or leave the page) to stop; all CCs are then resent with the network's values.
- **MIDI Channel** page: 1–16, starting at the mode's default; saved per mode (`/<root>_ch.bin`).
- **Fade** (every mode): each CC has a home; knob Y fades from home (left) to the network (right); knob G sets the home of the CC under the cursor while the CC Assign page has control. Homes default to 0, except Blofeld and Supernova II, which have preset homes.
- **HOLD** (on the Focus page) freezes CC output.
- **Saved models** go in each mode's own SD folder.

### Compatibility with the single-synth firmwares
Each preset keeps the old mode's SD folder, model tag and CC file, so:
- CC setups from the old TR-8S, TR-6S, Blofeld and Erica Filterbank modes load (both file formats). microQ, Mopho and Steampipe use new CC files, because their old tables were wrong (see below).
- Saved models from TR-8S, microQ, Mopho and Blofeld (32 outputs) load; the microQ and Mopho ones were trained on wrong CCs, so retrain them. Models from TR-6S, Steampipe and Erica (16 outputs) are rejected as the wrong size.
- Paramsite always writes the CC file with homes, which the old TR-8S and Blofeld firmwares read but the old TR-6S and Erica Filterbank firmwares (plain CC bytes) do not.

### USB MIDI
Build with `scripts/build.sh -m` (FQBN option `usbstack=tinyusb`). The MEMLNaut then also appears on a computer as a class-compliant USB MIDI port, "MEMLNaut MIDI": everything sent on DIN MIDI out (CCs, notes, clock) is also sent on USB, and notes and CCs received on USB are handled like DIN MIDI in. SysEx is DIN only. Without `-m` the firmware is unchanged (no USB MIDI code is compiled). Implemented in memllib `MIDIInOut` (`PollUSB()`, called from core 0's loop); messages cross cores through queues so only core 0 uses TinyUSB. The USB serial port (debug output) stays available but reconnects once at start-up, when the MIDI port is added.

### Plugins and DAWs
Use a User slot: connect over USB MIDI (or a USB-MIDI interface on DIN), then for each CC go to MIDI Learn, press, and click MIDI learn on the plugin parameter or macro. Mapping to a plugin's macros lets one CC drive several parameters.

### Implementation
- **CC rate:** at most every 30 ms (32 CCs take about 30 ms at 31250 baud); values held back are sent from `loopCore0`.
- **Restart:** `rp2040.reboot()` from `loopCore0`, after the mode file is written. Each mode builds its screens at boot because screens cannot be removed at runtime.
- **Adding a synth:** add a namespace with its CC options, defaults and `ccToGroupMask`, and a row in `kParamsitePresets`.

### minilogue xd, Bass Station II, Prophet Rev2, Digitone, MicroFreak
Each table was typed from the maker's published CC table and checked by script: every CC number matched against the label printed beside it in the manual text.

- **minilogue xd** (54 CCs, 32 defaults). Source: Korg minilogue xd Owner's Manual, MIDI Implementation Chart (v1.22, 2019-12-17). Global > MIDI Rx CC = On.
- **Bass Station II** (54 CCs, 32 defaults). Source: Novation Bass Station II User Guide v5, "MIDI parameters list" (coarse CC of each pair). The CC pairs (e.g. 16:48) are coarse:fine; only the coarse CC is sent.
- **Prophet Rev2** (59 CCs, 32 defaults). Source: Sequential Prophet Rev2 User's Guide 1.2.4, Appendix E, "Additional Continuous Controllers". Global > MIDI Param Receive = CC. CCs edit the layer being edited (A or B).
- **Digitone** (62 CCs, 32 defaults). Source: Elektron Digitone User Manual OS 1.44, Appendix B (track parameters, CC MSB). Set the MIDI Channel page to the track's channel (Digitone Settings > MIDI > Channels). The FX parameters use the separate FX channel and are not included. The manual also lists trig parameters on CC 13-16 (portamento on, filter/LFO trig); they are left out, and CC 13, 14 and 16 (delay send, reverb send, ratio B) are not in the defaults.
- **MicroFreak** (20 CCs, 16 defaults). Source: Arturia MicroFreak User Manual 5.0.1, Appendix D "CC# Values" (default CC numbers). Only the main panel controls have CCs, so 20 are available.
- None tested on hardware yet.

### Melbourne Instruments Nina CC map
43 CCs, 32 defaults, checked by script against the sources. Source: Melbourne Instruments NINA User Manual v1.4, Appendix A (CC 0-63), and NINA v2.0 release notes (effects CCs 44-51; CC 44 was a second Morph before 2.0). Needs firmware 2.0 or later for the effects CCs. Set System > MIDI Echo Filter to Echo Filter if Nina's MIDI out comes back to the MEMLNaut. Nina's knobs are motorised and follow incoming CCs, so they move while the network plays. Nina is 4-part multitimbral; the MIDI Channel page picks the layer.

### Novation Supernova II CC map
From the Supernova II User Manual, "MIDI Controller Table": all 110 sound-parameter CCs (oscillators and their Env 2/3 and LFO 1/2 modulation depths, mixer and ring mods, filter and its modulation, envelopes 1–3, LFOs, distortion/chorus/delay/reverb). Left out: bank select (0/32), arp pattern (3), data entry (6), sustain/latch (64/65), master volume (97), NRPN select (98/99), reverb type (100) and 120+. CCs act on the part on the sending channel, so the MIDI Channel page picks the part. Default homes put fine tunes, pulse widths and modulation depths at centre (assumed bipolar; not stated in the table). Untested on hardware: check that CC 105 moves the cutoff, and that the synth receives controllers.

### Waldorf microQ CC map
Checked row by row against the microQ User's Manual, Appendix "Controller Numbers". The old table was offset by one or two from CC 15 up (e.g. filter 1 cutoff on CC 70; the microQ uses 69) and put "OSC1 FM" on CC 32, which is bank select. Replaced in both Paramsite and `MEMLNautModeMicroQ.hpp`; it is the Blofeld map without noise colour (CC 60/61 are N/E level/balance). Set Global → Controller Recv on. CC file is now `/microq_v2_cc.bin`.

### Erica Steampipe CC map
The rows are the Steampipe's own MIDI CC MAP list, in order, read from its firmware v1.1: Envelope (Attack, Decay, Sustain, Release, Scaling, Velocity, Legato), Steam (DC, Cutoff, Resonance, KTrack, Envelope, Gain, 1-Pole), Pipe (Push, Decay, Release, SoftHard, Symmetry, Tune, Fine, Harmonics, SplitPoint, Split, Polarity), HighPass, LowPass, Detune, Glide, Glide Time, Reverb (Bass, Treble, Size, Spin, Mix, Spread), Volume, then five LFOs (left out). The table uses CC = 70 + row, giving CC 70–106. The manual confirms only rows 0–4 (CC 70–74); the factory CC for later rows is not published, and the firmware computes it rather than storing a table. **Check Settings → MIDI CC MAP on the Steampipe and set any row that differs.** CC file is now `/steampipe_v2_cc.bin`.

### DSI Mopho CC map
Checked against the CC table in the DSI Mopho manual (MIDI Implementation). The table carried over from the old Mopho mode was wrong (e.g. cutoff on CC 74; the Mopho uses CC 102), so it was replaced, in both Paramsite and `MEMLNautModeMopho.hpp`. The Mopho has CCs only for the oscillators (20–27), mixer (28–31), filter (102–107), filter envelope (108–112), VCA and amp envelope (113, 115–119, 75, 76), envelope 3 (85–90, 77, 78), BPM (14) and clock divide (15); LFOs and the mod matrix are NRPN only. Set Global → MIDI Param Receive to CC or All. The CC file is now `/mopho_v2_cc.bin`, so the old file (wrong CCs) is ignored.

### Usage Notes
- If the MIDI CC input source is in use and the receiving device echoes CCs back (MIDI thru), keep the output CCs off CC 1–8 to avoid a feedback loop.
