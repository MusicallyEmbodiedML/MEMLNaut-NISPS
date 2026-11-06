#ifndef __MEMLACIIID_AUDIO_APP_HPP__
#define __MEMLACIIID_AUDIO_APP_HPP__

#include "../../src/memllib/audio/AudioAppBase.hpp"
#include "../../src/memllib/synth/maximilian.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "../../src/memllib/interface/InterfaceBase.hpp"

#include "GlitchSeqEngine.hpp"
#include "AcidVoice.hpp"
#include "OneShotSampler.hpp"
#include "samples/bd2.h"
#include "samples/909closed.h"
#include "samples/909open.h"
#include "../../src/memllib/synth/GrainDelayI16.hpp"

template<size_t NPARAMS=79>
class MEMLAciiidAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;

    // Focus group bitmasks
    static constexpr uint32_t kFocusSeq  = (1u << 0);
    static constexpr uint32_t kFocusDrum = (1u << 1);
    static constexpr uint32_t kFocusBassSeq = (1u << 2);
    static constexpr uint32_t kFocusBass = (1u << 3);
    static constexpr uint32_t kFocusLeadSeq = (1u << 4);
    static constexpr uint32_t kFocusLead = (1u << 5);
    static constexpr uint32_t kFocusClosedHat = (1u << 6);
    static constexpr uint32_t kFocusOpenHat = (1u << 7);
    static constexpr uint32_t kFocusFX   = (1u << 8);

    // Per-param group membership
    static constexpr std::array<uint32_t, NPARAMS> kParamGroupMask = {
        // 0-7: sequencer — 3 ratios, speed, offset, 3 note levels (rest/low/full)
        kFocusSeq, kFocusSeq, kFocusSeq, kFocusSeq, kFocusSeq, kFocusSeq, kFocusSeq, kFocusSeq,
        // 8: drum — decay
        kFocusDrum,
        // 9-18: bass sequencer — 4 ratios, speed, offset, 4 note levels
        kFocusBassSeq, kFocusBassSeq, kFocusBassSeq, kFocusBassSeq, kFocusBassSeq,
        kFocusBassSeq, kFocusBassSeq, kFocusBassSeq, kFocusBassSeq, kFocusBassSeq,
        // 19-23: bass pattern — pitch of each of the 4 notes, slides
        kFocusBassSeq, kFocusBassSeq, kFocusBassSeq, kFocusBassSeq, kFocusBassSeq,
        // 24-29: bass voice — cutoff, resonance, envelope amount, decay, accent, waveform
        kFocusBass, kFocusBass, kFocusBass, kFocusBass, kFocusBass, kFocusBass,
        // 30-39: lead sequencer — 4 ratios, speed, offset, 4 note levels
        kFocusLeadSeq, kFocusLeadSeq, kFocusLeadSeq, kFocusLeadSeq, kFocusLeadSeq,
        kFocusLeadSeq, kFocusLeadSeq, kFocusLeadSeq, kFocusLeadSeq, kFocusLeadSeq,
        // 40-44: lead pattern — pitch of each of the 4 notes, slides
        kFocusLeadSeq, kFocusLeadSeq, kFocusLeadSeq, kFocusLeadSeq, kFocusLeadSeq,
        // 45-51: lead voice — as the bass voice, then distortion drive
        kFocusLead, kFocusLead, kFocusLead, kFocusLead, kFocusLead, kFocusLead, kFocusLead,
        // 52-60: closed hat — sequencer as 0-7, then decay
        kFocusClosedHat, kFocusClosedHat, kFocusClosedHat, kFocusClosedHat, kFocusClosedHat,
        kFocusClosedHat, kFocusClosedHat, kFocusClosedHat, kFocusClosedHat,
        // 61-69: open hat — sequencer as 0-7, then decay
        kFocusOpenHat, kFocusOpenHat, kFocusOpenHat, kFocusOpenHat, kFocusOpenHat,
        kFocusOpenHat, kFocusOpenHat, kFocusOpenHat, kFocusOpenHat,
        // 70-72: sine shaper — gain, asymmetry, mix
        kFocusFX, kFocusFX, kFocusFX,
        // 73-78: master grain delay — length, time, feedback, pitch, spread, wet/dry
        kFocusFX, kFocusFX, kFocusFX, kFocusFX, kFocusFX, kFocusFX,
    };

    // Separate engines because the note counts differ (sequences with the same count share
    // one). They stay in lockstep: all tick, and all change tempo and start or stop, at the
    // same point in Process().
    GlitchSeqEngine<1, 3> drumSeq_;
    enum AcidSequence : size_t { kSeqBass = 0, kSeqLead, kNumAcidSeqs };
    GlitchSeqEngine<kNumAcidSeqs, 4> acidSeq_;
    enum HatSequence : size_t { kSeqClosedHat = 0, kSeqOpenHat, kNumHatSeqs };
    GlitchSeqEngine<kNumHatSeqs, 3> hatSeq_;

    queue_t sequencerControlQueue;
    queue_t bpmControlQueue;

    // Enable bits (Enable screen; set on the control core, read on the audio core):
    // 0 drum, 1 bass, 2 lead, 3 closed hat, 4 open hat, 5 sine shaper, 6 grain delay (off =
    // dry, buffer still fed).
    static constexpr uint32_t kEnableDrum = 1u << 0;
    static constexpr uint32_t kEnableBass = 1u << 1;
    static constexpr uint32_t kEnableLead = 1u << 2;
    static constexpr uint32_t kEnableClosedHat = 1u << 3;
    static constexpr uint32_t kEnableOpenHat = 1u << 4;
    static constexpr uint32_t kEnableShaper = 1u << 5;
    static constexpr uint32_t kEnableDelay = 1u << 6;
    volatile uint32_t enableMask_ = 0b1111111;

    // Mixer screen trims (x0..x2, unity 1). Written by the touch UI on core 0, read per
    // sample here.
    enum MixChannel : size_t {
        kMixDrum = 0, kMixBass, kMixLead, kMixClosedHat, kMixOpenHat, kMixDelay, kMixMaster, kNumMix
    };
    volatile float mixGain_[kNumMix] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};

    // Codec output level up from the 0.55 default: sparse, quiet material (rests, 10%
    // notes) otherwise comes out low.
    AudioDriver::codec_config_t GetDriverConfig() const override {
        return {
            .mic_input     = false,
            .line_level    = 3,
            .mic_gain_dB   = 0,
            .output_volume = 0.97f
        };
    }

    MEMLAciiidAudioApp() : AudioAppBase<NPARAMS>() {
        queue_init(&sequencerControlQueue, sizeof(int), 1);
        queue_init(&bpmControlQueue, sizeof(float), 1);
    };

    stereosample_t __force_inline Process(const stereosample_t x) override
    {
        if (pendingPlay_ >= 0) {
            drumSeq_.setPlaying(pendingPlay_ == 1);
            acidSeq_.setPlaying(pendingPlay_ == 1);
            hatSeq_.setPlaying(pendingPlay_ == 1);
            pendingPlay_ = -1;
        }
        if (pendingBpm_ > 0.f) {
            drumSeq_.updateBPM(pendingBpm_);
            acidSeq_.updateBPM(pendingBpm_);
            hatSeq_.updateBPM(pendingBpm_);
            pendingBpm_ = 0.f;
        }
        drumSeq_.tick();
        acidSeq_.tick();
        hatSeq_.tick();

        float mix = drum_.process() * kDrumGain * mixGain_[kMixDrum];
        if (!(enableMask_ & kEnableDrum)) mix = 0.f;

        // The lines always run, so disabling one doesn't freeze it.
        const float bass = lines_[kSeqBass].voice.process();
        if (enableMask_ & kEnableBass) mix += bass * kBassGain * mixGain_[kMixBass];

        // Lead: its own distortion, then a highpass that keeps it clear of the bass.
        float lead = lines_[kSeqLead].voice.process();
        lead = AcidVoice::softClip(lead * leadDrive_) * leadMakeup_;
        lead = leadHighpass_.play(lead);
        if (enableMask_ & kEnableLead) mix += lead * kLeadGain * mixGain_[kMixLead];

        const float closedHat = closedHat_.process();
        if (enableMask_ & kEnableClosedHat) mix += closedHat * kHatGain * mixGain_[kMixClosedHat];
        const float openHat = openHat_.process();
        if (enableMask_ & kEnableOpenHat) mix += openHat * kHatGain * mixGain_[kMixOpenHat];

        if (enableMask_ & kEnableShaper) {
            float shape = sinf(mix * TWOPI);
            shape = sinf(((shape * TWOPI) * sineShapeGain_) + sineShapeASym_);
            mix = mix + (shape * sineShapeMix_);
        }

        mix = lowBoost_.play(mix);
        mix = midBoost_.play(mix);
        mix = highBoost_.play(mix);

        // Master grain delay, wet/dry.
        const stereosample_t wet = masterGrain_.processStereo(mix);
        const float wetMix = (enableMask_ & kEnableDelay) ? grainMix_ : 0.f;
        const float dry = mix * (1.f - wetMix);

        // Mixer: the Delay fader trims the wet return only; Master drives the final tanh.
        const float wetGain = wetMix * mixGain_[kMixDelay];
        const float master = mixGain_[kMixMaster];
        stereosample_t ret { tanhf((dry + wet.L * wetGain) * master),
                             tanhf((dry + wet.R * wetGain) * master) };
        return ret;
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override
    {
        AudioAppBase<NPARAMS>::Setup(sample_rate, interface);
        maxiSettings::sampleRate = sample_rate;

        drum_.setup(bd2_samples, bd2_length, bd2_sampleRate, sample_rate);
        drum_.setLength(2.f);

        closedHat_.setup(s_909closed_samples, s_909closed_length, s_909closed_sampleRate, sample_rate);
        closedHat_.setLength(2.f);
        openHat_.setup(s_909open_samples, s_909open_length, s_909open_sampleRate, sample_rate);
        openHat_.setLength(2.f);

        lines_[kSeqBass].voice.setup(sample_rate);
        lines_[kSeqLead].voice.setup(sample_rate);
        lines_[kSeqLead].octaveMul = 2.f;
        leadHighpass_.set(maxiBiquad::HIGHPASS, 300.f, 0.707f, 0.f);
        for (size_t i = 0; i < kNumPitches; i++)
            pitchHz_[i] = kBaseHz * exp2f(kPitchSemitones[i] / 12.f);

        lowBoost_.set(maxiBiquad::PEAK, 70.f, 0.707f, 6.f);
        midBoost_.set(maxiBiquad::PEAK, 800.f, 0.707f, 6.f);
        highBoost_.set(maxiBiquad::PEAK, 5000.f, 0.707f, 6.f);

        masterGrain_.setup(sample_rate);

        drumSeq_.setup(sample_rate);
        drumSeq_.updateBPM(120.f);
        drumSeq_.setPlaying(true);
        acidSeq_.setup(sample_rate);
        acidSeq_.updateBPM(120.f);
        acidSeq_.setPlaying(true);
        // The lead favours busy patterns: faster speeds and few rests. Full level is an
        // accent, so it stays the minority.
        acidSeq_.states[kSeqLead].speedMuls = {2.f, 4.f, 4.f, 8.f};
        acidSeq_.states[kSeqLead].restBelow = 0.1f;
        acidSeq_.states[kSeqLead].lowBelow = 0.75f;
        hatSeq_.setup(sample_rate);
        hatSeq_.updateBPM(120.f);
        hatSeq_.setPlaying(true);
        // Hats favour fast patterns. The open hat stays slower: it rings for longer, and
        // each hit cuts off the last.
        hatSeq_.states[kSeqClosedHat].speedMuls = {4.f, 8.f, 8.f, 16.f};
        hatSeq_.states[kSeqOpenHat].speedMuls = {2.f, 4.f, 4.f, 8.f};
        // Hats also favour playing: few rests, half the range at full level, and quiet
        // notes loud enough to hear as ghost notes under the bass.
        for (auto& state : hatSeq_.states) {
            state.restBelow = 0.15f;
            state.lowBelow = 0.5f;
            state.lowLevel = 0.3f;
        }

        // Drum: level is the hit's amplitude (low = 10%, full = 1), with no note-off: the
        // hit always plays out. Acid lines: a full-level note is an accent.
        drumSeq_.onNoteOn = [this](size_t, float level) { drum_.trigger(level); };
        acidSeq_.onNoteOn = [this](size_t seq, float level) {
            AcidLine& line = lines_[seq];
            const size_t note = acidSeq_.states[seq].currentNote;
            line.voice.noteOn(pitchHz_[line.notePitch[note]] * line.octaveMul, level >= 1.f,
                              line.slideMask & (1u << note));
        };
        hatSeq_.onNoteOn = [this](size_t seq, float level) {
            (seq == kSeqClosedHat ? closedHat_ : openHat_).trigger(level);
        };
        // A note stays held if the next one slides in from it.
        acidSeq_.onNoteOff = [this](size_t seq) {
            AcidLine& line = lines_[seq];
            const auto& state = acidSeq_.states[seq];
            const size_t next = (state.currentNote + 1) % kNumNotes;
            if (acidSeq_.playing && (line.slideMask & (1u << next)) && state.levels[next] > 0.f) return;
            line.voice.noteOff();
        };
    }

    void loop() override {
        float newBPM;
        if (queue_try_remove(&bpmControlQueue, &newBPM)) {
            pendingBpm_ = newBPM;
            bpm_ = newBPM;
            applyDelayTime();  // keep the grain delay on the beat
        }

        AudioAppBase<NPARAMS>::loop();
    }

    void ProcessParams(const std::array<float, NPARAMS>& params) override
    {
        int seqControl;
        if (queue_try_remove(&sequencerControlQueue, &seqControl)) {
            pendingPlay_ = seqControl == 1 ? 1 : 0;
        }

        size_t paramIdx = 0;
        auto sqParam = [&]() { const float p = params[paramIdx++]; return p * p; };

        drumSeq_.updateSeqParams(0, params, paramIdx);
        paramIdx += decltype(drumSeq_)::kParamsPerSeq;
        // Drum: length is 0.2x the sample up to the whole sample.
        drum_.setLength(0.2f + params[paramIdx++] * 1.8f);

        updateAcidLine(kSeqBass, params, paramIdx);
        updateAcidLine(kSeqLead, params, paramIdx);

        // Lead distortion: drive x2..x24, with makeup so that more drive isn't much louder.
        const float drive = 2.f * exp2f(params[paramIdx++] * 3.585f);
        leadDrive_ = drive;
        leadMakeup_ = 1.f / sqrtf(sqrtf(drive));

        // Hats: the same decay range as the drum, except that the open hat's long sample can
        // close down much further.
        hatSeq_.updateSeqParams(kSeqClosedHat, params, paramIdx);
        paramIdx += decltype(hatSeq_)::kParamsPerSeq;
        closedHat_.setLength(0.2f + params[paramIdx++] * 1.8f);

        hatSeq_.updateSeqParams(kSeqOpenHat, params, paramIdx);
        paramIdx += decltype(hatSeq_)::kParamsPerSeq;
        openHat_.setLength(0.05f + params[paramIdx++] * 1.95f);

        // Sine shaper, gentler than MEMLCelium's (gain 0..1, mix 0..1).
        sineShapeGain_ = params[paramIdx++] * 0.5f;
        sineShapeASym_ = params[paramIdx++] * 0.5f;
        sineShapeMix_ = params[paramIdx++] * 0.4f;

        // Master grain delay. Pitch snaps to musical ratios (octaves, fifths) so the
        // feedback builds shimmer rather than detuned mush; spread adds a slight detune.
        static constexpr float kGrainPitches[] = {0.5f, 0.75f, 1.f, 1.5f, 2.f};
        masterGrain_.setGrainLengthMs(20.f + sqParam() * 480.f);
        // Delay time: a beat division, synced to the sequencer tempo.
        static constexpr float kDelayBeats[] = {0.125f, 0.1875f, 0.25f, 0.375f, 0.5f,
                                                0.75f, 1.f, 1.5f, 2.f};
        constexpr size_t kNDelayBeats = sizeof(kDelayBeats) / sizeof(kDelayBeats[0]);
        delayBeats_ = kDelayBeats[static_cast<int>(params[paramIdx++] * (kNDelayBeats - 0.001f))];
        applyDelayTime();
        masterGrain_.setFeedback(params[paramIdx++] * 0.85f);
        masterGrain_.setPitch(kGrainPitches[static_cast<int>(params[paramIdx++] * 4.999f)]);
        masterGrain_.setPitchSpread(sqParam() * 0.03f);
        grainMix_ = params[paramIdx++];
    }

protected:
    // The params shared by the acid lines: sequence, pattern and voice.
    void updateAcidLine(size_t seq, const std::array<float, NPARAMS>& params, size_t& paramIdx) {
        AcidLine& line = lines_[seq];
        acidSeq_.updateSeqParams(seq, params, paramIdx);
        paramIdx += decltype(acidSeq_)::kParamsPerSeq;

        // Each of the sequence's notes picks its pitch from the table, and the sequence
        // loops through them. Slides: a bitmask, where bit n slides into note n.
        for (size_t i = 0; i < kNumNotes; i++)
            line.notePitch[i] = static_cast<uint8_t>(params[paramIdx++] * (kNumPitches - 0.001f));
        line.slideMask = static_cast<uint32_t>(params[paramIdx++] * ((1u << kNumNotes) - 0.001f));

        // Cutoff 100Hz..3.2kHz (moved up with the line's octave), envelope +1..+6 octaves,
        // decay 30ms..1s.
        line.voice.setCutoff(100.f * line.octaveMul * exp2f(params[paramIdx++] * 5.f));
        line.voice.setResonance(params[paramIdx++]);
        line.voice.setEnvAmount(1.f + params[paramIdx++] * 5.f);
        line.voice.setDecay(0.03f * expf(params[paramIdx++] * 3.5f));
        line.voice.setAccent(params[paramIdx++]);
        line.voice.setSquare(params[paramIdx++] >= 0.5f);
    }

    // Delay = delayBeats_ at the current tempo. Halved (staying on the beat grid) until
    // it fits the buffer, which matters at slow tempos.
    void applyDelayTime() {
        static constexpr float kMaxDelayMs = 1300.f;
        float ms = delayBeats_ * 60000.f / bpm_;
        while (ms > kMaxDelayMs) ms *= 0.5f;
        masterGrain_.setStartTimeMs(ms);
    }

    static constexpr float kDrumGain = 0.5f;  // the sample peaks at 0.98
    static constexpr float kBassGain = 0.3f;
    static constexpr float kLeadGain = 0.6f;  // higher than the bass: the highpass takes its fundamental
    static constexpr float kHatGain = 0.3f;

    // Bass pitches: semitones above a fixed base.
    static constexpr float kBaseHz = 40.f;
    static constexpr float kPitchSemitones[] = {0.f, 5.f, 7.f, 11.f, 20.f};
    static constexpr size_t kNumPitches = sizeof(kPitchSemitones) / sizeof(kPitchSemitones[0]);
    static constexpr size_t kNumNotes = 4;  // acidSeq_'s note count

    OneShotSampler drum_;
    OneShotSampler closedHat_;
    OneShotSampler openHat_;
    struct AcidLine {
        AcidVoice voice;
        float octaveMul = 1.f;
        volatile uint8_t notePitch[kNumNotes] = {0, 0, 0, 0};
        volatile uint32_t slideMask = 0;
    };
    AcidLine lines_[kNumAcidSeqs];
    float pitchHz_[kNumPitches] = {};
    maxiBiquad leadHighpass_;
    volatile float leadDrive_ = 1.f;
    volatile float leadMakeup_ = 1.f;
    volatile int pendingPlay_ = -1;      // from the control core: 1 start, 0 stop
    volatile float pendingBpm_ = 0.f;    // from the control core
    maxiBiquad lowBoost_, midBoost_, highBoost_;

    GrainDelayI16<65536, 4> masterGrain_;  // ~1.36s at 48kHz, 128KB
    float grainMix_ = 0.f;
    float bpm_ = 120.f;        // matches the sequencers' initial tempo
    float delayBeats_ = 0.5f;  // grain delay time as a beat division

    float sineShapeGain_ = 0.1f;
    float sineShapeASym_ = 0.f;
    float sineShapeMix_ = 0.f;
};

#endif  // __MEMLACIIID_AUDIO_APP_HPP__
