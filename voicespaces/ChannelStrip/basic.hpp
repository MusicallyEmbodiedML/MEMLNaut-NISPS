#ifndef __VOICE_SPACE_CHSTRIP_BASIC_HPP__
#define __VOICE_SPACE_CHSTRIP_BASIC_HPP__

#define VOICE_SPACE_CHSTRIP_NEVE66_BODY \
    preGain=0.5f + (params[0] * params[0] * 4.f); \
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
    postGain=0.5f + (params[23] * params[23] * 4.f); \

#define VOICE_SPACE_CHSTRIP_SSL4KGIST_BODY \
    preGain=0.5f + (params[0] * params[0] * 4.f); \
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
    postGain=0.5f + (params[23] * params[23] * 4.f); \

#define VOICE_SPACE_CHSTRIP_SSL9KINDA_BODY \
    preGain=0.5f + (params[0] * params[0] * 4.f); \
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
    postGain=0.5f + (params[23] * params[23] * 4.f); \

#define VOICE_SPACE_CHSTRIP_MALE_VOX_BODY \
    preGain=0.5f + (params[0] * params[0] * 4.f); \
    inLowPassCutoff = 1000.f + (params[7] * params[7] * 19000.f); \
    inHighPassCutoff = 10.f + (params[8] * params[8] * 1990.f); \
    compThreshold = params[10] * -30.f; \
    compRatio = 2.0f + (params[11] * 6.f); \
    compAttack = 0.08f + (params[12] * 50.f); \
    compRelease = 50.0f + (params[13] * 500.f); \
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
    postGain=0.5f + (params[23] * params[23] * 4.f); \



#define VOICE_SPACE_CHSTRIP_FEMALE_VOX_BODY \
    preGain=0.5f + (params[0] * params[0] * 4.f); \
    inLowPassCutoff = 1000.f + (params[7] * params[7] * 19000.f); \
    inHighPassCutoff = 10.f + (params[8] * params[8] * 1990.f); \
    compThreshold = params[10] * -30.f; \
    compRatio = 2.0f + (params[11] * 6.f); \
    compAttack = 0.08f + (params[12] * 50.f); \
    compRelease = 50.0f + (params[13] * 500.f); \
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
    postGain=0.5f + (params[23] * params[23] * 4.f); \


#define VOICE_SPACE_CHSTRIP_NEVE_80 \
    preGain=0.5f + (params[0] * params[0] * 4.f); \
    constexpr float loPassFrequencies[] = {190, 1200, 3900, 5600, 9200}; \
    inLowPassCutoff = loPassFrequencies[static_cast<size_t>(params[7] * 3.999999)]; \
    constexpr float hiPassFrequencies[] = {27, 47, 92, 150, 270}; \
    inHighPassCutoff = hiPassFrequencies[static_cast<size_t>(params[8] * 3.999999)]; \
    \
    compThreshold = params[10] * -30.f; \
    constexpr float compRatios[] = {1.5, 2, 3, 4, 6}; \
    compRatio = compRatios[static_cast<size_t>(params[11] * 3.999999)]; \
    compAttack = params[12] > 0.5f ? 5.f : 1.f; \
    constexpr float compReleases[] = {400, 800, 1500}; \
    compRelease = compReleases[static_cast<size_t>(params[13] * 1.999999)]; \
    \
    constexpr float lowShelfFrequencies[] = {33, 56, 100, 190, 330};\
    lowShelfFreq = lowShelfFrequencies[static_cast<size_t>(params[14] * 3.999999)];\
    lowShelfQ = 0.6f + (params[15]*4.4f); \
    lowShelfGain = -18.f + (params[16]*36.f);\
    \
    constexpr float lowPeakFrequencies[] = {220, 270, 330, 390, 470, 560, 690, 820, 1000, 1200}; \
    peak0Freq = lowPeakFrequencies[static_cast<size_t>(params[1] * 8.999999)]; \
    peak0Q = 0.6f + (params[5]*4.4f); \
    peak0Gain = -18.f + (params[6]*36.f);\
    \
    constexpr float highPeakFrequencies[] = {1500, 1900, 2200, 2700, 3300, 3900, 4700, 5600, 6900, 8200}; \
    peak1Freq = highPeakFrequencies[static_cast<size_t>(params[4] * 8.999999)]; \
    peak1Q = 0.6f + (params[5]*4.4f); \
    peak1Gain = -18.f + (params[6]*36.f);\
    \
    constexpr float highShelfFrequencies[] = {3300, 4700, 6900, 10000, 15000}; \
    highShelfFreq = highShelfFrequencies[static_cast<size_t>(params[17] * 3.999999)]; \
    highShelfQ = 0.6f + (params[18]*4.4f); \
    highShelfGain = -18.f + (params[19]*36.f);\
    \
    postGain=0.5f + (params[23] * params[23] * 4.f); \

#endif 
