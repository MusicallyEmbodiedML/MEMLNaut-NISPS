#ifndef __VOICE_SPACE_VERBFX_DARK_HPP__
#define __VOICE_SPACE_VERBFX_DARK_HPP__

#define VOICE_SPACE_VERBFX_DARK_BODY \
        filterBankDelayXFade = smoothParams[0]; \
\
        lp0fb = smoothParams[1] * 0.9f;\
        lp0cutoff = (smoothParams[2] * 0.3f) + 0.02f;\
\
        lp1fb = smoothParams[3] * 0.9f;\
        lp1cutoff = (smoothParams[4] * 0.3f) + 0.02f;\
\
        lp2fb = smoothParams[5] * 0.9f;\
        lp2cutoff = (smoothParams[6] * 0.3f) + 0.02f;\
\
        lp3fb = smoothParams[7] * 0.9f;\
        lp3cutoff = (smoothParams[8] * 0.3f) + 0.02f;\
\
        lp4fb = smoothParams[9] * 0.9f;\
        lp4cutoff = (smoothParams[10] * 0.3f) + 0.02f;\
\
        lp5fb = smoothParams[11] * 0.9f;\
        lp5cutoff = (smoothParams[12] * 0.3f) + 0.02f;\
\
        lp6fb = smoothParams[13] * 0.9f;\
        lp6cutoff = (smoothParams[14] * 0.3f) + 0.02f;\
\
        lp7fb = smoothParams[15] * 0.9f;\
        lp7cutoff = (smoothParams[16] * 0.3f) + 0.02f;\
\
        allp0fb = smoothParams[17] * 0.9f;\
        allp1fb = smoothParams[18] * 0.9f;\
        allp2fb = smoothParams[19] * 0.9f;\
        allp3fb = smoothParams[20] * 0.9f;\
\
        filterBankF0 = 40.f  + ((smoothParams[21] * smoothParams[21]) * 40.f);\
        filterBankF1 = 80.f  + ((smoothParams[22] * smoothParams[22]) * 80.f);\
        filterBankF2 = 160.f + ((smoothParams[23] * smoothParams[23]) * 160.f);\
        filterBankF3 = 320.f + ((smoothParams[24] * smoothParams[24]) * 320.f);\
        filterBankF4 = 640.f + ((smoothParams[25] * smoothParams[25]) * 640.f);\
        filterBankF5 = 1280.f + ((smoothParams[26] * smoothParams[26]) * 1280.f);\
        filterBankF6 = 2560.f + ((smoothParams[27] * smoothParams[27]) * 2560.f);\
        filterBankF7 = 5120.f + ((smoothParams[28] * smoothParams[28]) * 5120.f);\
\
        filterBankRes0 = 1.f + (sqrtf(smoothParams[29]) * 19.f);\
        filterBankRes1 = 1.f + (sqrtf(smoothParams[30]) * 19.f);\
        filterBankRes2 = 1.f + (sqrtf(smoothParams[31]) * 19.f);\
        filterBankRes3 = 1.f + (sqrtf(smoothParams[32]) * 19.f);\
        filterBankRes4 = 1.f + ((smoothParams[33] * smoothParams[33]) * 19.f);\
        filterBankRes5 = 1.f + ((smoothParams[34] * smoothParams[34]) * 19.f);\
        filterBankRes6 = 1.f + ((smoothParams[35] * smoothParams[35]) * 19.f);\
        filterBankRes7 = 1.f + ((smoothParams[36] * smoothParams[36]) * 19.f);\
\
        ddelayTime = 10.f + (smoothParams[37] * 16373.f);\
        ddelayFeedback = smoothParams[38] * 0.98f;\
\
        ddelayTime1 = 10.f + (smoothParams[39] * 2037.f);\
        ddelayFeedback1 = smoothParams[40] * 0.98f;\
\
        ddelayTime2 = 10.f + (smoothParams[41] * 501.f);\
        ddelayFeedback2 = smoothParams[42] * 0.98f;\
\
        verbVsDelayLevel = smoothParams[43];\
        delayToVerbLevel = smoothParams[44] * 0.99f;\
\
        delayMorph = smoothParams[45];\
        delayBlend = smoothParams[46];

#endif
