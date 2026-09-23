// meml.euclid~ - the MEMLNaut's Euclidean rhythm generator as an MSP object.
//
// Ports the euclidean() function defined in modes/AudioApps/
// ElysiamorfAudioApp.hpp (MODE_ELYSIAMORFS) and MEMLCeliumAudioApp.hpp
// (MODE_MEMLCELIUM); the generator itself lives in ../common/RhythmCore.h so
// it can be tested without Max (../tests/rhythm_test.cpp).
//
//   meml.euclid~ [n=8] [k=3] [@attrs]
//
//   inlet 0  (signal) : phase 0..1, one ramp per cycle - patch a phasor~ in.
//                       With nothing connected, an internal phasor runs at
//                       @bpm / @beats (one cycle per bar).
//            (messages) every parameter below, plus:
//              norm <n> <k> <offset>   three 0..1 floats, mapped the way the
//                       firmware maps NN output (EuclideanAudioApp::
//                       VoiceOperator_): n snapped to a power of 2 or 3
//                       within @nrange, k within @krange, offset over 0..n-1
//              reset    restart the internal phasor at 0
//   outlet 0 (signal) : gate, 1 while a pulse sounds, 0 otherwise
//   outlet 1 (bang)   : one bang at each pulse onset (scheduler thread)
//   outlet 2 (int)    : the step index 0..n-1 that just fired, sent before
//                       the bang so it is set by the time the bang lands
//
//   attributes
//     @n 8            steps per cycle (1..64)
//     @k 3            pulses distributed over those steps (1..n)
//     @offset 0       rotate the pattern by this many steps
//     @pw 0.5         pulse width as a fraction of one step (0..1)
//     @bpm 120.       internal phasor tempo, used only when inlet 0 is unpatched
//     @beats 4.       beats per cycle for @bpm and for `norm`'s offset grid
//     @nrange 1 16    n limits used by `norm`
//     @krange 1 16    k limits used by `norm`
//
// Every parameter is a Max attribute, so all of them are settable at control
// rate by name (`n 12`, `pw 0.25`) from any patch cord, which is what makes
// the whole generator drivable from a mapping object such as nisps.

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

#include <algorithm>
#include <cstdio>

#include "RhythmCore.h"

using namespace memlrhythm;

typedef struct _meml_euclid {
    t_pxobject ob; // must be first, t_pxobject for MSP
    void* clock;
    void* out_bang;
    void* out_step;

    // attributes - Max reads these by offset, so the types must match the macros
    t_atom_long n;
    t_atom_long k;
    t_atom_long offset;
    double pw;
    double bpm;
    double beats;
    long nrange[2]; // CLASS_ATTR_LONG_VARSIZE requires literally `long`
    long nrange_count;
    long krange[2];
    long krange_count;

    // dsp state
    double phase;      // internal phasor, used when inlet 0 is unpatched
    double phaseInc;   // per sample, recomputed in dsp64 and by @bpm / @beats
    double sr;
    short phaseConnected;
    bool lastGate;

    // set by perform64, read by the clock callback
    t_atom_long pendingStep;
} t_meml_euclid;

static t_class* meml_euclid_class = nullptr;

void* meml_euclid_new(t_symbol* s, long argc, t_atom* argv);
void meml_euclid_free(t_meml_euclid* x);
void meml_euclid_assist(t_meml_euclid* x, void* b, long m, long a, char* s);
void meml_euclid_dsp64(t_meml_euclid* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags);
void meml_euclid_perform64(t_meml_euclid* x, t_object* dsp64, double** ins, long numins, double** outs,
                           long numouts, long sampleframes, long flags, void* userparam);
void meml_euclid_tick(t_meml_euclid* x);
void meml_euclid_norm(t_meml_euclid* x, t_symbol* s, long argc, t_atom* argv);
void meml_euclid_reset(t_meml_euclid* x);
t_max_err meml_euclid_set_n(t_meml_euclid* x, t_object* attr, long argc, t_atom* argv);
t_max_err meml_euclid_set_k(t_meml_euclid* x, t_object* attr, long argc, t_atom* argv);
t_max_err meml_euclid_set_bpm(t_meml_euclid* x, t_object* attr, long argc, t_atom* argv);

static void meml_euclid_update_inc(t_meml_euclid* x) {
    const double beats = (x->beats > 0.) ? x->beats : 4.;
    const double barHz = (x->bpm / 60.) / beats; // one phasor cycle per bar
    x->phaseInc = (x->sr > 0.) ? (barHz / x->sr) : 0.;
}

void C74_EXPORT ext_main(void* r) {
    t_class* c = class_new("meml.euclid~", (method)meml_euclid_new, (method)meml_euclid_free,
                           (long)sizeof(t_meml_euclid), 0L, A_GIMME, 0);

    class_addmethod(c, (method)meml_euclid_dsp64, "dsp64", A_CANT, 0);
    class_addmethod(c, (method)meml_euclid_norm, "norm", A_GIMME, 0);
    class_addmethod(c, (method)meml_euclid_reset, "reset", 0);
    class_addmethod(c, (method)meml_euclid_assist, "assist", A_CANT, 0);

    CLASS_ATTR_ATOM_LONG(c, "n", 0, t_meml_euclid, n);
    CLASS_ATTR_LABEL(c, "n", 0, "Steps Per Cycle");
    CLASS_ATTR_ACCESSORS(c, "n", NULL, meml_euclid_set_n);

    CLASS_ATTR_ATOM_LONG(c, "k", 0, t_meml_euclid, k);
    CLASS_ATTR_LABEL(c, "k", 0, "Pulses");
    CLASS_ATTR_ACCESSORS(c, "k", NULL, meml_euclid_set_k);

    CLASS_ATTR_ATOM_LONG(c, "offset", 0, t_meml_euclid, offset);
    CLASS_ATTR_LABEL(c, "offset", 0, "Rotation (steps)");

    CLASS_ATTR_DOUBLE(c, "pw", 0, t_meml_euclid, pw);
    CLASS_ATTR_FILTER_CLIP(c, "pw", 0., 1.);
    CLASS_ATTR_LABEL(c, "pw", 0, "Pulse Width");

    CLASS_ATTR_DOUBLE(c, "bpm", 0, t_meml_euclid, bpm);
    CLASS_ATTR_LABEL(c, "bpm", 0, "Internal Tempo (BPM)");
    CLASS_ATTR_ACCESSORS(c, "bpm", NULL, meml_euclid_set_bpm);

    CLASS_ATTR_DOUBLE(c, "beats", 0, t_meml_euclid, beats);
    CLASS_ATTR_LABEL(c, "beats", 0, "Beats Per Cycle");
    CLASS_ATTR_ACCESSORS(c, "beats", NULL, meml_euclid_set_bpm);

    CLASS_ATTR_LONG_VARSIZE(c, "nrange", 0, t_meml_euclid, nrange, nrange_count, 2);
    CLASS_ATTR_LABEL(c, "nrange", 0, "n Range For norm");

    CLASS_ATTR_LONG_VARSIZE(c, "krange", 0, t_meml_euclid, krange, krange_count, 2);
    CLASS_ATTR_LABEL(c, "krange", 0, "k Range For norm");

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    meml_euclid_class = c;
}

void* meml_euclid_new(t_symbol* s, long argc, t_atom* argv) {
    t_meml_euclid* x = (t_meml_euclid*)object_alloc(meml_euclid_class);
    if (!x) return nullptr;

    dsp_setup((t_pxobject*)x, 1); // one signal inlet: phase

    // outlets, right to left
    x->out_step = intout((t_object*)x);
    x->out_bang = bangout((t_object*)x);
    outlet_new((t_object*)x, "signal");

    x->clock = clock_new(x, (method)meml_euclid_tick);

    x->n = 8;
    x->k = 3;
    x->offset = 0;
    x->pw = 0.5;
    x->bpm = 120.;
    x->beats = 4.;
    x->nrange[0] = 1; x->nrange[1] = 16; x->nrange_count = 2;
    x->krange[0] = 1; x->krange[1] = 16; x->krange_count = 2;
    x->phase = 0.;
    x->sr = sys_getsr() > 0 ? sys_getsr() : 48000.;
    x->phaseConnected = 0;
    x->lastGate = false;
    x->pendingStep = 0;
    meml_euclid_update_inc(x);

    const long nPositional = attr_args_offset((short)argc, argv);
    if (nPositional > 0) x->n = std::clamp<t_atom_long>(atom_getlong(argv), 1, 64);
    if (nPositional > 1) x->k = std::clamp<t_atom_long>(atom_getlong(argv + 1), 1, x->n);

    attr_args_process(x, (short)argc, argv);
    return x;
}

void meml_euclid_free(t_meml_euclid* x) {
    dsp_free((t_pxobject*)x); // must come first
    clock_unset(x->clock);
    object_free(x->clock);
}

void meml_euclid_assist(t_meml_euclid* x, void* b, long m, long a, char* s) {
    if (m == ASSIST_INLET) {
        snprintf(s, 256, "(signal) phase 0..1, or messages: n, k, offset, pw, bpm, norm, reset");
    } else if (a == 0) {
        snprintf(s, 256, "(signal) gate, 1 while a pulse sounds");
    } else if (a == 1) {
        snprintf(s, 256, "(bang) at each pulse onset");
    } else {
        snprintf(s, 256, "(int) step index that fired");
    }
}

// Clamping setters. A custom setter bypasses CLASS_ATTR_FILTER_*, so the
// clamp has to live here.
t_max_err meml_euclid_set_n(t_meml_euclid* x, t_object* attr, long argc, t_atom* argv) {
    if (argc && argv) {
        x->n = std::clamp<t_atom_long>(atom_getlong(argv), 1, 64);
        if (x->k > x->n) x->k = x->n;
    }
    return MAX_ERR_NONE;
}

t_max_err meml_euclid_set_k(t_meml_euclid* x, t_object* attr, long argc, t_atom* argv) {
    if (argc && argv) x->k = std::clamp<t_atom_long>(atom_getlong(argv), 1, x->n);
    return MAX_ERR_NONE;
}

t_max_err meml_euclid_set_bpm(t_meml_euclid* x, t_object* attr, long argc, t_atom* argv) {
    if (argc && argv) {
        const t_symbol* name = (t_symbol*)object_method(attr, gensym("getname"));
        const double v = atom_getfloat(argv);
        if (name == gensym("beats")) x->beats = std::max(0.001, v);
        else x->bpm = std::clamp(v, 0.001, 9999.);
        meml_euclid_update_inc(x);
    }
    return MAX_ERR_NONE;
}

// Three 0..1 floats -> n, k, offset, mapped as the firmware maps NN output.
void meml_euclid_norm(t_meml_euclid* x, t_symbol* s, long argc, t_atom* argv) {
    if (argc < 3) {
        object_warn((t_object*)x, "norm needs 3 values (n k offset) in 0..1, got %ld", argc);
        return;
    }
    const long nMin = (x->nrange_count > 0) ? x->nrange[0] : 1;
    const long nMax = (x->nrange_count > 1) ? x->nrange[1] : 16;
    const long kMin = (x->krange_count > 0) ? x->krange[0] : 1;
    const long kMax = (x->krange_count > 1) ? x->krange[1] : 16;

    const EuclidParams p = euclidFromNorm((float)atom_getfloat(argv), (float)atom_getfloat(argv + 1),
                                          (float)atom_getfloat(argv + 2), nMin, nMax, kMin, kMax);
    x->n = p.n;
    x->k = p.k;
    x->offset = p.offset;
}

void meml_euclid_reset(t_meml_euclid* x) {
    x->phase = 0.;
}

void meml_euclid_dsp64(t_meml_euclid* x, t_object* dsp64, short* count, double samplerate,
                       long maxvectorsize, long flags) {
    x->sr = samplerate;
    x->phaseConnected = count[0];
    meml_euclid_update_inc(x);
    x->lastGate = false;
    object_method(dsp64, gensym("dsp_add64"), x, meml_euclid_perform64, 0, NULL);
}

void meml_euclid_perform64(t_meml_euclid* x, t_object* dsp64, double** ins, long numins, double** outs,
                           long numouts, long sampleframes, long flags, void* userparam) {
    const double* in = ins[0];
    double* out = outs[0];

    // Read the attributes once per vector, UGen style: aligned scalar reads
    // racing a main-thread write are benign, and this keeps a parameter change
    // from taking effect halfway through a vector.
    const long n = (long)x->n;
    const long k = (long)x->k;
    const long offset = (long)x->offset;
    const float pw = (float)x->pw;
    const bool useInput = x->phaseConnected != 0;
    const double inc = x->phaseInc;

    double phase = x->phase;
    bool lastGate = x->lastGate;
    bool fired = false;
    long firedStep = 0;

    for (long i = 0; i < sampleframes; ++i) {
        float ph;
        if (useInput) {
            ph = (float)in[i];
        } else {
            ph = (float)phase;
            phase += inc;
            if (phase >= 1.) phase -= 1.;
        }

        const bool gate = euclideanGate(ph, n, k, offset, pw);
        out[i] = gate ? 1.0 : 0.0;

        if (gate && !lastGate) {
            fired = true;
            firedStep = euclideanStep(ph, n);
        }
        lastGate = gate;
    }

    x->phase = phase;
    x->lastGate = lastGate;

    // No outlet calls from the audio thread: hand the onset to a clock, which
    // re-arms, so several onsets in one vector coalesce into one bang.
    if (fired) {
        x->pendingStep = firedStep;
        clock_delay(x->clock, 0);
    }
}

void meml_euclid_tick(t_meml_euclid* x) {
    outlet_int(x->out_step, x->pendingStep); // right outlet first
    outlet_bang(x->out_bang);
}
