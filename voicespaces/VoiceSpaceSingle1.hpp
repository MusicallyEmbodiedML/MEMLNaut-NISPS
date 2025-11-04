#ifndef __VOICE_SPACE_SINGLE_1_HPP__
#define __VOICE_SPACE_SINGLE_1_HPP__

// VoiceSpaceSingle1 - Magnetarch
// Macro to generate the voice space lambda body inline
#define VOICE_SPACE_SINGLE_1_BODY \
    p0Gain=1.f; \
    p1Gain=0.f; \
    p2Gain=0.f; \
    p3Gain=0.f; \
    \
    paf0_cf = (params[0]  * 2.0f); \
    paf0_bw = 10.f + (params[0] * 500.f); \
    \
    paf0_vib = 0.f; \
    \
    paf0_vfr = 0.f; \
    \
    paf0_shift =  -20.f + (params[0] * 40.f); \
    \
    dl1mix = 0.f; \
    \
    dlfb = 0.f; \
    \
    env.setup(1.f+(params[1] * 50.f) , 1.f + (params[2] * 300.f), params[3] * 0.7f, 10.f + (params[4] * 500.f), sampleRatef ); \
    \
    sineShapeGain = params[5] * params[5] * 0.2f; \
    sineShapeASym = 0.f; \
    sineShapeMix = params[6] * 0.3f; \
    \
    rmGain = 0.f; \
    feedbackGain = 0.0f; \
    \
    delayMax=1000; \
    fbSmoothAlpha=0.f;

#endif // __VOICE_SPACE_SINGLE_1_HPP__
