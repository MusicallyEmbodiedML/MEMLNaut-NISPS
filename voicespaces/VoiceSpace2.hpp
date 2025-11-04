#ifndef __VOICE_SPACE_2_HPP__
#define __VOICE_SPACE_2_HPP__

// VoiceSpace2 - Neemeda
// Macro to generate the voice space lambda body inline
#define VOICE_SPACE_2_BODY \
    p0Gain=1.f; \
    p1Gain=1.f; \
    p2Gain=1.f; \
    p3Gain=1.f; \
    \
    detune1 = 2.0f; \
    detune2 = 0.5f; \
    detune3 = 1.0f; \
    \
    paf0_cf = (params[2]  * 1.f); \
    paf1_cf = (params[3]  * 1.f); \
    paf2_cf = (params[4] * 1.f); \
    paf3_cf = (params[21] * 1.f); \
    \
    paf0_bw = 10.f + (params[5] * 400.f); \
    paf1_bw = 10.f + (params[6] * 300.f); \
    paf2_bw = 10.f + (params[7] * 200.f); \
    paf3_bw = 10.f + (params[22] * 100.f); \
    \
    paf0_vib = params[8] * params[8] * 0.1f; \
    paf1_vib = (params[9] * params[9] * 0.05f); \
    paf2_vib = (params[10] * params[10] * 0.05); \
    paf3_vib = (params[23] * params[23] * 0.05f); \
    \
    paf0_vfr = (params[11] * params[11]* 5.0f); \
    paf1_vfr = (params[12] * params[12] * 5.f); \
    paf2_vfr = (params[13] * params[13] * 10.f); \
    paf3_vfr = (params[24] * params[24] * 10.f); \
    \
    paf0_shift =  -50.f + (params[14] * 200.f); \
    paf1_shift = -50.f + (params[15] * 200.f); \
    paf2_shift = -50.f + (params[16] * 300.f); \
    paf3_shift = -50.f + (params[25] * 400.f); \
    \
    dl1mix = params[17] * params[17] * 0.3f; \
    \
    dlfb = params[19] * 0.95f; \
    \
    env.setup(params[30] * 200.f,params[20] * params[20] * 500.f,0.01 + (params[31] * 0.5f), params[32] * 500.f, sampleRatef ); \
    \
    sineShapeGain = params[26] * params[26]; \
    sineShapeASym = params[27] * params[27] * 0.2f; \
    sineShapeMix = params[28]; \
    \
    rmGain = params[29] * params[29]; \
    feedbackGain = 0.01f; \
    \
    delayMax=3000; \
    fbSmoothAlpha=0.94f;

#endif // __VOICE_SPACE_2_HPP__
