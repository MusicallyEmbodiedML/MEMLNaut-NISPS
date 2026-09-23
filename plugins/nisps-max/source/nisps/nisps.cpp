// nisps - Max/MSP port of the NISPS (Neural Interactive Shaping of Parameter
// Spaces) RL mapper: the same NISPSCore engine the NISPS SuperCollider UGen
// wraps (../../../nisps-sc), from the MEMLNaut-NISPS embedded firmware whose
// src/memllib/examples/InterfaceRL this all descends from.
//
// This is a control-rate Max object (t_object, no MSP): the SC UGen runs at
// kr, and Max's equivalent of kr is scheduler-thread messages. An internal
// clock ticks the engine at @interval ms (default 5 ms = the firmware's 200 Hz
// training loop); each tick trains, maps the current input vector and emits
// the action list if it changed.
//
//   nisps [n_inputs=2] [n_outputs=4] [@attrs]
//
//   inlet 0 (list)  : n_inputs floats in [0,1] - the control vector to map
//                     (joystick axes, listening features, CCs...)
//           (float) : same, for a 1-input object
//           (bang)  : run one tick now (train + map + output). With @active 0
//                     this is how to drive it from your own metro.
//           like / dislike : store the current (input, action) pair as a
//                     liked / disliked experience (one-shot gestures)
//           randomise      : scramble the network weights
//           clear          : empty the replay memory
//           jolt 1 / jolt 0: hold / release the weight-morph gate (the
//                     hardware's momentary button; learning pauses while held)
//   outlet 0 (list) : n_outputs floats in [0,1], the mapped action
//   outlet 1 (int)  : replay-memory size, after every like/dislike/clear
//
//   attributes
//     @active 1        run the internal clock (0 = tick only on bang)
//     @interval 5.     tick period in ms
//     @lrscale 1.      learning-rate multiplier
//     @rewardscale 1.  reward multiplier applied to like/dislike
//     @noise 0.2       exploration noise level, 0..1 (0 = off)
//     @traindivisor 1  train once every N ticks
//     @storemode 2     replay-memory dedup policy: 0 add, 1 replace5, 2
//                      replace10, 3 replace15, 4 decay10, 5 decay20
//
// Arguments vs attributes follow the SC UGen: n_inputs/n_outputs size the MLP
// so they are creation arguments; everything the UGen took as a kr input is
// an attribute here. Gesture inputs that SC edge-detected are plain messages.
//
// Threading: messages arrive on the main or scheduler thread, the clock tick
// on the scheduler thread, and NISPSCore has no locking of its own (SC
// serialised everything on one thread), so every access goes through a
// mutex. No audio thread is involved. Outlet calls happen after the lock is
// released so a patch that feeds the output back into the inlet can't
// deadlock.

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ext.h"
#include "ext_obex.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <vector>

#include "NISPSCore.h"

// C++ state with constructors lives behind a pointer; object_alloc doesn't
// run constructors and nisps_free doesn't run destructors.
struct State {
    State(size_t nIn, size_t nOut) : core(nIn, nOut), input(nIn, 0.f), lastOut(nOut, -1.f), outAtoms(nOut) {}
    std::mutex lock;
    NISPSCore core;
    std::vector<float> input;   // latest control vector from the inlet
    std::vector<float> lastOut; // last action list emitted (for output-on-change)
    std::vector<t_atom> outAtoms;
    double lastTickMs = -1.0;   // scheduler time of the previous tick, <0 = none yet
    bool warnedLength = false;  // one-off warning for a wrongly sized list
};

typedef struct _nisps {
    t_object ob; // must be first
    State* state;
    void* clock;
    void* out_action; // left outlet: list
    void* out_memsize; // right outlet: int
    long n_inputs;
    long n_outputs;
    // attributes (Max reads these by offset; types must match the macros)
    long active;
    double interval;
    double lrscale;
    double rewardscale;
    double noise;
    long traindivisor;
    long storemode;
} t_nisps;

static t_class* nisps_class = nullptr;

void* nisps_new(t_symbol* s, long argc, t_atom* argv);
void nisps_free(t_nisps* x);
void nisps_assist(t_nisps* x, void* b, long m, long a, char* s);
void nisps_list(t_nisps* x, t_symbol* s, long argc, t_atom* argv);
void nisps_float(t_nisps* x, double f);
void nisps_int(t_nisps* x, long n);
void nisps_bang(t_nisps* x);
void nisps_like(t_nisps* x);
void nisps_dislike(t_nisps* x);
void nisps_randomise(t_nisps* x);
void nisps_clear(t_nisps* x);
void nisps_jolt(t_nisps* x, long on);
void nisps_tick(t_nisps* x);
t_max_err nisps_set_active(t_nisps* x, t_object* attr, long argc, t_atom* argv);
t_max_err nisps_set_interval(t_nisps* x, t_object* attr, long argc, t_atom* argv);

void C74_EXPORT ext_main(void* r) {
    t_class* c = class_new("nisps", (method)nisps_new, (method)nisps_free, (long)sizeof(t_nisps), 0L, A_GIMME, 0);

    class_addmethod(c, (method)nisps_list, "list", A_GIMME, 0);
    class_addmethod(c, (method)nisps_float, "float", A_FLOAT, 0);
    class_addmethod(c, (method)nisps_int, "int", A_LONG, 0);
    class_addmethod(c, (method)nisps_bang, "bang", 0);
    class_addmethod(c, (method)nisps_like, "like", 0);
    class_addmethod(c, (method)nisps_dislike, "dislike", 0);
    class_addmethod(c, (method)nisps_randomise, "randomise", 0);
    class_addmethod(c, (method)nisps_randomise, "randomize", 0); // US spelling alias
    class_addmethod(c, (method)nisps_clear, "clear", 0);
    class_addmethod(c, (method)nisps_jolt, "jolt", A_DEFLONG, 0);
    class_addmethod(c, (method)nisps_assist, "assist", A_CANT, 0);

    CLASS_ATTR_LONG(c, "active", 0, t_nisps, active);
    CLASS_ATTR_STYLE_LABEL(c, "active", 0, "onoff", "Run Internal Clock");
    CLASS_ATTR_ACCESSORS(c, "active", NULL, nisps_set_active);

    CLASS_ATTR_DOUBLE(c, "interval", 0, t_nisps, interval);
    CLASS_ATTR_LABEL(c, "interval", 0, "Tick Interval (ms)");
    CLASS_ATTR_ACCESSORS(c, "interval", NULL, nisps_set_interval);

    CLASS_ATTR_DOUBLE(c, "lrscale", 0, t_nisps, lrscale);
    CLASS_ATTR_FILTER_MIN(c, "lrscale", 0.0);
    CLASS_ATTR_LABEL(c, "lrscale", 0, "Learning Rate Scale");

    CLASS_ATTR_DOUBLE(c, "rewardscale", 0, t_nisps, rewardscale);
    CLASS_ATTR_FILTER_MIN(c, "rewardscale", 0.0);
    CLASS_ATTR_LABEL(c, "rewardscale", 0, "Reward Scale");

    CLASS_ATTR_DOUBLE(c, "noise", 0, t_nisps, noise);
    CLASS_ATTR_FILTER_CLIP(c, "noise", 0.0, 1.0);
    CLASS_ATTR_LABEL(c, "noise", 0, "Exploration Noise");

    CLASS_ATTR_LONG(c, "traindivisor", 0, t_nisps, traindivisor);
    CLASS_ATTR_FILTER_MIN(c, "traindivisor", 1);
    CLASS_ATTR_LABEL(c, "traindivisor", 0, "Train Every N Ticks");

    CLASS_ATTR_LONG(c, "storemode", 0, t_nisps, storemode);
    CLASS_ATTR_ENUMINDEX6(c, "storemode", 0, "add", "replace5", "replace10", "replace15", "decay10", "decay20");
    CLASS_ATTR_FILTER_CLIP(c, "storemode", 0, 5);
    CLASS_ATTR_LABEL(c, "storemode", 0, "Memory Store Mode");

    class_register(CLASS_BOX, c);
    nisps_class = c;
}

void* nisps_new(t_symbol* s, long argc, t_atom* argv) {
    t_nisps* x = (t_nisps*)object_alloc(nisps_class);
    if (!x) return nullptr;

    const long nPositional = attr_args_offset((short)argc, argv);
    x->n_inputs = 2;
    x->n_outputs = 4;
    if (nPositional > 0) x->n_inputs = std::max<long>(1, atom_getlong(argv));
    if (nPositional > 1) x->n_outputs = std::max<long>(1, atom_getlong(argv + 1));

    // Defaults mirror NISPS.sc's *kr argument defaults; interval mirrors the
    // firmware's 200 Hz loop rather than SC's control rate.
    x->active = 1;
    x->interval = 5.0;
    x->lrscale = 1.0;
    x->rewardscale = 1.0;
    x->noise = 0.2;
    x->traindivisor = 1;
    x->storemode = 2; // REPLACE_10_PERCENT, NISPSCore's default

    // One-time allocation here; nothing allocates on the tick path afterwards
    // beyond what the core itself does while training.
    x->state = new State(static_cast<size_t>(x->n_inputs), static_cast<size_t>(x->n_outputs));

    // outlets, right to left
    x->out_memsize = intout((t_object*)x);
    x->out_action = listout((t_object*)x);

    // the clock must exist before attr_args_process: @active's setter arms it
    x->clock = clock_new(x, (method)nisps_tick);

    attr_args_process(x, (short)argc, argv);
    if (x->active) clock_fdelay(x->clock, 0.0);
    return x;
}

void nisps_free(t_nisps* x) {
    clock_unset(x->clock);
    object_free(x->clock);
    delete x->state;
    x->state = nullptr;
}

void nisps_assist(t_nisps* x, void* b, long m, long a, char* s) {
    if (m == ASSIST_INLET) {
        snprintf(s, 256, "(list) %ld inputs; like, dislike, randomise, clear, jolt 0/1, bang", x->n_inputs);
    } else if (a == 0) {
        snprintf(s, 256, "(list) %ld mapped outputs in 0..1", x->n_outputs);
    } else {
        snprintf(s, 256, "(int) replay memory size");
    }
}

// Push the attribute values into the core. Called under the lock, at the top
// of every tick and before a gesture (the reward scale applies at store time).
static void nisps_apply_attrs(t_nisps* x) {
    NISPSCore& core = x->state->core;
    core.setLearningRateScale(static_cast<float>(x->lrscale));
    core.setRewardScale(static_cast<float>(x->rewardscale));
    core.setNoiseLevel(static_cast<float>(x->noise));
    core.setOptimiseDivisor(static_cast<size_t>(std::max<long>(1, x->traindivisor)));
    core.setMemoryStoreMode(static_cast<NISPSCore::MemoryStoreMode>(std::clamp<long>(x->storemode, 0, 5)));
}

// One engine step: advance the engine clock by real elapsed scheduler time,
// train/map, and emit the action if it changed (or if forced). Runs on the
// scheduler thread from the clock, or on whichever thread sent a bang.
static void nisps_step(t_nisps* x, bool forceOutput) {
    State* st = x->state;
    bool emit = false;
    long nOut = 0;
    {
        std::lock_guard<std::mutex> guard(st->lock);
        double nowMs = 0.0;
        clock_getftime(&nowMs);
        if (st->lastTickMs >= 0.0) st->core.advanceClock(std::max(0.0, nowMs - st->lastTickMs));
        st->lastTickMs = nowMs;

        nisps_apply_attrs(x);
        st->core.setInputs(st->input);
        st->core.process();

        const std::vector<float>& action = st->core.getAction();
        nOut = static_cast<long>(std::min(action.size(), st->outAtoms.size()));
        for (long i = 0; i < nOut; ++i) {
            if (action[i] != st->lastOut[i]) emit = true;
            st->lastOut[i] = action[i];
        }
        if (emit || forceOutput) {
            for (long i = 0; i < nOut; ++i) atom_setfloat(&st->outAtoms[i], action[i]);
            emit = true;
        }
    }
    // outside the lock: an outlet call may re-enter this object
    if (emit && nOut > 0) outlet_list(x->out_action, NULL, static_cast<short>(nOut), st->outAtoms.data());
}

void nisps_tick(t_nisps* x) {
    nisps_step(x, false);
    if (x->active) clock_fdelay(x->clock, std::max(1.0, x->interval));
}

void nisps_bang(t_nisps* x) {
    nisps_step(x, true);
}

void nisps_list(t_nisps* x, t_symbol* s, long argc, t_atom* argv) {
    State* st = x->state;
    std::lock_guard<std::mutex> guard(st->lock);
    const long n = static_cast<long>(st->input.size());
    if (argc != n && !st->warnedLength) {
        object_warn((t_object*)x, "expected a list of %ld values, got %ld (extra ignored, missing left as-is)", n, argc);
        st->warnedLength = true;
    }
    const long m = std::min(argc, n);
    for (long i = 0; i < m; ++i) st->input[i] = static_cast<float>(atom_getfloat(argv + i));
}

void nisps_float(t_nisps* x, double f) {
    t_atom a;
    atom_setfloat(&a, f);
    nisps_list(x, NULL, 1, &a);
}

void nisps_int(t_nisps* x, long n) {
    nisps_float(x, static_cast<double>(n));
}

static void nisps_report_memsize(t_nisps* x) {
    long size;
    {
        std::lock_guard<std::mutex> guard(x->state->lock);
        size = static_cast<long>(x->state->core.memorySize());
    }
    outlet_int(x->out_memsize, size);
}

void nisps_like(t_nisps* x) {
    {
        std::lock_guard<std::mutex> guard(x->state->lock);
        nisps_apply_attrs(x);
        x->state->core.like();
    }
    nisps_report_memsize(x);
}

void nisps_dislike(t_nisps* x) {
    {
        std::lock_guard<std::mutex> guard(x->state->lock);
        nisps_apply_attrs(x);
        x->state->core.dislike();
    }
    nisps_report_memsize(x);
}

void nisps_randomise(t_nisps* x) {
    std::lock_guard<std::mutex> guard(x->state->lock);
    x->state->core.randomiseNetwork();
}

void nisps_clear(t_nisps* x) {
    {
        std::lock_guard<std::mutex> guard(x->state->lock);
        x->state->core.clearMemory();
    }
    nisps_report_memsize(x);
}

// "jolt 1" = press, "jolt 0" = release (a bare "jolt" is A_DEFLONG 0, i.e.
// release). The morph itself happens inside core.process() on every tick
// while the gate is held.
void nisps_jolt(t_nisps* x, long on) {
    std::lock_guard<std::mutex> guard(x->state->lock);
    NISPSCore& core = x->state->core;
    if (on && !core.isJoltActive()) core.startJolt();
    else if (!on && core.isJoltActive()) core.stopJolt();
}

t_max_err nisps_set_active(t_nisps* x, t_object* attr, long argc, t_atom* argv) {
    if (argc && argv) {
        const long wasActive = x->active;
        x->active = atom_getlong(argv) ? 1 : 0;
        if (x->active && !wasActive) clock_fdelay(x->clock, 0.0);
        else if (!x->active) clock_unset(x->clock);
    }
    return MAX_ERR_NONE;
}

t_max_err nisps_set_interval(t_nisps* x, t_object* attr, long argc, t_atom* argv) {
    if (argc && argv) x->interval = std::max(1.0, atom_getfloat(argv)); // custom setter bypasses FILTER_*
    return MAX_ERR_NONE;
}
