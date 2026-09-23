// meml.ratioseq~ - the MEMLNaut's ratio-based rhythm generator as an MSP
// object: one sequence of the RatioSeqEngine that drives MODE_MEMLCELIUM.
//
// Ports ratioSeq() from modes/AudioApps/RatioSeq.hpp together with the
// per-sequence body of RatioSeqEngine::tick() (modes/AudioApps/
// RatioSeqEngine.hpp). The generator lives in ../common/RhythmCore.h so it
// can be tested without Max (../tests/rhythm_test.cpp). The firmware runs
// NSEQUENCES of these in one engine; here one object is one sequence, so a
// patch makes as many as it needs.
//
// A cycle is cut into slices proportional to @ratios, and each slice fires
// for @pw of its own length - so {1 2 1} gives short, long, short. @ampratios
// is a second, independent ratio pattern whose gate picks the velocity the
// firmware sends: 127 where it is high, 64 where it is low.
//
//   meml.ratioseq~ [ratios...] [@attrs]
//
//   inlet 0  (signal) : bar phase 0..1 - patch a phasor~ in. With nothing
//                       connected, an internal phasor runs at @bpm / @beats.
//            (messages) every attribute below, plus:
//              norm <floats>  the firmware's own NN parameter vector, in
//                       RatioSeqEngine::updateParams() order: one 0..1 float
//                       per ratio, then mul, then offset, then one per amp
//                       ratio. Ratios snap to 1..4, mul to 1/2/4/8, offset to
//                       the @beats grid, exactly as the firmware maps them.
//              reset    restart the internal phasor at 0
//   outlet 0 (signal) : trigger gate, 1 while the slice sounds
//   outlet 1 (signal) : amp gate, 1 where @ampratios is high
//   outlet 2 (int)    : 127 or 64 at each onset, 0 at each release - wire it
//                       straight into makenote / noteout
//
//   attributes
//     @ratios 1 2 1      slice lengths, up to 32 of them
//     @ampratios 1 3     the amp pattern, up to 32
//     @mul 1.            cycles per bar (the firmware uses 1, 2, 4 or 8)
//     @offset 0.         phase offset 0..1 (see PORT NOTE on double offset)
//     @pw 0.5            pulse width as a fraction of each slice
//     @bpm 120.          internal phasor tempo, used when inlet 0 is unpatched
//     @beats 4.          beats per bar, for @bpm and `norm`'s offset grid
//
// Every parameter is a Max attribute, settable by name at control rate, so
// the whole generator can be driven from a mapping object such as nisps.

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

#include <algorithm>
#include <atomic>
#include <cstdio>

#include "RhythmCore.h"

using namespace memlrhythm;

// A ratio list is several words wide, so it cannot be written in place while
// perform64 reads it. Each one is double-buffered: the setter fills the
// inactive slot and publishes it with one atomic store, and perform64 reads
// whichever slot is current. Both slots live for the object's lifetime, so
// nothing is allocated or freed on either side.
struct SwapPattern {
    RatioPattern slots[2];
    std::atomic<int> active { 0 };

    void publish(const float* values, size_t n) {
        const int next = 1 - active.load(std::memory_order_relaxed);
        slots[next].set(values, n);
        active.store(next, std::memory_order_release);
    }

    const RatioPattern& current() const {
        return slots[active.load(std::memory_order_acquire)];
    }
};

struct State {
    SwapPattern ratios;
    SwapPattern ampRatios;
};

typedef struct _meml_ratioseq {
    t_pxobject ob; // must be first
    State* state;  // C++ members with constructors live behind a pointer
    void* clock;
    void* out_vel;

    // attributes
    double ratios[RatioPattern::kMaxRatios];
    long ratios_count;
    double ampratios[RatioPattern::kMaxRatios];
    long ampratios_count;
    double mul;
    double offset;
    double pw;
    double bpm;
    double beats;

    // dsp state
    double phase;
    double phaseInc;
    double sr;
    short phaseConnected;
    bool lastTrig;

    // set by perform64, read by the clock callback
    t_atom_long pendingVel;
} t_meml_ratioseq;

static t_class* meml_ratioseq_class = nullptr;

void* meml_ratioseq_new(t_symbol* s, long argc, t_atom* argv);
void meml_ratioseq_free(t_meml_ratioseq* x);
void meml_ratioseq_assist(t_meml_ratioseq* x, void* b, long m, long a, char* s);
void meml_ratioseq_dsp64(t_meml_ratioseq* x, t_object* dsp64, short* count, double samplerate,
                         long maxvectorsize, long flags);
void meml_ratioseq_perform64(t_meml_ratioseq* x, t_object* dsp64, double** ins, long numins, double** outs,
                             long numouts, long sampleframes, long flags, void* userparam);
void meml_ratioseq_tick(t_meml_ratioseq* x);
void meml_ratioseq_norm(t_meml_ratioseq* x, t_symbol* s, long argc, t_atom* argv);
void meml_ratioseq_reset(t_meml_ratioseq* x);
t_max_err meml_ratioseq_set_ratios(t_meml_ratioseq* x, t_object* attr, long argc, t_atom* argv);
t_max_err meml_ratioseq_set_bpm(t_meml_ratioseq* x, t_object* attr, long argc, t_atom* argv);

static void meml_ratioseq_update_inc(t_meml_ratioseq* x) {
    const double beats = (x->beats > 0.) ? x->beats : 4.;
    const double barHz = (x->bpm / 60.) / beats;
    x->phaseInc = (x->sr > 0.) ? (barHz / x->sr) : 0.;
}

// Copy one of the double[] attribute arrays into its RT-side double buffer.
static void meml_ratioseq_publish(t_meml_ratioseq* x, bool amp) {
    float tmp[RatioPattern::kMaxRatios];
    const double* src = amp ? x->ampratios : x->ratios;
    const long count = amp ? x->ampratios_count : x->ratios_count;
    const long n = std::clamp<long>(count, 1, (long)RatioPattern::kMaxRatios);
    for (long i = 0; i < n; ++i) tmp[i] = (float)std::max(0.0, src[i]);
    (amp ? x->state->ampRatios : x->state->ratios).publish(tmp, (size_t)n);
}

void C74_EXPORT ext_main(void* r) {
    t_class* c = class_new("meml.ratioseq~", (method)meml_ratioseq_new, (method)meml_ratioseq_free,
                           (long)sizeof(t_meml_ratioseq), 0L, A_GIMME, 0);

    class_addmethod(c, (method)meml_ratioseq_dsp64, "dsp64", A_CANT, 0);
    class_addmethod(c, (method)meml_ratioseq_norm, "norm", A_GIMME, 0);
    class_addmethod(c, (method)meml_ratioseq_reset, "reset", 0);
    class_addmethod(c, (method)meml_ratioseq_assist, "assist", A_CANT, 0);

    CLASS_ATTR_DOUBLE_VARSIZE(c, "ratios", 0, t_meml_ratioseq, ratios, ratios_count, RatioPattern::kMaxRatios);
    CLASS_ATTR_LABEL(c, "ratios", 0, "Slice Lengths");
    CLASS_ATTR_ACCESSORS(c, "ratios", NULL, meml_ratioseq_set_ratios);

    CLASS_ATTR_DOUBLE_VARSIZE(c, "ampratios", 0, t_meml_ratioseq, ampratios, ampratios_count, RatioPattern::kMaxRatios);
    CLASS_ATTR_LABEL(c, "ampratios", 0, "Amp Pattern");
    CLASS_ATTR_ACCESSORS(c, "ampratios", NULL, meml_ratioseq_set_ratios);

    CLASS_ATTR_DOUBLE(c, "mul", 0, t_meml_ratioseq, mul);
    CLASS_ATTR_FILTER_MIN(c, "mul", 0.);
    CLASS_ATTR_LABEL(c, "mul", 0, "Cycles Per Bar");

    CLASS_ATTR_DOUBLE(c, "offset", 0, t_meml_ratioseq, offset);
    CLASS_ATTR_LABEL(c, "offset", 0, "Phase Offset");

    CLASS_ATTR_DOUBLE(c, "pw", 0, t_meml_ratioseq, pw);
    CLASS_ATTR_FILTER_CLIP(c, "pw", 0., 1.);
    CLASS_ATTR_LABEL(c, "pw", 0, "Pulse Width");

    CLASS_ATTR_DOUBLE(c, "bpm", 0, t_meml_ratioseq, bpm);
    CLASS_ATTR_LABEL(c, "bpm", 0, "Internal Tempo (BPM)");
    CLASS_ATTR_ACCESSORS(c, "bpm", NULL, meml_ratioseq_set_bpm);

    CLASS_ATTR_DOUBLE(c, "beats", 0, t_meml_ratioseq, beats);
    CLASS_ATTR_LABEL(c, "beats", 0, "Beats Per Bar");
    CLASS_ATTR_ACCESSORS(c, "beats", NULL, meml_ratioseq_set_bpm);

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    meml_ratioseq_class = c;
}

void* meml_ratioseq_new(t_symbol* s, long argc, t_atom* argv) {
    t_meml_ratioseq* x = (t_meml_ratioseq*)object_alloc(meml_ratioseq_class);
    if (!x) return nullptr;

    dsp_setup((t_pxobject*)x, 1); // one signal inlet: bar phase

    // outlets, right to left
    x->out_vel = intout((t_object*)x);
    outlet_new((t_object*)x, "signal"); // amp gate
    outlet_new((t_object*)x, "signal"); // trigger gate

    x->state = new State();
    x->clock = clock_new(x, (method)meml_ratioseq_tick);

    // RatioSeqEngine's own defaults: 3 ratios, 2 amp ratios.
    x->ratios[0] = 1.; x->ratios[1] = 2.; x->ratios[2] = 1.; x->ratios_count = 3;
    x->ampratios[0] = 1.; x->ampratios[1] = 3.; x->ampratios_count = 2;
    x->mul = 1.;
    x->offset = 0.;
    x->pw = 0.5;
    x->bpm = 120.;
    x->beats = 4.;
    x->phase = 0.;
    x->sr = sys_getsr() > 0 ? sys_getsr() : 48000.;
    x->phaseConnected = 0;
    x->lastTrig = false;
    x->pendingVel = 0;
    meml_ratioseq_update_inc(x);

    // Positional arguments are the ratios: meml.ratioseq~ 1 2 1
    const long nPositional = attr_args_offset((short)argc, argv);
    if (nPositional > 0) {
        const long n = std::min<long>(nPositional, (long)RatioPattern::kMaxRatios);
        for (long i = 0; i < n; ++i) x->ratios[i] = std::max(0., atom_getfloat(argv + i));
        x->ratios_count = n;
    }

    attr_args_process(x, (short)argc, argv);
    meml_ratioseq_publish(x, false);
    meml_ratioseq_publish(x, true);
    return x;
}

void meml_ratioseq_free(t_meml_ratioseq* x) {
    dsp_free((t_pxobject*)x); // must come first
    clock_unset(x->clock);
    object_free(x->clock);
    delete x->state;
    x->state = nullptr;
}

void meml_ratioseq_assist(t_meml_ratioseq* x, void* b, long m, long a, char* s) {
    if (m == ASSIST_INLET) {
        snprintf(s, 256, "(signal) bar phase 0..1, or messages: ratios, ampratios, mul, offset, pw, bpm, norm, reset");
    } else if (a == 0) {
        snprintf(s, 256, "(signal) trigger gate");
    } else if (a == 1) {
        snprintf(s, 256, "(signal) amp gate, high = velocity 127");
    } else {
        snprintf(s, 256, "(int) velocity 127 / 64 at onset, 0 at release");
    }
}

// One setter serves both ratio lists; which one is named by the attribute.
t_max_err meml_ratioseq_set_ratios(t_meml_ratioseq* x, t_object* attr, long argc, t_atom* argv) {
    const t_symbol* name = (t_symbol*)object_method(attr, gensym("getname"));
    const bool amp = (name == gensym("ampratios"));
    if (argc > 0 && argv) {
        const long n = std::min<long>(argc, (long)RatioPattern::kMaxRatios);
        double* dst = amp ? x->ampratios : x->ratios;
        for (long i = 0; i < n; ++i) dst[i] = std::max(0., atom_getfloat(argv + i));
        (amp ? x->ampratios_count : x->ratios_count) = n;
        meml_ratioseq_publish(x, amp);
    }
    return MAX_ERR_NONE;
}

t_max_err meml_ratioseq_set_bpm(t_meml_ratioseq* x, t_object* attr, long argc, t_atom* argv) {
    if (argc && argv) {
        const t_symbol* name = (t_symbol*)object_method(attr, gensym("getname"));
        const double v = atom_getfloat(argv);
        if (name == gensym("beats")) x->beats = std::max(0.001, v);
        else x->bpm = std::clamp(v, 0.001, 9999.);
        meml_ratioseq_update_inc(x);
    }
    return MAX_ERR_NONE;
}

// The firmware's NN parameter vector, in updateParams() order:
//   ratios_count floats, mul, offset, ampratios_count floats - all 0..1.
void meml_ratioseq_norm(t_meml_ratioseq* x, t_symbol* s, long argc, t_atom* argv) {
    const long nR = std::clamp<long>(x->ratios_count, 1, (long)RatioPattern::kMaxRatios);
    const long nA = std::clamp<long>(x->ampratios_count, 1, (long)RatioPattern::kMaxRatios);
    const long need = nR + 2 + nA;
    if (argc < need) {
        object_warn((t_object*)x, "norm needs %ld values (%ld ratios, mul, offset, %ld amp ratios), got %ld",
                    need, nR, nA, argc);
        return;
    }

    long idx = 0;
    for (long i = 0; i < nR; ++i) x->ratios[i] = ratioFromNorm((float)atom_getfloat(argv + idx++));
    x->mul = mulFromNorm((float)atom_getfloat(argv + idx++));
    x->offset = offsetFromNorm((float)atom_getfloat(argv + idx++), (float)x->beats);
    for (long i = 0; i < nA; ++i) x->ampratios[i] = ratioFromNorm((float)atom_getfloat(argv + idx++));

    meml_ratioseq_publish(x, false);
    meml_ratioseq_publish(x, true);
}

void meml_ratioseq_reset(t_meml_ratioseq* x) {
    x->phase = 0.;
}

void meml_ratioseq_dsp64(t_meml_ratioseq* x, t_object* dsp64, short* count, double samplerate,
                         long maxvectorsize, long flags) {
    x->sr = samplerate;
    x->phaseConnected = count[0];
    meml_ratioseq_update_inc(x);
    x->lastTrig = false;
    object_method(dsp64, gensym("dsp_add64"), x, meml_ratioseq_perform64, 0, NULL);
}

void meml_ratioseq_perform64(t_meml_ratioseq* x, t_object* dsp64, double** ins, long numins, double** outs,
                             long numouts, long sampleframes, long flags, void* userparam) {
    const double* in = ins[0];
    double* trigOut = outs[0];
    double* ampOut = outs[1];

    // One read of each parameter per vector, and one read of each ratio
    // pattern - the reference stays valid for the whole vector because both
    // slots outlive the object.
    const RatioPattern& ratios = x->state->ratios.current();
    const RatioPattern& ampRatios = x->state->ampRatios.current();
    const float mul = (float)x->mul;
    const float offset = (float)x->offset;
    const float pw = (float)x->pw;
    const bool useInput = x->phaseConnected != 0;
    const double inc = x->phaseInc;

    double phase = x->phase;
    bool lastTrig = x->lastTrig;
    bool changed = false;
    bool finalTrig = lastTrig;
    bool finalAmp = false;

    for (long i = 0; i < sampleframes; ++i) {
        float ph;
        if (useInput) {
            ph = (float)in[i];
        } else {
            ph = (float)phase;
            phase += inc;
            if (phase >= 1.) phase -= 1.;
        }

        bool trig = false;
        bool amp = false;
        ratioVoiceStep(ph, mul, offset, pw, ratios, ampRatios, trig, amp);

        trigOut[i] = trig ? 1.0 : 0.0;
        ampOut[i] = amp ? 1.0 : 0.0;

        if (trig != lastTrig) {
            changed = true;
            finalTrig = trig;
            finalAmp = amp;
        }
        lastTrig = trig;
    }

    x->phase = phase;
    x->lastTrig = lastTrig;

    // Note on / note off, off the audio thread. clock_delay re-arms, so if a
    // vector contains several edges only the last state is sent - at any
    // sensible tempo an edge per vector is already far more than a player
    // hears, and it keeps note-ons and note-offs balanced.
    if (changed) {
        x->pendingVel = finalTrig ? velocityFor(finalAmp) : 0;
        clock_delay(x->clock, 0);
    }
}

void meml_ratioseq_tick(t_meml_ratioseq* x) {
    outlet_int(x->out_vel, x->pendingVel);
}
