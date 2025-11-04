#ifndef __VOICE_SPACE_1_HPP__
#define __VOICE_SPACE_1_HPP__

// VoiceSpace1 - Rowantares
// Macro to generate the voice space lambda body inline
#define VOICE_SPACE_1_BODY \
    p0Gain=1.f; \
    p1Gain=1.f; \
    p2Gain=0.f; \
    p3Gain=0.f; \
    \
    detune1 = 2.0f; \
    detune2 = 1.0f; \
    detune3 = 1.0f; \
    \
    paf0_cf = (params[2]  * 1.f); \
    paf1_cf = (params[3]  * 1.f); \
    \
    paf0_bw = 10.f + (params[5] * 200.f); \
    paf1_bw = 10.f + (params[6] * 200.f); \
    \
    paf0_vib = params[8] * params[8] * 0.05f; \
    paf1_vib = (params[9] * params[9] * 0.05f); \
    \
    paf0_vfr = (params[11] * params[11]* 5.0f); \
    paf1_vfr = (params[12] * params[12] * 5.f); \
    \
    paf0_shift =  -50.f + (params[14] * 100.f); \
    paf1_shift = -50.f + (params[15] * 100.f); \
    \
    dl1mix = params[17] * params[17] * 0.8f; \
    \
    dlfb = params[19] * 0.9f; \
    \
    env.setup(params[30] * 200.f,params[20] * params[20] * 500.f,0.01 + (params[31] * 0.5f), params[32] * 500.f, sampleRatef ); \
    \
    sineShapeGain = params[26] * params[26]; \
    sineShapeASym = params[27] * params[27] * 0.1f; \
    sineShapeMix = params[28]; \
    \
    rmGain = params[29] * params[29]; \
    feedbackGain = 0.0f; \
    \
    delayMax=1000; \
    fbSmoothAlpha=0.f;

#endif // __VOICE_SPACE_1_HPP__
