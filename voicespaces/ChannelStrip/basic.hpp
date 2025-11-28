#ifndef __VOICE_SPACE_CHSTRIP_BASIC_HPP__
#define __VOICE_SPACE_CHSTRIP_BASIC_HPP__

#define VOICE_SPACE_CHSTRIP_NEVE66_BODY \
    preGain=0.5f + (params[0] * 4.f); \
    \
    inLowPassCutoff = 2000.f + (params[7] * params[7] * 18000.f); \
    inHighPassCutoff = 30.f + (params[8] * params[8] * 270.f); \
    \
    lowShelfFreq = 31.5f + (params[14] * params[14] * 313.5f); \
    lowShelfQ = 0.6f + (params[15] * 4.4f); \
    lowShelfGain = -15.f + (params[16] * 30.f);\
    \
    peak0Freq = 200.f + (params[1] * params[1] * 1800.f); \
    peak0Q = 0.6f + (params[5] * 4.4f); \
    peak0Gain = -18.f + (params[6] * 36.f);\
    peak1Freq = 800.f + (params[4] * params[4] * 7200.f); \
    peak1Q = 0.6f + (params[5] * 4.4f); \
    peak1Gain = -18.f + (params[6] * 36.f);\
    \
    highShelfFreq = 1600.f + (params[17] * params[17] * 14400.f); \
    highShelfQ = 0.6f + (params[18] * 4.4f); \
    highShelfGain = -18.f + (params[19] * 36.f);\
    \
    compThreshold = 20 + (params[10] * -40.f); \
    compRatio = 1.0f + (params[11] * 19.f); \
    compAttack = 0.002f + (params[12] * 10.f); \
    compRelease = 30.0f + (params[13] * params[13] * 2970.f); \
    \
    postGain=0.5f + (params[23] * 4.f); \

#define VOICE_SPACE_CHSTRIP_SSL4KGIST_BODY \
    preGain=0.5f + (params[0] * 4.f); \
    inLowPassCutoff = 3000.f + (params[7] * params[7] *  18000.f); \
    inHighPassCutoff = 10.f + (params[8] * params[8] * 340.f); \
    \
    lowShelfFreq = 30.f + (params[14] * params[14] * 420.f); \
    lowShelfQ = 0.6f + (params[15] * 4.4f); \
    lowShelfGain = -18.f + (params[16] * 36.f);\
    \
    peak0Freq = 200.f + (params[1] * params[1] * 2300.f); \
    peak0Q = 0.6f + (params[5] * 4.4f); \
    peak0Gain = -22.f + (params[6] * 44.f);\
    \
    peak1Freq = 600.f + (params[4] * params[4] * 6400.f); \
    peak1Q = 0.6f + (params[5] * 4.4f); \
    peak1Gain = -22.f + (params[6] * 44.f);\
    \
    highShelfFreq = 1500.f + (params[17] * params[17] * 14500.f); \
    highShelfQ = 0.6f + (params[18] * 4.4f); \
    highShelfGain = -20.f + (params[19] * 40.f);\
    \
    compThreshold = 10.f + (params[10] * -30.f); \
    compRatio = 1.0f + (params[11] * params[11] * 20.f); \
    compAttack = 0.08f + (params[12] * 3.f); \
    compRelease = 100.0f + (params[13] * params [13] * 3900.f); \
    \
    postGain=0.5f + (params[23] * 4.f); \

#define VOICE_SPACE_CHSTRIP_SSL9KINDA_BODY \
    preGain=0.5f + (params[0] * 4.f); \
    inLowPassCutoff = 3000.f + (params[7] * params[7] *  18000.f); \
    inHighPassCutoff = 10.f + (params[8] * params[8] * 490.f); \
    \
    lowShelfFreq = 40.f + (params[14] * params[14] * 560.f); \
    lowShelfQ = 0.6f + (params[15] * 4.4f); \
    lowShelfGain = -20.f + (params[16] * 40.f);\
    \
    peak0Freq = 200.f + (params[1] * params[1] * 1800.f); \
    peak0Q = 0.5f + (params[5] * 2.f); \
    peak0Gain = -20.f + (params[6] * 40.f);\
    \
    peak1Freq = 600.f + (params[4] * params[4] * 6400.f); \
    peak1Q = 0.5f + (params[5] * 2.f); \
    peak1Gain = -20.f + (params[6] * 40.f);\
    \
    highShelfFreq = 1500.f + (params[17] * params[17] * 20500.f); \
    highShelfQ = 0.6f + (params[18] * 4.4f); \
    highShelfGain = -20.f + (params[19] * 40.f);\
    \
    compThreshold = 10.f + (params[10] * -30.f); \
    compRatio = 1.0f + (params[11] * params[11] * 20.f); \
    compAttack = 0.08f + (params[12] * 3.f); \
    compRelease = 100.0f + (params[13] * params [13] * 3900.f); \
    \
    postGain=0.5f + (params[23] * 4.f); \

#define VOICE_SPACE_CHSTRIP_MALE_VOX_BODY \
    preGain=0.5f + (params[0] * 4.f); \
    inLowPassCutoff = 1000.f + (params[7] * params[7] * 19000.f); \
    inHighPassCutoff = 10.f + (params[8] * params[8] * 1990.f); \
    compThreshold = params[10] * -30.f; \
    compRatio = 2.0f + (params[11] * 6.f); \
    compAttack = 0.08f + (params[12] * 50.f); \
    compRelease = 50.0f + (params[13] * 500.f); \
    postGain=0.5f + (params[23] * 4.f); \
    \
    lowShelfFreq = 60.f + (params[14] * params[14] * 240.f); \
    lowShelfQ = 0.6f + (params[15]*4.4f); \
    lowShelfGain = -18.f + (params[16]*36.f);\
    \
    peak0Freq = 60.f + (params[1] * params[1] * 440.f); \
    peak0Q = 0.6f + (params[5]*4.4f); \
    peak0Gain = -18.f + (params[6]*36.f);\
    \
    peak1Freq = 300.f + (params[4] * params[4] * 7700.f); \
    peak1Q = 0.6f + (params[5]*4.4f); \
    peak1Gain = -18.f + (params[6]*36.f);\
    \
    highShelfFreq = 1000.f + (params[17] * params[17] * 7000.f); \
    highShelfQ = 0.6f + (params[18]*4.4f); \
    highShelfGain = -18.f + (params[19]*36.f);\

 

#define VOICE_SPACE_CHSTRIP_FEMALE_VOX_BODY \
    preGain=0.5f + (params[0] * 4.f); \
    inLowPassCutoff = 1000.f + (params[7] * params[7] * 19000.f); \
    inHighPassCutoff = 10.f + (params[8] * params[8] * 1990.f); \
    compThreshold = params[10] * -30.f; \
    compRatio = 2.0f + (params[11] * 6.f); \
    compAttack = 0.08f + (params[12] * 50.f); \
    compRelease = 50.0f + (params[13] * 500.f); \
    postGain=0.5f + (params[23] * 4.f); \
    \
    lowShelfFreq = 120.f + (params[14] * params[14] * 180.f); \
    lowShelfQ = 0.6f + (params[15]*4.4f); \
    lowShelfGain = -18.f + (params[16]*36.f);\
    \
    peak0Freq = 120.f + (params[1] * params[1] * 380.f); \
    peak0Q = 0.6f + (params[5]*4.4f); \
    peak0Gain = -18.f + (params[6]*36.f);\
    \
    peak1Freq = 300.f + (params[4] * params[4] * 9700.f); \
    peak1Q = 0.6f + (params[5]*4.4f); \
    peak1Gain = -18.f + (params[6]*36.f);\
    \
    highShelfFreq = 1000.f + (params[17] * params[17] * 9000.f); \
    highShelfQ = 0.6f + (params[18]*4.4f); \
    highShelfGain = -18.f + (params[19]*36.f);\

#endif 
