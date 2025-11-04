#ifndef __VOICE_SPACE_PERC_HPP__
#define __VOICE_SPACE_PERC_HPP__

// VoiceSpacePerc - Aquillow
// Macro to generate the voice space lambda body inline
#define VOICE_SPACE_PERC_BODY \
    p0Gain=1.f; \
    p1Gain=1.f; \
    p2Gain=1.f; \
    p3Gain=1.f; \
    \
    detune1 = 1.0f; \
    detune2 = 1.1f; \
    detune3 = 1.2f; \
    \
    paf0_cf = (params[2]  * 2.f); \
    paf1_cf = (params[3]  * 2.f); \
    paf2_cf = (params[4] * 2.f); \
    paf3_cf = (params[21] * 2.f); \
    \
    paf0_bw = 10.f + (params[5] * 400.f); \
    paf1_bw = 10.f + (params[6] * 50.f); \
    paf2_bw = 10.f + (params[7] * 50.f); \
    paf3_bw = 10.f + (params[22] * 100.f); \
    \
    paf0_vib = params[8] * params[8] * 0.01f; \
    paf1_vib = (params[9] * params[9] * 0.01f); \
    paf2_vib = (params[10] * params[10] * 0.01); \
    paf3_vib = (params[23] * params[23] * 0.01f); \
    \
    paf0_vfr = (params[11] * params[11]* 15.0f); \
    paf1_vfr = (params[12] * params[12] * 15.f); \
    paf2_vfr = (params[13] * params[13] * 15.f); \
    paf3_vfr = (params[24] * params[24] * 15.f); \
    \
    paf0_shift =  -500.f + (params[14] * 500.f); \
    paf1_shift = -300.f + (params[15] * 300.f); \
    paf2_shift = -300.f + (params[16] * 300.f); \
    paf3_shift = -300.f + (params[25] * 300.f); \
    \
    dl1mix = params[17] * params[17] * 0.5f; \
    \
    dlfb = params[19] * 0.95f; \
    \
    env.setup(params[30] * 1.f,params[20] * params[20] * 100.f,0.01 + (params[31] * 0.1f), params[32] * params[32] * 300.f, sampleRatef ); \
    \
    sineShapeGain = params[26]; \
    sineShapeASym = params[27]* 0.5f; \
    sineShapeMix = params[28]; \
    \
    rmGain = params[29]; \
    feedbackGain = 0.1f; \
    \
    delayMax=178; \
    fbSmoothAlpha=0.5f;

#endif // __VOICE_SPACE_PERC_HPP__
