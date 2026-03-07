#ifndef __RATIO_SEQ_HPP__
#define __RATIO_SEQ_HPP__

#include <array>
#include <cstddef>

struct ratioSeqState {
    std::array<float, 3> ratios{1.f};
    float phasor=0.f;
    float phasorInc=0.f;
    float phaseOffset = 0.f;
    bool lastTrig = false;
    float phasorMul = 1.f;
    float ratioSum=1.f;
    int midiNote = 36;
    float pulseWidth = 0.5f;

    std::array<float, 2> ampRatios{1.f};
    float ampRatioSum=1.f;

};

template<size_t seqLength>
inline bool __not_in_flash_func(ratioSeq)(float phasor, float phaseOffset, float ratioSum, const std::array<float, seqLength> &ratios, float pulseWidth) {
    bool trig     = 0;
    float offsetPhase  = phaseOffset + phasor;
    if (offsetPhase >= 1.f) {
        offsetPhase -= 1.f;
    }
    float phaseAdj           = ratioSum * offsetPhase;
    float accumulatedSum     = 0;
    float lastAccumulatedSum = 0;
    for (size_t v : ratios)
    {
        accumulatedSum += v;
        if (phaseAdj <= accumulatedSum)
        {
            // check pulse width
            float beatPhase = (phaseAdj - lastAccumulatedSum) /
                               (accumulatedSum - lastAccumulatedSum);
            trig = beatPhase <= pulseWidth;
            break;
        }
        lastAccumulatedSum = accumulatedSum;
    }
    return trig;
}

#endif  // __RATIO_SEQ_HPP__
