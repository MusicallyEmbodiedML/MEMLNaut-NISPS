#ifndef __FM_PATTERN_GEN_HPP__
#define __FM_PATTERN_GEN_HPP__

#include <cmath>
#include "../../src/memllib/synth/FastMath.hpp"

#ifndef TWO_PI
#define TWO_PI 6.28318530717958647692f
#endif

struct FMOp {
    float prevOutput = 0.f;

    // phase:    external phasor 0-1
    // modIn:    signal from another operator [-1, 1], 0 = none
    // freqMul:  cycles per phase period
    // modIndex: depth of modulation
    // fbLevel:  feedback 0-1 — feeds prevOutput back into own phase
    float __force_inline process(float phase, float modIn,
                                 float freqMul, float modIndex,
                                 float fbLevel) {
        float p = FastMath::fmod1(phase * freqMul + modIndex * modIn + fbLevel * prevOutput);
        if (p < 0.f) p += 1.f;
        float out = FastMath::sinTurns(p);  // sin(TWO_PI * p)
        prevOutput = out;
        return out;
    }
};

// 2-op FM pair (DX7 style: fbLevel on modulator).
float __force_inline fmPair(float phase,
                             float carrierFreq, float modFreq,
                             float modIndex, float fbLevel,
                             FMOp& carrier, FMOp& modulator) {
    float modOut = modulator.process(phase, 0.f, modFreq, 0.f, fbLevel);
    return carrier.process(phase, modOut, carrierFreq, modIndex, 0.f);
}

struct FMPatternGenState {
    float phaseOffset = 0.f;
    float phasorMul   = 1.f;
    float carrierFreq = 1.f;
    float modFreq     = 2.f;
    float modIndex    = 2.f;
    float fbLevel     = 0.f;
    FMOp carrier;
    FMOp modulator;
};

#endif // __FM_PATTERN_GEN_HPP__
