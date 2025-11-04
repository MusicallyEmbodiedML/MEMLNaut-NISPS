#ifndef __VOICE_SPACE_QUAD_OCT_HPP__
#define __VOICE_SPACE_QUAD_OCT_HPP__

// VoiceSpaceQuadOct - Elderstar
// Macro to generate the voice space lambda body inline
#define VOICE_SPACE_QUAD_OCT_BODY \
    p0Gain=1.f; \
    p1Gain=params[24]; \
    p2Gain=params[25]; \
    p3Gain=params[26]; \
    \
    const float factor = 1.f + (params[17] + params[27] * 0.2f); \
    detune1 = (1.f * factor) * 0.5f; \
    detune2 = (1.f * factor * factor) * 2.f; \
    detune3 = (detune2 * factor) * 2.f; \
    \
    paf0_cf = (params[0]  * 1.0f); \
    paf1_cf = (params[0]  * 2.0f); \
    paf2_cf = (params[1]  * 3.0f); \
    paf3_cf = (params[1]  * 5.0f); \
    \
    paf0_bw = 10.f + (params[2] * 500.f); \
    paf1_bw = 10.f + (params[3] * 500.f); \
    paf2_bw = 10.f + (params[4] * 500.f); \
    paf3_bw = 10.f + (params[5] * 2000.f); \
    \
    paf0_vib = (params[18] * params[18] * 0.05f); \
    paf1_vib = paf0_vib * 2.f; \
    paf2_vib = params[28] * 0.1f; \
    paf3_vib = params[29] * 0.1f; \
    \
    paf0_vfr = (params[19] * params[19] * 15.f); \
    paf1_vfr = paf0_vfr; \
    paf2_vfr = 0.f; \
    paf3_vfr = 0.f; \
    \
    paf0_shift=0.f; \
    paf1_shift=0.f; \
    paf2_shift =  -100.f + (params[8] * 200.f); \
    paf3_shift =  -150.f + (params[9] * 300.f); \
    \
    dl1mix = params[20] * params[20] * 0.1f; \
    \
    dlfb = params[21] * params[21] * 0.7f; \
    \
    env.setup(1.f+(params[10] * 20.f) , 1.f + (params[11] * 200.f), params[12] * 0.4f, 10.f + (params[13] * 300.f), sampleRatef ); \
    \
    sineShapeGain = params[14] * params[14] * 0.9f; \
    sineShapeASym = params[15] * 0.5f; \
    sineShapeMix = params[16] * 0.8f; \
    \
    rmGain = params[22] * params[22] * 0.7f; \
    feedbackGain = params[23] * params[23] * 0.4f; \
    \
    delayMax=4000; \
    fbSmoothAlpha=0.9f;

#endif // __VOICE_SPACE_QUAD_OCT_HPP__
