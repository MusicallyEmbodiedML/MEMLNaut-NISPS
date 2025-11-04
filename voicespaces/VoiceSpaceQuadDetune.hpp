#ifndef __VOICE_SPACE_QUAD_DETUNE_HPP__
#define __VOICE_SPACE_QUAD_DETUNE_HPP__

// VoiceSpaceQuadDetune - Ellipticacacia
// Macro to generate the voice space lambda body inline
#define VOICE_SPACE_QUAD_DETUNE_BODY \
    p0Gain=1.f; \
    p1Gain=1.f; \
    p2Gain=1.f; \
    p3Gain=0.8f; \
    \
    const float factor = 1.f + (params[17] * 0.2f); \
    detune1 = 1.f * factor; \
    detune2 = detune1 * factor; \
    detune3 = detune2 * factor; \
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
    paf1_vib = paf0_vib; \
    paf2_vib = 0.f; \
    paf3_vib = 0.f; \
    \
    paf0_vfr = (params[19] * params[19] * 15.f); \
    paf1_vfr = paf0_vfr; \
    paf2_vfr = 0.f; \
    paf3_vfr = 0.f; \
    \
    paf0_shift=0.f; \
    paf1_shift=0.f; \
    paf2_shift=0.f; \
    paf3_shift =  -40.f + (params[9] * 80.f); \
    \
    dl1mix = 0.f; \
    \
    dlfb = 0.f; \
    \
    env.setup(1.f+(params[10] * 20.f) , 1.f + (params[11] * 200.f), params[12] * 0.4f, 10.f + (params[13] * 300.f), sampleRatef ); \
    \
    sineShapeGain = params[14] * params[14] * 0.2f; \
    sineShapeASym = params[15] * 0.05f; \
    sineShapeMix = params[16] * 0.3f; \
    \
    rmGain = 0.f; \
    feedbackGain = 0.0f; \
    \
    delayMax=1000; \
    fbSmoothAlpha=0.f;

#endif // __VOICE_SPACE_QUAD_DETUNE_HPP__
