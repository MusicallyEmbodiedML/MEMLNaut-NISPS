#ifndef __NAUTLINEAR_AUDIO_APP_HPP__
#define __NAUTLINEAR_AUDIO_APP_HPP__

#include "../../src/memllib/audio/AudioAppBase.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "../../src/memllib/synth/OnePoleSmoother.hpp"
#include "../../src/memllib/synth/maximilian.h"
#include "../../src/memllib/synth/HardClip.hpp"
#include "../../src/memllib/synth/TanhSoftClip.hpp"
#include "../../src/memllib/synth/AsymPowerShaper.hpp"
#include "../../src/memllib/synth/SineShaper.hpp"
#include "../../src/memllib/synth/ChebyshevShaper.hpp"
#include "../../src/memllib/synth/VariableKneeClip.hpp"
#include "../../src/memllib/synth/DiodeClip.hpp"
#include "../../src/memllib/synth/CubicSoftClip.hpp"
#include "../../src/memllib/synth/SubbandFlux.hpp"

// NautLinear — pass-through audio app.
// Cloned from VerbFXAudioApp and cleared out: Process() currently just passes
// the input straight through. The NN-output -> smoother param plumbing is kept
// as a foundation for the new set of processes to come.

// Param map (NN outputs -> smoothParams):
//   0      : clip drive (shared by all clippers)
//   1,2    : asym shaper pos/neg exponents
//   3,4    : sine shaper gain/asym
//   5..10  : Chebyshev harmonic gains T2..T7 (NN-driven harmonic mixer)
//   11     : variable-knee clip knee (0 soft .. 1 hard)
//   12,13  : diode clip pos/neg softness (eta)
template<size_t NPARAMS=14>
class NautLinearAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;

    queue_t wetdryQueue;

    void setWetDryQueued(float value) {
        queue_try_add(&wetdryQueue, &value);
    }

    NautLinearAudioApp() : AudioAppBase<NPARAMS>() {
        queue_init(&wetdryQueue, sizeof(float), 1);
    };

    __attribute__((hot)) stereosample_t __force_inline Process(const stereosample_t x) override
    {
        smoother.Process(neuralNetOutputs.data(), smoothParams.data());

        // mono sum of the input — all processing is mono.
        const float mono = x.L + x.R;

        // param 0 -> clip drive (1x .. ~33x), maps a normalised NN output.
        const float drive = 1.f + smoothParams[0] * 32.f;
        hardClip.setDrive(drive);
        softClip.setDrive(drive);
        cubicClip.setDrive(drive);
        kneeClip.setDrive(drive);
        diodeClip.setDrive(drive);

        // param 11 -> variable-knee hardness (0 soft .. 1 hard).
        kneeClip.setKnee(smoothParams[11]);

        // params 12,13 -> diode clip per-half softness (eta), mapped to 0.05 .. 2.
        diodeClip.setEtaPos(0.05f + smoothParams[12] * 1.95f);
        diodeClip.setEtaNeg(0.05f + smoothParams[13] * 1.95f);

        // params 1,2 -> asymmetric shaper exponents, each mapped to (-3, 3).
        asymShaper.setPosExp(smoothParams[1] * 6.f - 3.f);
        asymShaper.setNegExp(smoothParams[2] * 6.f - 3.f);

        // params 3,4 -> sine shaper fold gain and asymmetry (asym halved, as MEMLCelium).
        sineShaper.setGain(smoothParams[3]);
        sineShaper.setAsym(smoothParams[4] * 0.5f);

        // params 5..10 -> Chebyshev harmonic gains T2..T7, mapped bipolar so the
        // net can null and phase-invert each harmonic.
        for (size_t h = 0; h < ChebyshevShaper<4>::kNumHarmonics; ++h) {
            chebyShaper.setGain(h, smoothParams[5 + h] * 2.f - 1.f);
        }

        // all shapers run in parallel, summed. Chebyshev and the asymmetric diode
        // are DC-blocked (even harmonics add DC).
        const float wet = hardClip.process(mono)
                        + softClip.process(mono)
                        + cubicClip.process(mono)
                        + kneeClip.process(mono)
                        + diodeDC.play(diodeClip.process(mono), 0.995f)
                        + asymShaper.process(mono)
                        + sineShaper.process(mono)
                        + chebyDC.play(chebyShaper.process(mono), 0.995f);

        // wet/dry (equal-power), RVX1 knob via wetdry_mix_.
        const float wetG = sqrtf(wetdry_mix_);
        const float dryG = sqrtf(1.f - wetdry_mix_);
        const float y = (wet * wetG) + (mono * dryG);

        // machine-listen on the OUTPUT — fills outputFlux.{bandEnv,bandFlux,fluxTotal}
        // for later feedback mapping into the signal chain.
        outputFlux.process(y);

        // copy mono output to both channels.
        stereosample_t ret { y, y };
        return ret;
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override
    {
        AudioAppBase<NPARAMS>::Setup(sample_rate, interface);
        maxiSettings::sampleRate = sample_rate;
        outputFlux.setup(sample_rate);  // filters need maxiSettings::sampleRate set first
    }

    __attribute__((always_inline)) void ProcessParams(const std::array<float, NPARAMS>& params)
    {
        {
            float v;
            if (queue_try_remove(&wetdryQueue, &v)) wetdryKnobValue = v;
        }
        if (wetdryKnobValue >= 0.f) {
            wetdry_mix_ = wetdryKnobValue;
        }
        neuralNetOutputs = params;
    }

protected:

    std::array<float,NPARAMS> neuralNetOutputs{0}, smoothParams{0};

    float wetdry_mix_{0.5f};
    float wetdryKnobValue{0.5f};

    OnePoleSmoother<kN_Params> smoother{150.f, (float)kSampleRate};

    HardClip<4> hardClip;
    TanhSoftClip<4> softClip;
    AsymPowerShaper<4> asymShaper;
    SineShaper<4> sineShaper;
    ChebyshevShaper<4> chebyShaper;
    maxiDCBlocker chebyDC;
    CubicSoftClip<4> cubicClip;
    VariableKneeClip<4> kneeClip;
    DiodeClip<4> diodeClip;
    maxiDCBlocker diodeDC;

    // machine listening on the output (feedback features for later mapping)
    SubbandFlux<3> outputFlux;

};

#endif
