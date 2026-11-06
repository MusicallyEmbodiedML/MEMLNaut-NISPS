#pragma once
// Paramsite presets. The synth CC tables, defaults and focus-group functions are
// copied unchanged from the single-synth modes (modes/MEMLNautMode<Synth>.hpp), each
// in its own namespace. The preset table that uses them is at the end of the file.

#include "../../src/memllib/hardware/memlnaut/display/CCSelectView.hpp"
#include <vector>
#include <cstdint>

// ---- TR8S (from MEMLNautModeTR8S.hpp) ----
namespace ps_tr8s {
// TR-8S MIDI CC targets — full set from the Roland TR-8S MIDI Implementation
// (Version 1.02, Feb 2018). 11 instruments (BD/SD/LT/MT/HT/RS/HC/CH/OH/CC/RC),
// each with Ctrl/Tune/Decay/Level, plus global effect controls.
static const std::vector<CCOption> kTR8SCCOptions = {
    // BD
    { 96, "BD Ctrl"  }, { 20, "BD Tune"  }, { 23, "BD Decay" }, { 24, "BD Level" },
    // SD
    { 97, "SD Ctrl"  }, { 25, "SD Tune"  }, { 28, "SD Decay" }, { 29, "SD Level" },
    // LT
    {102, "LT Ctrl"  }, { 46, "LT Tune"  }, { 47, "LT Decay" }, { 48, "LT Level" },
    // MT
    {103, "MT Ctrl"  }, { 49, "MT Tune"  }, { 50, "MT Decay" }, { 51, "MT Level" },
    // HT
    {104, "HT Ctrl"  }, { 52, "HT Tune"  }, { 53, "HT Decay" }, { 54, "HT Level" },
    // RS
    {105, "RS Ctrl"  }, { 55, "RS Tune"  }, { 56, "RS Decay" }, { 57, "RS Level" },
    // HC
    {106, "HC Ctrl"  }, { 58, "HC Tune"  }, { 59, "HC Decay" }, { 60, "HC Level" },
    // CH
    {107, "CH Ctrl"  }, { 61, "CH Tune"  }, { 62, "CH Decay" }, { 63, "CH Level" },
    // OH
    {108, "OH Ctrl"  }, { 80, "OH Tune"  }, { 81, "OH Decay" }, { 82, "OH Level" },
    // CC (Crash)
    {109, "CC Ctrl"  }, { 83, "CC Tune"  }, { 84, "CC Decay" }, { 85, "CC Level" },
    // RC (Ride)
    {110, "RC Ctrl"  }, { 86, "RC Tune"  }, { 87, "RC Decay" }, { 88, "RC Level" },
    // Global / FX
    { 91, "Reverb Level"   }, { 16, "Delay Level"    }, { 17, "Delay Time"     },
    { 18, "Delay Feedback" }, { 19, "Master FX Ctrl" }, { 15, "Master FX On"   },
    { 71, "Accent"         }, {  9, "Shuffle"        }, { 12, "Ext In Level"   },
    { 14, "Fill In On"     }, { 70, "Fill In Trig"   },
};

// Default 32 assignments: Ctrl + Tune for every instrument (22), Decay for the
// six core voices (6), and four global FX (4).
static const std::vector<uint8_t> kTR8SDefaultCCs = {
    // Ctrl, all 11 instruments
    96, 97, 102, 103, 104, 105, 106, 107, 108, 109, 110,
    // Tune, all 11 instruments
    20, 25, 46, 49, 52, 55, 58, 61, 80, 83, 86,
    // Decay for BD/SD/LT/HC/CH/OH
    23, 28, 47, 59, 62, 81,
    // Reverb + Delay Level/Time/Feedback
    91, 16, 17, 18
};

// Focus groups — one per TR-8S instrument plus a combined FX group.
static constexpr size_t kTR8SNumGroups = 12;
static constexpr uint32_t kTR8SFocusBD = (1u << 0);
static constexpr uint32_t kTR8SFocusSD = (1u << 1);
static constexpr uint32_t kTR8SFocusLT = (1u << 2);
static constexpr uint32_t kTR8SFocusMT = (1u << 3);
static constexpr uint32_t kTR8SFocusHT = (1u << 4);
static constexpr uint32_t kTR8SFocusRS = (1u << 5);
static constexpr uint32_t kTR8SFocusHC = (1u << 6);
static constexpr uint32_t kTR8SFocusCH = (1u << 7);
static constexpr uint32_t kTR8SFocusOH = (1u << 8);
static constexpr uint32_t kTR8SFocusCC = (1u << 9);
static constexpr uint32_t kTR8SFocusRC = (1u << 10);
static constexpr uint32_t kTR8SFocusFX = (1u << 11);

static uint32_t ccToGroupMask(uint8_t cc) {
    switch (cc) {
        case 96: case 20: case 23: case 24:  return kTR8SFocusBD;
        case 97: case 25: case 28: case 29:  return kTR8SFocusSD;
        case 102: case 46: case 47: case 48: return kTR8SFocusLT;
        case 103: case 49: case 50: case 51: return kTR8SFocusMT;
        case 104: case 52: case 53: case 54: return kTR8SFocusHT;
        case 105: case 55: case 56: case 57: return kTR8SFocusRS;
        case 106: case 58: case 59: case 60: return kTR8SFocusHC;
        case 107: case 61: case 62: case 63: return kTR8SFocusCH;
        case 108: case 80: case 81: case 82: return kTR8SFocusOH;
        case 109: case 83: case 84: case 85: return kTR8SFocusCC;
        case 110: case 86: case 87: case 88: return kTR8SFocusRC;
        case 91: case 16: case 17: case 18: case 19:
        case 15: case 71: case 9: case 12: case 14: case 70: return kTR8SFocusFX;
        default: return 0;
    }
}
} // namespace ps_tr8s

// ---- TR6S (from MEMLNautModeTR6S.hpp) ----
namespace ps_tr6s {
// TR-6S MIDI CC targets — all 33 supported CCs, grouped by instrument then effects
static const std::vector<CCOption> kTR6SCCOptions = {
    { 96, "BD Ctrl"        },
    { 97, "SD Ctrl"        },
    { 25, "SD Tune"        },
    { 28, "SD Decay"       },
    { 29, "SD Level"       },
    {102, "LT Ctrl"        },
    { 46, "LT Tune"        },
    { 47, "LT Decay"       },
    { 48, "LT Level"       },
    {106, "HC Ctrl"        },
    { 58, "HC Tune"        },
    { 59, "HC Decay"       },
    { 60, "HC Level"       },
    {107, "CH Ctrl"        },
    { 61, "CH Tune"        },
    { 62, "CH Decay"       },
    { 63, "CH Level"       },
    {108, "OH Ctrl"        },
    { 80, "OH Tune"        },
    { 81, "OH Decay"       },
    { 82, "OH Level"       },
    { 91, "Reverb Level"   },
    { 16, "Delay Level"    },
    { 17, "Delay Time"     },
    { 18, "Delay Feedback" },
    { 19, "Master FX Ctrl" },
    { 15, "Master FX"      },
    { 71, "Accent"         },
    {  9, "Shuffle"        },
    { 70, "Fill IN Trig"   },
};

// Default 16 assignments: 6 Ctrl knobs + Reverb + Delay (Level/Time/FB) + SD Tune/Decay + LT/HC/CH/OH Tune
static const std::vector<uint8_t> kTR6SDefaultCCs = {
    96, 97, 102, 106, 107, 108,   // BD/SD/LT/HC/CH/OH Ctrl
    91, 16, 17, 18,               // Reverb Level, Delay Level/Time/Feedback
    25, 28,                       // SD Tune, SD Decay
    46, 58, 61, 80                // LT/HC/CH/OH Tune
};

// Focus group bitmasks — one per TR-6S instrument/category
static constexpr uint32_t kTR6SFocusBD = (1u << 0);
static constexpr uint32_t kTR6SFocusSD = (1u << 1);
static constexpr uint32_t kTR6SFocusLT = (1u << 2);
static constexpr uint32_t kTR6SFocusHC = (1u << 3);
static constexpr uint32_t kTR6SFocusCH = (1u << 4);
static constexpr uint32_t kTR6SFocusOH = (1u << 5);
static constexpr uint32_t kTR6SFocusFX = (1u << 6);

static uint32_t ccToGroupMask(uint8_t cc) {
    switch (cc) {
        case 96:                              return kTR6SFocusBD;
        case 97: case 25: case 28: case 29:  return kTR6SFocusSD;
        case 102: case 46: case 47: case 48: return kTR6SFocusLT;
        case 106: case 58: case 59: case 60: return kTR6SFocusHC;
        case 107: case 61: case 62: case 63: return kTR6SFocusCH;
        case 108: case 80: case 81: case 82: return kTR6SFocusOH;
        case 91: case 16: case 17: case 18:
        case 19: case 15: case 71: case 9: case 70: return kTR6SFocusFX;
        default: return 0;
    }
}
} // namespace ps_tr6s

// ---- MicroQ (from MEMLNautModeMicroQ.hpp) ----
namespace ps_microq {
// Waldorf microQ MIDI CC options, from the microQ User's Manual, Appendix
// "Controller Numbers". Left out: bank select (0/32), sustain and sostenuto
// (64/66) and 120+. Set Global > Controller Recv to on (W..Z off these CCs).
static const std::vector<CCOption> kMicroQCCOptions = {
    // Performance
    {  1, "Mod Wheel"      }, {  2, "Breath"         }, {  4, "Foot Ctrl"      },
    {  5, "Glide Rate"     }, {  7, "Volume"         }, { 10, "Pan"            },
    { 50, "Pitch Mod"      }, { 51, "Glide Mode"     }, { 65, "Glide Active"   },
    // Arpeggiator
    { 12, "Arp Range"      }, { 13, "Arp Length"     }, { 14, "Arp Active"     },
    // LFOs
    { 15, "LFO1 Shape"     }, { 16, "LFO1 Speed"     }, { 17, "LFO1 Sync"      }, { 18, "LFO1 Delay"     },
    { 19, "LFO2 Shape"     }, { 20, "LFO2 Speed"     }, { 21, "LFO2 Sync"      }, { 22, "LFO2 Delay"     },
    { 23, "LFO3 Shape"     }, { 24, "LFO3 Speed"     }, { 25, "LFO3 Sync"      }, { 26, "LFO3 Delay"     },
    // Oscillators
    { 27, "OSC1 Octave"    }, { 28, "OSC1 Semi"      }, { 29, "OSC1 Detune"    }, { 30, "OSC1 FM"        },
    { 31, "OSC1 Shape"     }, { 33, "OSC1 PW"        }, { 34, "OSC1 PWM"       },
    { 35, "OSC2 Octave"    }, { 36, "OSC2 Semi"      }, { 37, "OSC2 Detune"    }, { 38, "OSC2 FM"        },
    { 39, "OSC2 Shape"     }, { 40, "OSC2 PW"        }, { 41, "OSC2 PWM"       },
    { 42, "OSC3 Octave"    }, { 43, "OSC3 Semi"      }, { 44, "OSC3 Detune"    }, { 45, "OSC3 FM"        },
    { 46, "OSC3 Shape"     }, { 47, "OSC3 PW"        }, { 48, "OSC3 PWM"       }, { 49, "OSC Sync"       },
    // Mixer
    { 52, "OSC1 Level"     }, { 53, "OSC1 Balance"   }, { 54, "RingMod Level"  }, { 55, "RingMod Bal"    },
    { 56, "OSC2 Level"     }, { 57, "OSC2 Balance"   }, { 58, "OSC3 Level"     }, { 59, "OSC3 Balance"   },
    { 60, "N/E Level"      }, { 61, "N/E Balance"    },
    // Filters
    { 67, "Filter Routing" },
    { 68, "F1 Type"        }, { 69, "F1 Cutoff"      }, { 70, "F1 Resonance"   }, { 71, "F1 Drive"       },
    { 72, "F1 Keytrack"    }, { 73, "F1 Env Amt"     }, { 74, "F1 Env Velo"    }, { 75, "F1 CutoffMod"   },
    { 76, "F1 FM"          }, { 77, "F1 Pan"         }, { 78, "F1 PanMod"      },
    { 79, "F2 Type"        }, { 80, "F2 Cutoff"      }, { 81, "F2 Resonance"   }, { 82, "F2 Drive"       },
    { 83, "F2 Keytrack"    }, { 84, "F2 Env Amt"     }, { 85, "F2 Env Velo"    }, { 86, "F2 CutoffMod"   },
    { 87, "F2 FM"          }, { 88, "F2 Pan"         }, { 89, "F2 PanMod"      },
    // Amp and effects
    { 90, "Amp Volume"     }, { 91, "Amp Velocity"   }, { 92, "Amp Mod"        },
    { 93, "FX1 Mix"        }, { 94, "FX2 Mix"        },
    // Filter envelope
    { 95, "FE Attack"      }, { 96, "FE Decay"       }, { 97, "FE Sustain"     },
    { 98, "FE Decay2"      }, { 99, "FE Sustain2"    }, {100, "FE Release"     },
    // Amp envelope
    {101, "AE Attack"      }, {102, "AE Decay"       }, {103, "AE Sustain"     },
    {104, "AE Decay2"      }, {105, "AE Sustain2"    }, {106, "AE Release"     },
    // Envelopes 3 and 4
    {107, "E3 Attack"      }, {108, "E3 Decay"       }, {109, "E3 Sustain"     },
    {110, "E3 Decay2"      }, {111, "E3 Sustain2"    }, {112, "E3 Release"     },
    {113, "E4 Attack"      }, {114, "E4 Decay"       }, {115, "E4 Sustain"     },
    {116, "E4 Decay2"      }, {117, "E4 Sustain2"    }, {118, "E4 Release"     },
};

// Default 32: as the Blofeld (same engine), with filter 2 drive in place of the
// Blofeld-only noise colour.
static const std::vector<uint8_t> kMicroQDefaultCCs = {
    16, 20,                          // LFO1/2 Speed
    29, 30, 33, 34, 37, 38, 40, 44,  // OSC detunes, FM, PW, PWM
    52, 54, 56, 58, 60,              // OSC1/2/3, ring mod and N/E levels
    69, 70, 71, 73,                  // Filter 1 cutoff, resonance, drive, env amount
    80, 81, 82, 84,                  // Filter 2 cutoff, resonance, drive, env amount
    93, 94,                          // FX1/FX2 mix
    95, 96, 97, 100,                 // Filter envelope ADSR
    101, 102, 106                    // Amp envelope attack, decay, release
};

// Focus group bitmasks
static constexpr uint32_t kMQFocusOSC    = (1u << 0);
static constexpr uint32_t kMQFocusMIX    = (1u << 1);
static constexpr uint32_t kMQFocusFILTER = (1u << 2);
static constexpr uint32_t kMQFocusENV    = (1u << 3);
static constexpr uint32_t kMQFocusLFO    = (1u << 4);
static constexpr uint32_t kMQFocusFX     = (1u << 5);

static uint32_t ccToGroupMask(uint8_t cc) {
    if (cc >= 15 && cc <= 26)   return kMQFocusLFO;
    if (cc >= 27 && cc <= 49)   return kMQFocusOSC;
    if (cc >= 52 && cc <= 61)   return kMQFocusMIX;
    if (cc >= 67 && cc <= 89)   return kMQFocusFILTER;
    if (cc >= 90 && cc <= 94)   return kMQFocusFX;
    if (cc >= 95 && cc <= 118)  return kMQFocusENV;
    return 0;  // performance and arp CCs: always active
}
} // namespace ps_microq

// ---- Mopho (from MEMLNautModeMopho.hpp) ----
namespace ps_mopho {
// DSI Mopho MIDI CC options, from the Dave Smith Instruments Mopho manual
// ("MIDI Implementation", CC table). The Mopho family shares this map. Only these
// parameters have CCs; the rest (LFOs, mod matrix, ...) are NRPN only. On the Mopho
// set Global > MIDI Param Receive to CC or All.
static const std::vector<CCOption> kMophoCCOptions = {
    // Performance
    {  1, "Mod Wheel"      }, {  7, "Volume"         },
    { 14, "BPM"            }, { 15, "Clock Divide"   },
    // Oscillators
    { 20, "OSC1 Freq"      }, { 21, "OSC1 Fine"      }, { 22, "OSC1 Shape"     }, { 23, "Glide 1"        },
    { 24, "OSC2 Freq"      }, { 25, "OSC2 Fine"      }, { 26, "OSC2 Shape"     }, { 27, "Glide 2"        },
    // Mixer
    { 28, "Osc Mix"        }, { 29, "Noise Level"    }, { 30, "Sub Osc 1"      }, { 31, "Sub Osc 2"      },
    // Filter
    {102, "Cutoff"         }, {103, "Resonance"      }, {104, "Filt Key Amt"   }, {105, "Filt Audio Mod" },
    {106, "Filt Env Amt"   }, {107, "Filt Env Vel"   },
    // Filter envelope
    {108, "FE Delay"       }, {109, "FE Attack"      }, {110, "FE Decay"       }, {111, "FE Sustain"     },
    {112, "FE Release"     },
    // Amp
    {113, "VCA Level"      }, {115, "Amp Env Amt"    }, {116, "Amp Velocity"   },
    // Amp envelope
    {117, "AE Delay"       }, {118, "AE Attack"      }, {119, "AE Decay"       }, { 75, "AE Sustain"     },
    { 76, "AE Release"     },
    // Envelope 3
    { 85, "E3 Dest"        }, { 86, "E3 Amount"      }, { 87, "E3 Velocity"    }, { 88, "E3 Delay"       },
    { 89, "E3 Attack"      }, { 90, "E3 Decay"       }, { 77, "E3 Sustain"     }, { 78, "E3 Release"     },
};

// Default 32: every sound CC except volume, VCA level, tempo, the glides, the
// velocity amounts and the envelope 3 destination.
static const std::vector<uint8_t> kMophoDefaultCCs = {
    20, 21, 22, 24, 25, 26,            // OSC freq, fine, shape
    28, 29, 30, 31,                    // Mixer
    102, 103, 104, 105, 106,           // Filter
    108, 109, 110, 111, 112,           // Filter envelope
    115,                               // Amp env amount
    117, 118, 119, 75, 76,             // Amp envelope
    86, 88, 89, 90, 77, 78             // Envelope 3
};

// Focus group bitmasks
static constexpr uint32_t kMophoFocusOSC  = (1u << 0);
static constexpr uint32_t kMophoFocusMIX  = (1u << 1);
static constexpr uint32_t kMophoFocusFILT = (1u << 2);
static constexpr uint32_t kMophoFocusFENV = (1u << 3);
static constexpr uint32_t kMophoFocusAENV = (1u << 4);   // amp envelope and VCA
static constexpr uint32_t kMophoFocusENV3 = (1u << 5);

static uint32_t ccToGroupMask(uint8_t cc) {
    if (cc >= 20 && cc <= 27)                 return kMophoFocusOSC;
    if (cc >= 28 && cc <= 31)                 return kMophoFocusMIX;
    if (cc >= 102 && cc <= 107)               return kMophoFocusFILT;
    if (cc >= 108 && cc <= 112)               return kMophoFocusFENV;
    if ((cc >= 113 && cc <= 119) || cc == 75 || cc == 76) return kMophoFocusAENV;
    if ((cc >= 85 && cc <= 90) || cc == 77 || cc == 78)   return kMophoFocusENV3;
    return 0;  // mod wheel, volume, BPM, clock divide: always active
}
} // namespace ps_mopho

// ---- Blofeld (from MEMLNautModeBlofeld.hpp) ----
namespace ps_blofeld {
// Waldorf Blofeld MIDI CC targets, from the Blofeld User's Manual (rev 1.1,
// Appendix "Controller Numbers"). Left out: bank select (0/32), sustain and
// sostenuto (64/66), and the channel mode messages (120+), which would change
// program or hang notes rather than shape the sound.
// On the Blofeld set Global > Ctrl Receive = On, and keep Control W..Z off
// these numbers: a CC used by Control W..Z no longer reaches its Sound parameter.
static const std::vector<CCOption> kBlofeldCCOptions = {
    // Performance
    {  1, "Mod Wheel"      }, {  2, "Breath"         }, {  4, "Foot Ctrl"      },
    {  5, "Glide Rate"     }, {  7, "Volume"         }, { 10, "Pan"            },
    { 50, "Pitch Mod"      }, { 51, "Glide Mode"     }, { 65, "Glide Active"   },
    // Arpeggiator
    { 12, "Arp Range"      }, { 13, "Arp Length"     }, { 14, "Arp Active"     },
    // LFOs
    { 15, "LFO1 Shape"     }, { 16, "LFO1 Speed"     }, { 17, "LFO1 Sync"      }, { 18, "LFO1 Delay"     },
    { 19, "LFO2 Shape"     }, { 20, "LFO2 Speed"     }, { 21, "LFO2 Sync"      }, { 22, "LFO2 Delay"     },
    { 23, "LFO3 Shape"     }, { 24, "LFO3 Speed"     }, { 25, "LFO3 Sync"      }, { 26, "LFO3 Delay"     },
    // Oscillators
    { 27, "OSC1 Octave"    }, { 28, "OSC1 Semi"      }, { 29, "OSC1 Detune"    }, { 30, "OSC1 FM"        },
    { 31, "OSC1 Shape"     }, { 33, "OSC1 PW"        }, { 34, "OSC1 PWM"       },
    { 35, "OSC2 Octave"    }, { 36, "OSC2 Semi"      }, { 37, "OSC2 Detune"    }, { 38, "OSC2 FM"        },
    { 39, "OSC2 Shape"     }, { 40, "OSC2 PW"        }, { 41, "OSC2 PWM"       },
    { 42, "OSC3 Octave"    }, { 43, "OSC3 Semi"      }, { 44, "OSC3 Detune"    }, { 45, "OSC3 FM"        },
    { 46, "OSC3 Shape"     }, { 47, "OSC3 PW"        }, { 48, "OSC3 PWM"       }, { 49, "OSC Sync"       },
    // Mixer
    { 52, "OSC1 Level"     }, { 53, "OSC1 Balance"   }, { 54, "RingMod Level"  }, { 55, "RingMod Bal"    },
    { 56, "OSC2 Level"     }, { 57, "OSC2 Balance"   }, { 58, "OSC3 Level"     }, { 59, "OSC3 Balance"   },
    { 60, "Noise Level"    }, { 61, "Noise Balance"  }, { 62, "Noise Colour"   },
    // Filters
    { 67, "Filter Routing" },
    { 68, "F1 Type"        }, { 69, "F1 Cutoff"      }, { 70, "F1 Resonance"   }, { 71, "F1 Drive"       },
    { 72, "F1 Keytrack"    }, { 73, "F1 Env Amt"     }, { 74, "F1 Env Velo"    }, { 75, "F1 CutoffMod"   },
    { 76, "F1 FM"          }, { 77, "F1 Pan"         }, { 78, "F1 PanMod"      },
    { 79, "F2 Type"        }, { 80, "F2 Cutoff"      }, { 81, "F2 Resonance"   }, { 82, "F2 Drive"       },
    { 83, "F2 Keytrack"    }, { 84, "F2 Env Amt"     }, { 85, "F2 Env Velo"    }, { 86, "F2 CutoffMod"   },
    { 87, "F2 FM"          }, { 88, "F2 Pan"         }, { 89, "F2 PanMod"      },
    // Amp and effects
    { 90, "Amp Volume"     }, { 91, "Amp Velocity"   }, { 92, "Amp Mod"        },
    { 93, "FX1 Mix"        }, { 94, "FX2 Mix"        },
    // Filter envelope
    { 95, "FE Attack"      }, { 96, "FE Decay"       }, { 97, "FE Sustain"     },
    { 98, "FE Decay2"      }, { 99, "FE Sustain2"    }, {100, "FE Release"     },
    // Amp envelope
    {101, "AE Attack"      }, {102, "AE Decay"       }, {103, "AE Sustain"     },
    {104, "AE Decay2"      }, {105, "AE Sustain2"    }, {106, "AE Release"     },
    // Envelopes 3 and 4
    {107, "E3 Attack"      }, {108, "E3 Decay"       }, {109, "E3 Sustain"     },
    {110, "E3 Decay2"      }, {111, "E3 Sustain2"    }, {112, "E3 Release"     },
    {113, "E4 Attack"      }, {114, "E4 Decay"       }, {115, "E4 Sustain"     },
    {116, "E4 Decay2"      }, {117, "E4 Sustain2"    }, {118, "E4 Release"     },
};

// Default 32 (sorted, so the order matches the CC Assign page and the NN outputs).
static const std::vector<uint8_t> kBlofeldDefaultCCs = {
    16, 20,                          // LFO1/2 Speed
    29, 30, 33, 34, 37, 38, 40, 44,  // OSC detunes, FM, PW, PWM
    52, 54, 56, 58, 60, 62,          // OSC1/2/3, ring mod and noise levels, noise colour
    69, 70, 71, 73,                  // Filter 1 cutoff, resonance, drive, env amount
    80, 81, 84,                      // Filter 2 cutoff, resonance, env amount
    93, 94,                          // FX1/FX2 mix
    95, 96, 97, 100,                 // Filter envelope ADSR
    101, 102, 106                    // Amp envelope attack, decay, release
};

// Home value for a CC before the user edits it: centre for bipolar parameters, open
// enough filters and loud enough oscillators to hear something with knob Y at zero.
static float blofeldDefaultHome(uint8_t cc) {
    switch (cc) {
        case 7: case 52: case 56: case 90:            return 100.f / 127.f;  // levels
        case 58:                                      return 50.f / 127.f;   // OSC3 level
        case 69: case 80:                             return 80.f / 127.f;   // cutoffs
        case 70: case 81:                             return 20.f / 127.f;   // resonances
        case 73: case 84:                             return 80.f / 127.f;   // env amounts, a little positive
        case 95: case 101:                            return 0.f;            // attacks
        case 96: case 102:                            return 50.f / 127.f;   // decays
        case 97: case 103:                            return 64.f / 127.f;   // sustains
        case 100: case 106:                           return 40.f / 127.f;   // releases
        case 16: case 20: case 24:                    return 50.f / 127.f;   // LFO speeds
        case 93: case 94:                             return 20.f / 127.f;   // FX mixes
        case 10: case 29: case 34: case 37: case 41:
        case 44: case 48: case 50: case 53: case 55:
        case 57: case 59: case 61: case 62: case 72:
        case 74: case 75: case 77: case 78: case 83:
        case 85: case 86: case 88: case 89: case 91:
        case 92: case 33: case 40: case 47:           return 64.f / 127.f;   // bipolar / mid
        case 28: case 36: case 43:                    return 64.f / 127.f;   // semitone 0
        case 27: case 35: case 42:                    return 64.f / 127.f;   // octave 8'
        default:                                      return 0.f;
    }
}

// Focus groups
static constexpr size_t kBlofeldNumGroups = 7;
static constexpr uint32_t kBFFocusOSC  = (1u << 0);
static constexpr uint32_t kBFFocusMIX  = (1u << 1);
static constexpr uint32_t kBFFocusFILT = (1u << 2);
static constexpr uint32_t kBFFocusFENV = (1u << 3);
static constexpr uint32_t kBFFocusAENV = (1u << 4);
static constexpr uint32_t kBFFocusMOD  = (1u << 5);   // LFOs, envelopes 3 and 4
static constexpr uint32_t kBFFocusFX   = (1u << 6);   // amp and effects

static uint32_t ccToGroupMask(uint8_t cc) {
    if (cc >= 15 && cc <= 26)   return kBFFocusMOD;
    if (cc >= 27 && cc <= 49)   return kBFFocusOSC;
    if (cc >= 52 && cc <= 62)   return kBFFocusMIX;
    if (cc >= 67 && cc <= 89)   return kBFFocusFILT;
    if (cc >= 90 && cc <= 94)   return kBFFocusFX;
    if (cc >= 95 && cc <= 100)  return kBFFocusFENV;
    if (cc >= 101 && cc <= 106) return kBFFocusAENV;
    if (cc >= 107 && cc <= 118) return kBFFocusMOD;
    return 0;  // performance and arp CCs: always active
}
} // namespace ps_blofeld

// ---- Steampipe (from MEMLNautModeSteampipe.hpp) ----
namespace ps_steampipe {
// Erica Synths x 112dB Steampipe MIDI CC options.
//
// The rows are the Steampipe's own MIDI CC MAP list (Settings > MIDI CC MAP), in
// order, as stored in its firmware v1.1 (Envelope, Steam, Pipe, filters, Glide,
// Reverb, Volume; the five LFO rows are left out). CC = 70 + row. The manual
// confirms rows 0-4 (CC 70-74) only; the factory numbers of the later rows are not
// published, so check them on the Steampipe's MIDI CC MAP page and set any that
// differ. The map is editable on the device.
static const std::vector<CCOption> kSteampipeCCOptions = {
    // Envelope
    { 70, "Env Attack"     }, { 71, "Env Decay"      }, { 72, "Env Sustain"    }, { 73, "Env Release"    },
    { 74, "Env Scaling"    }, { 75, "Env Velocity"   }, { 76, "Legato"         },
    // Steam / generator
    { 77, "Stm DC"         }, { 78, "Stm Cutoff"     }, { 79, "Stm Resonance"  }, { 80, "Stm KTrack"     },
    { 81, "Stm Envelope"   }, { 82, "Stm Gain"       }, { 83, "Stm 1-Pole"     },
    // Pipe / resonator
    { 84, "Pipe Push"      }, { 85, "Pipe Decay"     }, { 86, "Pipe Release"   }, { 87, "Pipe SoftHard"  },
    { 88, "Pipe Symmetry"  }, { 89, "Pipe Tune"      }, { 90, "Pipe Fine"      }, { 91, "Pipe Harmonics" },
    { 92, "Pipe SplitPnt"  }, { 93, "Pipe Split"     }, { 94, "Pipe Polarity"  },
    { 95, "HighPass"       }, { 96, "LowPass"        },
    // Glide
    { 97, "Detune"         }, { 98, "Glide"          }, { 99, "Glide Time"     },
    // Reverb
    {100, "Rvb Bass"       }, {101, "Rvb Treble"     }, {102, "Rvb Size"       }, {103, "Rvb Spin"       },
    {104, "Rvb Mix"        }, {105, "Rvb Spread"     },
    // Output
    {106, "Volume"         },
};

// Default 32: everything except the switches (Legato, 1-Pole, Split, Polarity) and Volume.
static const std::vector<uint8_t> kSteampipeDefaultCCs = {
    70, 71, 72, 73, 74, 75,                          // Envelope
    77, 78, 79, 80, 81, 82,                          // Steam
    84, 85, 86, 87, 88, 89, 90, 91, 92, 95, 96,      // Pipe, filters
    97, 98, 99,                                      // Glide
    100, 101, 102, 103, 104, 105                     // Reverb
};

// Focus group bitmasks — one per section
static constexpr uint32_t kSteampipeFocusENV   = (1u << 0);  // Envelope
static constexpr uint32_t kSteampipeFocusSTEAM = (1u << 1);  // Steam / generator
static constexpr uint32_t kSteampipeFocusPIPE  = (1u << 2);  // Pipe, filters, glide
static constexpr uint32_t kSteampipeFocusREVERB= (1u << 3);  // Reverb

static uint32_t ccToGroupMask(uint8_t cc) {
    if (cc >= 70 && cc <= 76)  return kSteampipeFocusENV;
    if (cc >= 77 && cc <= 83)  return kSteampipeFocusSTEAM;
    if (cc >= 84 && cc <= 99)  return kSteampipeFocusPIPE;
    if (cc >= 100 && cc <= 105) return kSteampipeFocusREVERB;
    return 0;  // Volume: always active
}
} // namespace ps_steampipe

// ---- EricaFBank (from MEMLNautModeEricaFBank.hpp) ----
namespace ps_erica {
// Erica Synths Resonant Filterbank MIDI CC options.
// CCs 70-83 are "active-mode dependent" on the device — the same CC number
// controls different parameters depending on which mode (FB, FILTER, CLK MOD,
// DYN EQ) the Erica unit is currently in. All four sets are listed here with
// mode prefixes so users can pick the labels matching their device's active mode.
static const std::vector<CCOption> kEricaFBankCCOptions = {
    // Filterbank (FB) mode — individual band gains
    { 70, "FB B1 29Hz"    },
    { 71, "FB B2 61Hz"    },
    { 72, "FB B3 115Hz"   },
    { 73, "FB B4 218Hz"   },
    { 74, "FB B5 411Hz"   },
    { 75, "FB B6 777Hz"   },
    { 76, "FB B7 1.5kHz"  },
    { 77, "FB B8 2.8kHz"  },
    { 78, "FB B9 5.2kHz"  },
    { 79, "FB B10 11kHz"  },
    // Filterbank mode — grouped gains
    { 80, "FB Low Gain"   },
    { 81, "FB Mid Gain"   },
    { 82, "FB Hi Gain"    },
    { 83, "FB All Gain"   },
    // Universal — always active regardless of device mode
    { 84, "Resonance"     },
    { 85, "FB Chn 1"      },
    { 86, "FB Chn 2"      },
    { 87, "FB Chn 3"      },
    { 88, "FB Chn 4"      },
    { 89, "FB Chn 5"      },
    { 90, "FB Chn 6"      },
    { 91, "FB Chn 7"      },
    { 92, "FB Chn 8"      },
    { 93, "FB Chn 9"      },
    { 94, "FB Chn 10"     },
    { 95, "FB Chn All"    },
    // Global (mono channel)
    { 96, "Mix"           },
    { 97, "Spread"        },
    // Filter mode — same CC range 70-75, different parameter meanings
    { 70, "Flt: Cutoff"   },
    { 71, "Flt: Slope"    },
    { 72, "Flt: Width"    },
    { 73, "Flt: Type"     },
    { 74, "Flt: Gain"     },
    { 75, "Flt: Offset"   },
    // Clocked Modulation mode — CC range 70-76
    { 70, "CLK: Mod Gain" },
    { 71, "CLK: Waveshape"},
    { 72, "CLK: Freq"     },
    { 73, "CLK: Step Dir" },
    { 74, "CLK: BPM"      },
    { 75, "CLK: BandOff"  },
    { 76, "CLK: R Invert" },
    // Dynamic Equalizer mode — CC range 70-76
    { 70, "DYN: Threshold"},
    { 71, "DYN: Ctrl Dcy" },
    { 72, "DYN: Ctrl Atk" },
    { 73, "DYN: EnvDelay" },
    { 74, "DYN: Flt Freq" },
    { 75, "DYN: AudioSrc" },
    { 76, "DYN: BandOff"  },
};

// Default 16: all 10 individual band gains, 3 grouped gains, resonance, mix, spread
static const std::vector<uint8_t> kEricaFBankDefaultCCs = {
    70, 71, 72,      // LOW bands: 29Hz, 61Hz, 115Hz
    73, 74, 75, 76,  // MID bands: 218Hz, 411Hz, 777Hz, 1.5kHz
    77, 78, 79,      // HIGH bands: 2.8kHz, 5.2kHz, 11kHz
    80, 81, 82,      // Band Low / Mid / High grouped
    84,              // Resonance
    96, 97           // Mix, Spread
};

// Focus group bitmasks — frequency-range based
static constexpr uint32_t kEricaFBankFocusLOW  = (1u << 0);  // 29–115 Hz
static constexpr uint32_t kEricaFBankFocusMID   = (1u << 1);  // 218 Hz – 1.5 kHz
static constexpr uint32_t kEricaFBankFocusHIGH  = (1u << 2);  // 2.8–11 kHz
static constexpr uint32_t kEricaFBankFocusGRP   = (1u << 3);  // Grouped band gains
static constexpr uint32_t kEricaFBankFocusRESO  = (1u << 4);  // Resonance + feedback
static constexpr uint32_t kEricaFBankFocusGLOB  = (1u << 5);  // Mix + spread

static uint32_t ccToGroupMask(uint8_t cc) {
    if (cc >= 70 && cc <= 72) return kEricaFBankFocusLOW;
    if (cc >= 73 && cc <= 76) return kEricaFBankFocusMID;
    if (cc >= 77 && cc <= 79) return kEricaFBankFocusHIGH;
    if (cc >= 80 && cc <= 83) return kEricaFBankFocusGRP;
    if (cc >= 84 && cc <= 95) return kEricaFBankFocusRESO;
    if (cc >= 96 && cc <= 97) return kEricaFBankFocusGLOB;
    return 0;
}
} // namespace ps_erica

// ---- Novation Supernova II ----
namespace ps_supernova2 {
// Novation Supernova II MIDI CC options, from the Supernova II User Manual, "MIDI
// Controller Table". Left out: bank select (0/32), data entry (6), arp pattern
// select (3), sustain/latch (64/65), master volume (97, global), NRPN select
// (98/99), reverb type (100) and 120+. CCs act on the part on the sending channel.
static const std::vector<CCOption> kSupernova2CCOptions = {
    // Performance
    {  1, "Mod Wheel"      }, {  2, "Breath"         }, {  5, "Porta Time"     },
    {  7, "Part Volume"    }, {  9, "Arp Speed"      }, { 10, "Pan"            },
    // Oscillators
    { 11, "OSC1 Fine"      }, { 23, "OSC2 Fine"      }, { 12, "OSC3 Fine"      },
    { 22, "OSC1 PW"        }, { 27, "OSC2 PW"        }, { 42, "OSC3 PW"        },
    { 13, "OSC1 Soften"    }, { 14, "OSC2 Soften"    }, { 15, "OSC3 Soften"    },
    { 49, "OSC1 Sync"      }, { 50, "OSC2 Sync"      }, { 51, "OSC3 Sync"      },
    { 20, "O1 Pitch Env2"  }, { 25, "O2 Pitch Env2"  }, { 40, "O3 Pitch Env2"  },
    { 66, "O1 Pitch Env3"  }, { 67, "O2 Pitch Env3"  }, { 68, "O3 Pitch Env3"  },
    { 21, "O1 Pitch LFO1"  }, { 26, "O2 Pitch LFO1"  }, { 41, "O3 Pitch LFO1"  },
    { 43, "O1 Width Env2"  }, { 44, "O2 Width Env2"  }, { 45, "O3 Width Env2"  },
    { 46, "O1 Width LFO1"  }, { 47, "O2 Width LFO1"  }, { 48, "O3 Width LFO1"  },
    { 69, "O1 Width LFO2"  }, { 70, "O2 Width LFO2"  }, { 71, "O3 Width LFO2"  },
    { 52, "O1 Sync Env2"   }, { 53, "O2 Sync Env2"   }, { 54, "O3 Sync Env2"   },
    { 55, "O1 Sync LFO1"   }, { 56, "O2 Sync LFO1"   }, { 57, "O3 Sync LFO1"   },
    { 61, "O1 Soft Env2"   }, { 62, "O2 Soft Env2"   }, { 63, "O3 Soft Env2"   },
    // Mixer
    { 28, "OSC1 Level"     }, { 29, "OSC2 Level"     }, { 33, "OSC3 Level"     },
    { 30, "Noise Level"    }, { 24, "Noise Soften"   },
    { 31, "Ring 1*3 Level" }, {  4, "Ring 2*3 Level" },
    { 36, "O1 Mix Env2"    }, { 37, "O2 Mix Env2"    }, { 38, "O3 Mix Env2"    }, { 39, "Noise Mix Env2" },
    // Filter
    {105, "Cutoff"         }, {106, "Resonance"      }, {104, "Overdrive"      }, { 34, "Filt Tracking"  },
    {107, "Freq Env2"      }, { 59, "Freq Env3"      }, { 60, "Freq LFO1"      }, { 35, "Freq LFO2"      },
    { 72, "Res Env2"       }, { 85, "Res Env3"       }, { 73, "Res LFO1"       }, { 86, "Res LFO2"       },
    // Envelopes
    {108, "Env1 Attack"    }, {109, "Env1 Decay"     }, {110, "Env1 Sustain"   }, {111, "Env1 Release"   },
    {112, "Env1 Velocity"  },
    {113, "Env2 Delay"     }, {114, "Env2 Attack"    }, {115, "Env2 Decay"     }, {116, "Env2 Sustain"   },
    {117, "Env2 Release"   }, {118, "Env2 Velocity"  },
    { 74, "Env3 Delay"     }, { 75, "Env3 Attack"    }, { 76, "Env3 Decay"     }, { 77, "Env3 Sustain"   },
    { 78, "Env3 Release"   }, { 79, "Env3 Velocity"  },
    // LFOs
    { 16, "LFO1 Speed"     }, { 17, "LFO1 Delay"     }, { 82, "LFO1 Offset"    }, { 80, "LFO1 Spd Env3"  },
    { 18, "LFO2 Speed"     }, { 19, "LFO2 Delay"     }, { 83, "LFO2 Offset"    }, { 81, "LFO2 Spd Env3"  },
    // Effects
    { 90, "Dist Level"     }, { 58, "Dist ModWheel"  }, {  8, "FX Morph"       },
    { 87, "Chorus Speed"   }, { 88, "Chorus Depth"   }, { 89, "Chorus Fback"   }, { 93, "Chorus Send"    },
    { 94, "Chorus ModWhl"  },
    {101, "Delay Time"     }, {102, "Delay Fback"    }, {103, "Delay HF Damp"  }, { 92, "Delay Send"     },
    {119, "Delay ModWhl"   },
    { 95, "Reverb Decay"   }, { 96, "Reverb HF Damp" }, { 91, "Reverb Send"    }, { 84, "Reverb ModWhl"  },
};

// Default 32 (sorted on the page): filter, mixer, oscillator detail, envelopes 1 and 2,
// LFO speeds and effect sends.
static const std::vector<uint8_t> kSupernova2DefaultCCs = {
    105, 106, 104, 107, 60, 34,      // Filter: cutoff, res, overdrive, env2, LFO1, tracking
    28, 29, 33, 30, 31,              // Mixer: OSC1-3, noise, ring 1*3
    11, 23, 22, 27, 42, 49,          // OSC1/2 fine, OSC1-3 PW, OSC1 sync
    108, 109, 110, 111,              // Env 1 ADSR
    114, 115, 116, 117,              // Env 2 ADSR
    16, 18,                          // LFO speeds
    90, 91, 92, 93, 101              // Distortion, reverb/delay/chorus sends, delay time
};

// Home values with knob Y at zero. Fine tunes, pulse widths and the modulation depths
// are set to centre (assumed bipolar); levels, filter and envelopes to a playable sound.
static float supernova2DefaultHome(uint8_t cc) {
    switch (cc) {
        case 7: case 28: case 29:                    return 100.f / 127.f;  // part, OSC1/2 levels
        case 105:                                    return 80.f / 127.f;   // cutoff
        case 106:                                    return 20.f / 127.f;   // resonance
        case 108: case 114: case 75:                 return 0.f;            // attacks
        case 109: case 115: case 76:                 return 50.f / 127.f;   // decays
        case 110:                                    return 100.f / 127.f;  // env 1 sustain
        case 116: case 77:                           return 64.f / 127.f;   // env 2/3 sustain
        case 111: case 117: case 78:                 return 40.f / 127.f;   // releases
        case 16: case 18:                            return 50.f / 127.f;   // LFO speeds
        case 91: case 92: case 93:                   return 20.f / 127.f;   // effect sends
        case 101:                                    return 64.f / 127.f;   // delay time
        case 10: case 11: case 12: case 23:
        case 22: case 27: case 42:                   return 64.f / 127.f;   // pan, fine tunes, PWs
        case 20: case 21: case 25: case 26: case 35: case 36: case 37: case 38:
        case 39: case 40: case 41: case 43: case 44: case 45: case 46: case 47:
        case 48: case 52: case 53: case 54: case 55: case 56: case 57: case 59:
        case 60: case 61: case 62: case 63: case 66: case 67: case 68: case 69:
        case 70: case 71: case 72: case 73: case 80: case 81: case 85: case 86:
        case 107:                                    return 64.f / 127.f;   // mod depths
        default:                                     return 0.f;
    }
}

static constexpr uint32_t kSN2FocusOSC  = (1u << 0);
static constexpr uint32_t kSN2FocusMIX  = (1u << 1);
static constexpr uint32_t kSN2FocusFILT = (1u << 2);
static constexpr uint32_t kSN2FocusENV  = (1u << 3);
static constexpr uint32_t kSN2FocusLFO  = (1u << 4);
static constexpr uint32_t kSN2FocusFX   = (1u << 5);

static uint32_t ccToGroupMask(uint8_t cc) {
    switch (cc) {
        case 11: case 12: case 13: case 14: case 15: case 20: case 21: case 22:
        case 23: case 25: case 26: case 27: case 40: case 41: case 42: case 43:
        case 44: case 45: case 46: case 47: case 48: case 49: case 50: case 51:
        case 52: case 53: case 54: case 55: case 56: case 57: case 61: case 62:
        case 63: case 66: case 67: case 68: case 69: case 70: case 71:
            return kSN2FocusOSC;
        case 4: case 24: case 28: case 29: case 30: case 31: case 33:
        case 36: case 37: case 38: case 39:
            return kSN2FocusMIX;
        case 34: case 35: case 59: case 60: case 72: case 73: case 85: case 86:
        case 104: case 105: case 106: case 107:
            return kSN2FocusFILT;
        case 74: case 75: case 76: case 77: case 78: case 79:
        case 108: case 109: case 110: case 111: case 112: case 113: case 114:
        case 115: case 116: case 117: case 118:
            return kSN2FocusENV;
        case 16: case 17: case 18: case 19: case 80: case 81: case 82: case 83:
            return kSN2FocusLFO;
        case 8: case 58: case 84: case 87: case 88: case 89: case 90: case 91:
        case 92: case 93: case 94: case 95: case 96: case 101: case 102: case 103:
        case 119:
            return kSN2FocusFX;
        default:
            return 0;  // performance CCs: always active
    }
}
} // namespace ps_supernova2

// ---- minilogue xd (from the maker's CC table) ----
namespace ps_minilogue_xd {
// Source: Korg minilogue xd Owner's Manual, MIDI Implementation Chart (v1.22, 2019-12-17).
// Global > MIDI Rx CC = On.
static const std::vector<CCOption> kOptions = {
    // Performance
    {  1, "Joystick +Y"},
    {  2, "Joystick -Y"},
    {  5, "Portamento"},
    { 27, "Voice Depth"},
    // OSC
    { 50, "VCO1 Wave"},
    { 48, "VCO1 Octave"},
    { 34, "VCO1 Pitch"},
    { 36, "VCO1 Shape"},
    { 51, "VCO2 Wave"},
    { 49, "VCO2 Octave"},
    { 35, "VCO2 Pitch"},
    { 37, "VCO2 Shape"},
    { 53, "Multi Type"},
    {103, "Multi SubType"},
    { 54, "Multi Shape"},
    {104, "Multi ShiftShp"},
    { 80, "Sync"},
    { 81, "Ring"},
    { 41, "Cross Mod"},
    // MIX
    { 39, "Mix VCO1"},
    { 40, "Mix VCO2"},
    { 33, "Mix Multi"},
    // FILT
    { 43, "Cutoff"},
    { 44, "Resonance"},
    { 84, "Cutoff Drive"},
    { 83, "Keytrack"},
    // ENV
    { 16, "Amp Attack"},
    { 17, "Amp Decay"},
    { 18, "Amp Sustain"},
    { 19, "Amp Release"},
    { 20, "EG Attack"},
    { 21, "EG Decay"},
    { 22, "EG Int"},
    { 23, "EG Target"},
    // LFO
    { 57, "LFO Wave"},
    { 58, "LFO Mode"},
    { 24, "LFO Rate"},
    { 26, "LFO Int"},
    { 56, "LFO Target"},
    // FX
    { 88, "ModFX Type"},
    { 96, "ModFX SubType"},
    { 28, "ModFX Time"},
    { 29, "ModFX Depth"},
    { 89, "Delay Type"},
    {105, "Delay Time"},
    {106, "Delay Depth"},
    {107, "Delay DryWet"},
    { 90, "Reverb Type"},
    {108, "Reverb Time"},
    {109, "Reverb Depth"},
    {110, "Reverb DryWet"},
    { 92, "ModFX On"},
    { 94, "Delay On"},
    { 95, "Reverb On"},
};
// Default 32 CCs.
static const std::vector<uint8_t> kDefaults = {
    34, 35, 36, 37, 54, 104, 80, 81, 41, 39, 40, 33,
    43, 44, 84, 83, 16, 17, 18, 19, 20, 21, 22, 24,
    26, 28, 29, 105, 106, 107, 109, 110,
};
static uint32_t ccToGroupMask(uint8_t cc) {
    switch (cc) {
        case 34: case 35: case 36: case 37: case 41: case 48: case 49: case 50: case 51: case 53: case 54: case 80: case 81: case 103: case 104:  return 1u << 0;  // OSC
        case 33: case 39: case 40:  return 1u << 1;  // MIX
        case 43: case 44: case 83: case 84:  return 1u << 2;  // FILT
        case 16: case 17: case 18: case 19: case 20: case 21: case 22: case 23:  return 1u << 3;  // ENV
        case 24: case 26: case 56: case 57: case 58:  return 1u << 4;  // LFO
        case 28: case 29: case 88: case 89: case 90: case 92: case 94: case 95: case 96: case 105: case 106: case 107: case 108: case 109: case 110:  return 1u << 5;  // FX
        default: return 0;  // performance CCs: always active
    }
}
} // namespace ps_minilogue_xd

// ---- Bass Station II (from the maker's CC table) ----
namespace ps_bassstation2 {
// Source: Novation Bass Station II User Guide v5, "MIDI parameters list" (coarse CC of each pair).
// The CC pairs (e.g. 16:48) are coarse:fine; only the coarse CC is sent.
static const std::vector<CCOption> kOptions = {
    // Performance
    {  7, "Patch Volume"},
    {  5, "Portamento"},
    // OSC
    { 26, "OSC1 Fine"},
    { 70, "OSC1 Range"},
    { 27, "OSC1 Coarse"},
    { 71, "OSC1 ModEnv"},
    { 28, "OSC1 LFO1"},
    { 72, "OSC1 PW ModEnv"},
    { 73, "OSC1 PW LFO2"},
    { 74, "OSC1 PW"},
    { 29, "OSC2 Fine"},
    { 75, "OSC2 Range"},
    { 30, "OSC2 Coarse"},
    { 76, "OSC2 ModEnv"},
    { 31, "OSC2 LFO1"},
    { 77, "OSC2 PW Env2"},
    { 78, "OSC2 PW LFO2"},
    { 79, "OSC2 PW"},
    { 81, "Sub Octave"},
    { 80, "Sub Wave"},
    {110, "OSC 1-2 Sync"},
    // MIX
    { 20, "OSC1 Level"},
    { 21, "OSC2 Level"},
    { 22, "Sub Level"},
    { 23, "Noise Level"},
    { 24, "Ring Level"},
    { 25, "Ext Level"},
    // FILT
    { 83, "Filter Type"},
    {106, "Filter Slope"},
    { 84, "Filter Shape"},
    { 16, "Cutoff"},
    { 82, "Resonance"},
    { 85, "Filt ModEnv"},
    { 17, "Filt LFO2"},
    {114, "Overdrive"},
    {115, "Osc Filt Mod"},
    // LFO
    { 18, "LFO1 Speed"},
    { 86, "LFO1 Delay"},
    { 19, "LFO2 Speed"},
    { 87, "LFO2 Delay"},
    { 88, "LFO1 Wave"},
    { 89, "LFO2 Wave"},
    // ENV
    { 90, "Amp Attack"},
    { 91, "Amp Decay"},
    { 92, "Amp Sustain"},
    { 93, "Amp Release"},
    {102, "ModEnv Attack"},
    {103, "ModEnv Decay"},
    {104, "ModEnv Sustain"},
    {105, "ModEnv Release"},
    {112, "Amp Env Velo"},
    {113, "Mod Env Velo"},
    // FX
    { 94, "Distortion"},
    { 95, "VCA Limit"},
};
// Default 32 CCs.
static const std::vector<uint8_t> kDefaults = {
    26, 27, 71, 28, 74, 29, 30, 76, 31, 79, 20, 21,
    22, 23, 24, 16, 82, 85, 17, 114, 90, 91, 92, 93,
    102, 103, 104, 105, 18, 19, 94, 115,
};
static uint32_t ccToGroupMask(uint8_t cc) {
    switch (cc) {
        case 26: case 27: case 28: case 29: case 30: case 31: case 70: case 71: case 72: case 73: case 74: case 75: case 76: case 77: case 78: case 79: case 80: case 81: case 110:  return 1u << 0;  // OSC
        case 20: case 21: case 22: case 23: case 24: case 25:  return 1u << 1;  // MIX
        case 16: case 17: case 82: case 83: case 84: case 85: case 106: case 114: case 115:  return 1u << 2;  // FILT
        case 90: case 91: case 92: case 93: case 102: case 103: case 104: case 105: case 112: case 113:  return 1u << 3;  // ENV
        case 18: case 19: case 86: case 87: case 88: case 89:  return 1u << 4;  // LFO
        case 94: case 95:  return 1u << 5;  // FX
        default: return 0;  // performance CCs: always active
    }
}
} // namespace ps_bassstation2

// ---- Prophet Rev2 (from the maker's CC table) ----
namespace ps_prophet_rev2 {
// Source: Sequential Prophet Rev2 User's Guide 1.2.4, Appendix E, "Additional Continuous Controllers".
// Global > MIDI Param Receive = CC. CCs edit the layer being edited (A or B).
static const std::vector<CCOption> kOptions = {
    // Performance
    {  1, "Mod Wheel"},
    {  5, "Glide Mode"},
    {  7, "Master Volume"},
    { 14, "BPM"},
    { 15, "Clock Divide"},
    { 10, "Pan Mod Mode"},
    { 33, "Arp On/Off"},
    { 34, "Arp Mode"},
    { 35, "Arp Range"},
    { 36, "Arp Repeats"},
    { 37, "Voice Volume"},
    { 65, "Glide On/Off"},
    // OSC
    { 20, "OSC1 Freq"},
    { 21, "OSC1 Fine"},
    { 22, "OSC1 Shape"},
    { 23, "OSC1 Glide"},
    { 30, "OSC1 ShapeMod"},
    { 24, "OSC2 Freq"},
    { 25, "OSC2 Fine"},
    { 26, "OSC2 Shape"},
    { 27, "OSC2 Glide"},
    { 31, "OSC2 ShapeMod"},
    {  9, "Osc Slop"},
    // MIX
    { 28, "Osc Mix"},
    { 29, "Noise Level"},
    {  8, "Sub Osc"},
    {114, "Pan Spread"},
    // FILT
    {102, "Cutoff"},
    {103, "Resonance"},
    {104, "Filt Key Amt"},
    {105, "Filt Audio Mod"},
    {106, "Filt Env Amt"},
    {107, "Filt Env Velo"},
    // FENV
    {108, "FE Delay"},
    {109, "FE Attack"},
    {110, "FE Decay"},
    {111, "FE Sustain"},
    {112, "FE Release"},
    // AENV
    {113, "VCA Level"},
    {115, "Amp Env Amt"},
    {116, "Amp Env Velo"},
    {117, "AE Delay"},
    {118, "AE Attack"},
    {119, "AE Decay"},
    { 75, "AE Sustain"},
    { 76, "AE Release"},
    // ENV3
    { 85, "E3 Dest"},
    { 86, "E3 Amount"},
    { 87, "E3 Velocity"},
    { 88, "E3 Delay"},
    { 89, "E3 Attack"},
    { 90, "E3 Decay"},
    { 77, "E3 Sustain"},
    { 78, "E3 Release"},
    // FX
    {  3, "FX Type"},
    { 12, "FX Param 1"},
    { 13, "FX Param 2"},
    { 16, "FX On/Off"},
    { 17, "FX Mix"},
};
// Default 32 CCs.
static const std::vector<uint8_t> kDefaults = {
    20, 21, 22, 24, 25, 26, 30, 31, 28, 29, 8, 102,
    103, 104, 105, 106, 108, 109, 110, 111, 112, 115, 118, 119,
    75, 76, 86, 89, 90, 77, 17, 12,
};
static uint32_t ccToGroupMask(uint8_t cc) {
    switch (cc) {
        case 9: case 20: case 21: case 22: case 23: case 24: case 25: case 26: case 27: case 30: case 31:  return 1u << 0;  // OSC
        case 8: case 28: case 29: case 114:  return 1u << 1;  // MIX
        case 102: case 103: case 104: case 105: case 106: case 107:  return 1u << 2;  // FILT
        case 108: case 109: case 110: case 111: case 112:  return 1u << 3;  // FENV
        case 75: case 76: case 113: case 115: case 116: case 117: case 118: case 119:  return 1u << 4;  // AENV
        case 77: case 78: case 85: case 86: case 87: case 88: case 89: case 90:  return 1u << 5;  // ENV3
        case 3: case 12: case 13: case 16: case 17:  return 1u << 6;  // FX
        default: return 0;  // performance CCs: always active
    }
}
} // namespace ps_prophet_rev2

// ---- Digitone (from the maker's CC table) ----
namespace ps_digitone {
// Source: Elektron Digitone User Manual OS 1.44, Appendix B (track parameters, CC MSB).
// Set the MIDI Channel page to the track's channel (Digitone Settings > MIDI > Channels). The FX parameters use the separate FX channel and are not included. The manual also lists trig parameters on CC 13-16 (portamento on, filter/LFO trig); they are left out, and CC 13, 14 and 16 (delay send, reverb send, ratio B) are not in the defaults.
static const std::vector<CCOption> kOptions = {
    // Performance
    { 95, "Track Level"},
    // FM
    { 90, "Algorithm"},
    { 91, "Ratio C"},
    { 92, "Ratio A"},
    { 16, "Ratio B"},
    { 17, "Harmonics"},
    { 18, "Detune"},
    { 19, "Feedback"},
    { 20, "Mix"},
    // FMENV
    { 75, "A Attack"},
    { 76, "A Decay"},
    { 77, "A End"},
    { 78, "A Level"},
    { 79, "B Attack"},
    { 80, "B Decay"},
    { 81, "B End"},
    { 82, "B Level"},
    { 83, "A Delay"},
    { 84, "A Trig"},
    { 85, "A Env Reset"},
    { 86, "B Delay"},
    { 87, "B Trig"},
    { 88, "B Env Reset"},
    { 89, "Phase Reset"},
    // FILT
    { 23, "Cutoff"},
    { 24, "Resonance"},
    { 74, "Filter Type"},
    { 70, "Filt Attack"},
    { 71, "Filt Decay"},
    { 72, "Filt Sustain"},
    { 73, "Filt Release"},
    { 25, "Filt Env Depth"},
    { 43, "Filt Env Delay"},
    { 26, "Base"},
    { 27, "Width"},
    // AMP
    {104, "Amp Attack"},
    {105, "Amp Decay"},
    {106, "Amp Sustain"},
    {107, "Amp Release"},
    {  9, "Drive"},
    { 10, "Pan"},
    {  7, "Volume"},
    { 12, "Chorus Send"},
    { 13, "Delay Send"},
    { 14, "Reverb Send"},
    {102, "Amp Env Reset"},
    // LFO
    { 28, "LFO1 Speed"},
    {108, "LFO1 Mult"},
    {109, "LFO1 Fade"},
    {110, "LFO1 Dest"},
    {111, "LFO1 Wave"},
    {112, "LFO1 Phase"},
    {113, "LFO1 Trig"},
    { 29, "LFO1 Depth"},
    { 30, "LFO2 Speed"},
    {114, "LFO2 Mult"},
    {115, "LFO2 Fade"},
    {116, "LFO2 Dest"},
    {117, "LFO2 Wave"},
    {118, "LFO2 Phase"},
    {119, "LFO2 Trig"},
    { 31, "LFO2 Depth"},
};
// Default 32 CCs.
static const std::vector<uint8_t> kDefaults = {
    91, 92, 17, 18, 19, 20, 75, 76, 77, 78, 79, 80,
    81, 82, 23, 24, 70, 71, 72, 73, 25, 26, 27, 104,
    105, 106, 107, 9, 28, 29, 30, 31,
};
static uint32_t ccToGroupMask(uint8_t cc) {
    switch (cc) {
        case 16: case 17: case 18: case 19: case 20: case 90: case 91: case 92:  return 1u << 0;  // FM
        case 75: case 76: case 77: case 78: case 79: case 80: case 81: case 82: case 83: case 84: case 85: case 86: case 87: case 88: case 89:  return 1u << 1;  // FMENV
        case 23: case 24: case 25: case 26: case 27: case 43: case 70: case 71: case 72: case 73: case 74:  return 1u << 2;  // FILT
        case 7: case 9: case 10: case 12: case 13: case 14: case 102: case 104: case 105: case 106: case 107:  return 1u << 3;  // AMP
        case 28: case 29: case 30: case 31: case 108: case 109: case 110: case 111: case 112: case 113: case 114: case 115: case 116: case 117: case 118: case 119:  return 1u << 4;  // LFO
        default: return 0;  // performance CCs: always active
    }
}
} // namespace ps_digitone

// ---- MicroFreak (from the maker's CC table) ----
namespace ps_microfreak {
// Source: Arturia MicroFreak User Manual 5.0.1, Appendix D "CC# Values" (default CC numbers).
// Only the main panel controls have CCs, so 20 are available.
static const std::vector<CCOption> kOptions = {
    // Performance
    {  2, "Spice"},
    {  5, "Glide"},
    { 91, "Arp/Seq Rate"},
    { 92, "Arp/Seq Sync"},
    // OSC
    {  9, "Osc Type"},
    { 10, "Osc Wave"},
    { 12, "Osc Timbre"},
    { 13, "Osc Shape"},
    // FILT
    { 23, "Cutoff"},
    { 83, "Resonance"},
    { 26, "Filter Amount"},
    // ENV
    {105, "Env Attack"},
    {106, "Env Decay"},
    { 29, "Env Sustain"},
    // CYC
    { 24, "Cyc Amount"},
    { 28, "Cyc Hold"},
    {102, "Cyc Rise"},
    {103, "Cyc Fall"},
    // LFO
    { 93, "LFO Rate"},
    { 94, "LFO Sync Rate"},
};
// Default 16 CCs.
static const std::vector<uint8_t> kDefaults = {
    2, 5, 10, 12, 13, 23, 83, 26, 105, 106, 29, 24,
    28, 102, 103, 93,
};
static uint32_t ccToGroupMask(uint8_t cc) {
    switch (cc) {
        case 9: case 10: case 12: case 13:  return 1u << 0;  // OSC
        case 23: case 26: case 83:  return 1u << 1;  // FILT
        case 29: case 105: case 106:  return 1u << 2;  // ENV
        case 24: case 28: case 102: case 103:  return 1u << 3;  // CYC
        case 93: case 94:  return 1u << 4;  // LFO
        default: return 0;  // performance CCs: always active
    }
}
} // namespace ps_microfreak

// ---- Nina (from the maker's CC table) ----
namespace ps_nina {
// Source: Melbourne Instruments NINA User Manual v1.4, Appendix A (CC 0-63), and NINA v2.0 release notes (effects CCs 44-51; CC 44 was a second Morph before 2.0).
// Needs firmware 2.0 or later for the effects CCs. Set System > MIDI Echo Filter to Echo Filter if Nina's MIDI out comes back to the MEMLNaut. Nina's knobs are motorised and follow incoming CCs, so they move while the network plays.
static const std::vector<CCOption> kOptions = {
    // Performance
    {  1, "Mod Wheel"},
    {  2, "Mod Mtx Source"},
    {  5, "Glide Rate"},
    {  7, "Patch Volume"},
    { 10, "Pan"},
    { 11, "Expression Src"},
    { 43, "Morph"},
    // OSC
    { 14, "Osc1 Tune"},
    { 15, "Osc2 Tune"},
    { 16, "Osc3 Tune"},
    { 17, "Osc1 Width"},
    { 18, "Osc2 Width"},
    { 19, "Osc3 Width"},
    { 20, "Osc1 Blend"},
    { 21, "Osc2 Blend"},
    // MIX
    { 24, "Osc1 Level"},
    { 25, "Osc2 Level"},
    { 22, "Osc3 Level"},
    { 26, "Noise Level"},
    { 42, "Spin"},
    // FILT
    { 29, "Cutoff"},
    { 28, "Resonance"},
    { 30, "Drive"},
    { 31, "Env Amount"},
    // ENV
    { 33, "EG1 Attack"},
    { 34, "EG1 Decay"},
    { 35, "EG1 Sustain"},
    { 36, "EG1 Release"},
    { 37, "EG2 Attack"},
    { 39, "EG2 Decay"},
    { 40, "EG2 Sustain"},
    { 41, "EG2 Release"},
    // LFO
    { 23, "LFO1 Rate"},
    { 27, "LFO1 Level"},
    // FX
    { 12, "Effect Level"},
    { 44, "Chorus Level"},
    { 45, "Chorus Mode"},
    { 46, "Delay Level"},
    { 47, "Delay Time"},
    { 48, "Delay Sync"},
    { 49, "Delay Feedback"},
    { 50, "Delay Tone"},
    { 51, "Reverb Shimmer"},
};
// Default 32 CCs.
static const std::vector<uint8_t> kDefaults = {
    15, 17, 18, 19, 20, 21, 24, 25, 22, 26, 42, 29,
    28, 30, 31, 33, 34, 35, 36, 37, 39, 40, 41, 23,
    27, 43, 44, 46, 47, 49, 50, 51,
};
static uint32_t ccToGroupMask(uint8_t cc) {
    switch (cc) {
        case 14: case 15: case 16: case 17: case 18: case 19: case 20: case 21:  return 1u << 0;  // OSC
        case 22: case 24: case 25: case 26: case 42:  return 1u << 1;  // MIX
        case 28: case 29: case 30: case 31:  return 1u << 2;  // FILT
        case 33: case 34: case 35: case 36: case 37: case 39: case 40: case 41:  return 1u << 3;  // ENV
        case 23: case 27:  return 1u << 4;  // LFO
        case 12: case 44: case 45: case 46: case 47: case 48: case 49: case 50: case 51:  return 1u << 5;  // FX
        default: return 0;  // performance CCs: always active
    }
}
} // namespace ps_nina

// ---- Preset table ----

struct ParamsitePreset {
    const char* name;                       // MIDI Mode menu entry
    const char* desc;                       // one line under the menu
    const char* root;                       // SD model folder and flash file prefix
    const char* tag;                        // model file tag (max 16 chars)
    const char* ccFile;                     // CC mapping file in flash
    uint8_t channel;                        // default MIDI channel
    bool notesThru;                         // pass MIDI-in notes to the synth
    bool hold;                              // HOLD block on the Focus page
    const std::vector<CCOption>* options;   // CC Assign list; nullptr = CC 1-119
    const std::vector<uint8_t>* defaults;   // default CCs
    std::vector<const char*> groups;        // Focus group names, in bit order
    uint32_t (*ccGroup)(uint8_t cc);        // CC -> group mask; nullptr = by output slot
    float (*defaultHome)(uint8_t cc);       // nullptr = 0
};

// User slots group by output slot: 1-8, 9-16, 17-24, 25-32.
static const std::vector<const char*> kParamsiteUserGroups = {"1-8", "9-16", "17-24", "25-32"};

// User slots start on CCs with no standard meaning (14-31, 102-115), so a DAW or
// plugin does not treat them as volume, pan, expression or mod wheel.
static const std::vector<uint8_t> kParamsiteUserDefaultCCs = {
    14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115
};

// Root, tag and CC file match the old single-synth modes, so their saved models and
// CC setups load here. Models from 16-output modes (TR-6S, Steampipe, Erica) are
// rejected by the loader as the wrong size; their CC setups still load.
static const std::vector<ParamsitePreset> kParamsitePresets = {
    {"User 1", "Your own CCs and channel", "paramsite_u1", "Paramsite U1", "/paramsite_u1_cc.bin",
     1, false, true, nullptr, &kParamsiteUserDefaultCCs, kParamsiteUserGroups, nullptr, nullptr},
    {"User 2", "Your own CCs and channel", "paramsite_u2", "Paramsite U2", "/paramsite_u2_cc.bin",
     1, false, true, nullptr, &kParamsiteUserDefaultCCs, kParamsiteUserGroups, nullptr, nullptr},
    {"User 3", "Your own CCs and channel", "paramsite_u3", "Paramsite U3", "/paramsite_u3_cc.bin",
     1, false, true, nullptr, &kParamsiteUserDefaultCCs, kParamsiteUserGroups, nullptr, nullptr},
    {"User 4", "Your own CCs and channel", "paramsite_u4", "Paramsite U4", "/paramsite_u4_cc.bin",
     1, false, true, nullptr, &kParamsiteUserDefaultCCs, kParamsiteUserGroups, nullptr, nullptr},

    {"Roland TR-8S", "Drum machine. Channel 10", "tr8s", "TR-8S", "/tr8s_cc.bin",
     10, false, false, &ps_tr8s::kTR8SCCOptions, &ps_tr8s::kTR8SDefaultCCs,
     {"BD", "SD", "LT", "MT", "HT", "RS", "HC", "CH", "OH", "CC", "RC", "FX"},
     ps_tr8s::ccToGroupMask, nullptr},
    {"Roland TR-6S", "Drum machine. Channel 10", "tr6s", "TR-6S", "/tr6s_cc.bin",
     10, false, false, &ps_tr6s::kTR6SCCOptions, &ps_tr6s::kTR6SDefaultCCs,
     {"BD", "SD", "LT", "HC", "CH", "OH", "FX"},
     ps_tr6s::ccToGroupMask, nullptr},
    // New CC file: the old microQ mode's saved CCs were for a wrong CC map.
    {"Waldorf microQ", "Synth. Channel 1, keyboard thru", "microq", "microQ", "/microq_v2_cc.bin",
     1, true, true, &ps_microq::kMicroQCCOptions, &ps_microq::kMicroQDefaultCCs,
     {"OSC", "MIX", "FILT", "ENV", "LFO", "FX"},
     ps_microq::ccToGroupMask, nullptr},
    {"Waldorf Blofeld", "Synth. Channel 1, keyboard thru", "blofeld", "Blofeld", "/blofeld_cc.bin",
     1, true, false, &ps_blofeld::kBlofeldCCOptions, &ps_blofeld::kBlofeldDefaultCCs,
     {"OSC", "MIX", "FILT", "FENV", "AENV", "MOD", "FX"},
     ps_blofeld::ccToGroupMask, ps_blofeld::blofeldDefaultHome},
    {"Supernova II", "Novation synth. Part on this channel", "supernova2", "Supernova II",
     "/supernova2_cc.bin",
     1, true, false, &ps_supernova2::kSupernova2CCOptions, &ps_supernova2::kSupernova2DefaultCCs,
     {"OSC", "MIX", "FILT", "ENV", "LFO", "FX"},
     ps_supernova2::ccToGroupMask, ps_supernova2::supernova2DefaultHome},
    // New CC file: the old Mopho mode's saved CCs were for a wrong CC map.
    {"DSI Mopho", "Synth. Channel 1, keyboard thru", "mopho", "Mopho", "/mopho_v2_cc.bin",
     1, true, true, &ps_mopho::kMophoCCOptions, &ps_mopho::kMophoDefaultCCs,
     {"OSC", "MIX", "FILT", "FENV", "AENV", "ENV3"},
     ps_mopho::ccToGroupMask, nullptr},
    // New CC file: the old Steampipe mode's CC table did not match the device's list.
    {"Erica Steampipe", "Physical modelling synth. Channel 1, keyboard thru", "steampipe", "Steampipe", "/steampipe_v2_cc.bin",
     1, true, true, &ps_steampipe::kSteampipeCCOptions, &ps_steampipe::kSteampipeDefaultCCs,
     {"ENV", "STEAM", "PIPE", "REVERB"},
     ps_steampipe::ccToGroupMask, nullptr},
    {"Erica Filterbank", "Resonant filterbank. Channel 1", "erica_fbank", "Resonant FBank", "/erica_fbank_cc.bin",
     1, false, true, &ps_erica::kEricaFBankCCOptions, &ps_erica::kEricaFBankDefaultCCs,
     {"LOW", "MID", "HIGH", "GRP", "RESO", "GLOB"},
     ps_erica::ccToGroupMask, nullptr},
    {"minilogue xd", "Korg synth. Channel 1, keyboard thru", "minilogue_xd", "minilogue xd", "/minilogue_xd_cc.bin",
     1, true, true, &ps_minilogue_xd::kOptions, &ps_minilogue_xd::kDefaults,
     {"OSC", "MIX", "FILT", "ENV", "LFO", "FX"},
     ps_minilogue_xd::ccToGroupMask, nullptr},
    {"Bass Station II", "Novation synth. Channel 1, keyboard thru", "bassstation2", "Bass Station II", "/bassstation2_cc.bin",
     1, true, true, &ps_bassstation2::kOptions, &ps_bassstation2::kDefaults,
     {"OSC", "MIX", "FILT", "ENV", "LFO", "FX"},
     ps_bassstation2::ccToGroupMask, nullptr},
    {"Prophet Rev2", "Sequential synth. Channel 1, keyboard thru", "prophet_rev2", "Prophet Rev2", "/prophet_rev2_cc.bin",
     1, true, true, &ps_prophet_rev2::kOptions, &ps_prophet_rev2::kDefaults,
     {"OSC", "MIX", "FILT", "FENV", "AENV", "ENV3", "FX"},
     ps_prophet_rev2::ccToGroupMask, nullptr},
    {"Digitone", "Elektron FM synth. Channel = track, keyboard thru", "digitone", "Digitone", "/digitone_cc.bin",
     1, true, true, &ps_digitone::kOptions, &ps_digitone::kDefaults,
     {"FM", "FMENV", "FILT", "AMP", "LFO"},
     ps_digitone::ccToGroupMask, nullptr},
    {"MicroFreak", "Arturia synth. Channel 1, keyboard thru", "microfreak", "MicroFreak", "/microfreak_cc.bin",
     1, true, true, &ps_microfreak::kOptions, &ps_microfreak::kDefaults,
     {"OSC", "FILT", "ENV", "CYC", "LFO"},
     ps_microfreak::ccToGroupMask, nullptr},
    {"Nina", "Melbourne Instruments synth. Channel = layer", "nina", "Nina", "/nina_cc.bin",
     1, true, true, &ps_nina::kOptions, &ps_nina::kDefaults,
     {"OSC", "MIX", "FILT", "ENV", "LFO", "FX"},
     ps_nina::ccToGroupMask, nullptr},
};
