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
//           write [name] : save the learned state (network weights and
//                     biases, replay memory, engine parameters) as a JSON
//                     file. With no name, a save dialog opens. Same file
//                     format idea as pattrstorage: a Max dictionary, so it
//                     can also be opened with [dict] or any text editor.
//           read [name] : load a file written by `write`. With no name, an
//                     open dialog appears; with one, it is looked up along
//                     Max's search path. A file whose network shape does not
//                     match this object is refused outright rather than
//                     loaded in part.
//           store <n> / recall <n> / erase <n> : the pattrstorage idea -
//                     keep numbered snapshots of the learned state in memory
//                     and jump between them live. Slots travel inside the
//                     same file as the live state.
//           slots   : report the occupied slot numbers out the right outlet
//           like / dislike : store the current (input, action) pair as a
//                     liked / disliked experience (one-shot gestures)
//           randomise      : scramble the network weights
//           clear          : empty the replay memory
//           jolt 1 / jolt 0: hold / release the weight-morph gate (the
//                     hardware's momentary button; learning pauses while held)
//   outlet 0 (list) : n_outputs floats in [0,1], the mapped action
//   outlet 1 (int)  : replay-memory size, after every like/dislike/clear/read
//   outlet 2 (any)  : status replies - `slots <n> <n> ...` listing what is
//                     stored, and `slot <n>` after a store or recall
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
//     @file <name>     the state file this object belongs to: read at load
//                      time when @autoload is on, and used by a bare `write`
//                      or `read` instead of opening a dialog
//     @autoload 1      read @file when the patcher finishes loading
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
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "NISPSCore.h"
#include "NISPSStateFlat.h"

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

    // Numbered snapshots, sparse and 1-based like pattrstorage's slots. Held
    // in memory and written into the same file as the live state.
    std::map<long, NISPSState> slots;
};

typedef struct _nisps {
    t_object ob; // must be first
    State* state;
    void* clock;
    void* out_action; // left outlet: list
    void* out_memsize; // middle outlet: int
    void* out_info;    // right outlet: status replies
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
    t_symbol* file;
    long autoload;
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
static void nisps_adopt_params(t_nisps* x, const NISPSState& st);
void nisps_store(t_nisps* x, long n);
void nisps_recall(t_nisps* x, long n);
void nisps_erase(t_nisps* x, long n);
void nisps_slots(t_nisps* x);
void nisps_loadbang(t_nisps* x);
void nisps_read(t_nisps* x, t_symbol* s, long argc, t_atom* argv);
void nisps_doread(t_nisps* x, t_symbol* s, long argc, t_atom* argv);
void nisps_write(t_nisps* x, t_symbol* s, long argc, t_atom* argv);
void nisps_dowrite(t_nisps* x, t_symbol* s, long argc, t_atom* argv);
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
    class_addmethod(c, (method)nisps_read, "read", A_GIMME, 0);
    class_addmethod(c, (method)nisps_write, "write", A_GIMME, 0);
    class_addmethod(c, (method)nisps_store, "store", A_LONG, 0);
    class_addmethod(c, (method)nisps_recall, "recall", A_LONG, 0);
    class_addmethod(c, (method)nisps_erase, "erase", A_LONG, 0);
    class_addmethod(c, (method)nisps_erase, "delete", A_LONG, 0); // pattrstorage's spelling
    class_addmethod(c, (method)nisps_slots, "slots", 0);
    class_addmethod(c, (method)nisps_loadbang, "loadbang", A_CANT, 0);
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

    CLASS_ATTR_SYM(c, "file", 0, t_nisps, file);
    CLASS_ATTR_LABEL(c, "file", 0, "State File");

    CLASS_ATTR_LONG(c, "autoload", 0, t_nisps, autoload);
    CLASS_ATTR_STYLE_LABEL(c, "autoload", 0, "onoff", "Read @file At Load Time");

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
    x->file = gensym("");
    x->autoload = 1;

    // One-time allocation here; nothing allocates on the tick path afterwards
    // beyond what the core itself does while training.
    x->state = new State(static_cast<size_t>(x->n_inputs), static_cast<size_t>(x->n_outputs));

    // outlets, right to left
    x->out_info = outlet_new((t_object*)x, NULL); // any message
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
        snprintf(s, 256, "(list) %ld inputs; like, dislike, randomise, clear, jolt 0/1, "
                          "read, write, store/recall/erase <n>, slots, bang",
                 x->n_inputs);
    } else if (a == 0) {
        snprintf(s, 256, "(list) %ld mapped outputs in 0..1", x->n_outputs);
    } else if (a == 1) {
        snprintf(s, 256, "(int) replay memory size");
    } else {
        snprintf(s, 256, "(any) slots <n>... / slot <n>");
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

// --- persistence -----------------------------------------------------------
//
// The file is a Max dictionary written as JSON, the same idea as
// pattrstorage's .json files: readable, diffable, and openable with [dict].
// Everything is stored as flat number arrays under one "nisps" sub-dictionary,
// with the layer shapes recorded alongside so the arrays can be split and
// validated on the way back in. Nested per-layer dictionaries would read no
// better and would be far more code to get right.
//
// Both messages run on the main thread via defer_low: file dialogs and file
// I/O must not happen on the scheduler thread, where a `read` from a message
// box would otherwise land.

namespace {

const char* kRootKey = "nisps";

void dictAppendFloats(t_dictionary* d, const char* key, const std::vector<float>& v) {
    if (v.empty()) return; // a missing key reads back as empty
    t_atom* av = (t_atom*)sysmem_newptr(sizeof(t_atom) * v.size());
    if (!av) return;
    for (size_t i = 0; i < v.size(); ++i) atom_setfloat(av + i, v[i]);
    dictionary_appendatoms(d, gensym(key), (long)v.size(), av);
    sysmem_freeptr(av);
}

void dictAppendLongs(t_dictionary* d, const char* key, const std::vector<long>& v) {
    if (v.empty()) return;
    t_atom* av = (t_atom*)sysmem_newptr(sizeof(t_atom) * v.size());
    if (!av) return;
    for (size_t i = 0; i < v.size(); ++i) atom_setlong(av + i, v[i]);
    dictionary_appendatoms(d, gensym(key), (long)v.size(), av);
    sysmem_freeptr(av);
}

// The atoms belong to the dictionary; they are copied out, never freed here.
std::vector<float> dictGetFloats(const t_dictionary* d, const char* key) {
    std::vector<float> out;
    long ac = 0;
    t_atom* av = nullptr;
    if (dictionary_getatoms(d, gensym(key), &ac, &av) != MAX_ERR_NONE || !av) return out;
    out.reserve((size_t)ac);
    for (long i = 0; i < ac; ++i) out.push_back((float)atom_getfloat(av + i));
    return out;
}

std::vector<long> dictGetLongs(const t_dictionary* d, const char* key) {
    std::vector<long> out;
    long ac = 0;
    t_atom* av = nullptr;
    if (dictionary_getatoms(d, gensym(key), &ac, &av) != MAX_ERR_NONE || !av) return out;
    out.reserve((size_t)ac);
    for (long i = 0; i < ac; ++i) out.push_back((long)atom_getlong(av + i));
    return out;
}

t_dictionary* stateToInner(const NISPSState& st) {
    nisps_flat::Flat flat;
    nisps_flat::flatten(st, flat);

    t_dictionary* inner = dictionary_new();
    dictionary_appendlong(inner, gensym("version"), flat.version);
    dictionary_appendlong(inner, gensym("inputs"), flat.inputs);
    dictionary_appendlong(inner, gensym("outputs"), flat.outputs);

    dictAppendLongs(inner, "layer_shape", flat.layerShape);
    dictAppendFloats(inner, "weights", flat.weights);
    dictAppendFloats(inner, "biases", flat.biases);

    dictionary_appendlong(inner, gensym("memory_count"), flat.memoryCount);
    dictAppendFloats(inner, "memory_inputs", flat.memInputs);
    dictAppendFloats(inner, "memory_actions", flat.memActions);
    dictAppendFloats(inner, "memory_rewards", flat.memRewards);
    dictAppendFloats(inner, "memory_ages", flat.memAges);

    dictAppendFloats(inner, "input", flat.input);
    dictAppendFloats(inner, "action", flat.action);

    dictionary_appendfloat(inner, gensym("lrscale"), flat.lrscale);
    dictionary_appendfloat(inner, gensym("rewardscale"), flat.rewardscale);
    dictionary_appendfloat(inner, gensym("noise"), flat.noise);
    dictionary_appendlong(inner, gensym("traindivisor"), flat.traindivisor);
    dictionary_appendlong(inner, gensym("storemode"), flat.storemode);
    return inner;
}

// The live state at the top level, with any numbered slots beside it under
// "slots" - so one file is a whole set-up, as a pattrstorage file is. A file
// with no "slots" entry is perfectly valid and simply has none.
t_dictionary* stateToDict(const NISPSState& st, const std::map<long, NISPSState>& slots) {
    t_dictionary* inner = stateToInner(st);

    if (!slots.empty()) {
        t_dictionary* slotDict = dictionary_new();
        for (const auto& entry : slots) {
            t_dictionary* one = stateToInner(entry.second);
            dictionary_appenddictionary(slotDict, gensym(std::to_string(entry.first).c_str()),
                                        (t_object*)one); // slotDict now owns it
        }
        dictionary_appenddictionary(inner, gensym("slots"), (t_object*)slotDict);
    }

    t_dictionary* root = dictionary_new();
    dictionary_appenddictionary(root, gensym(kRootKey), (t_object*)inner); // root now owns inner
    return root;
}

// Pulls the keys out of the file and hands them to nisps_flat::unflatten,
// which does the validation - so what lives here is only the key names.
bool innerToState(t_nisps* x, t_dictionary* inner, const char* what, NISPSState& st) {
    nisps_flat::Flat flat;
    t_atom_long l = 0;
    double f = 0.;

    dictionary_getdeflong(inner, gensym("version"), &l, 0);
    flat.version = (int)l;
    dictionary_getdeflong(inner, gensym("inputs"), &l, 0);
    flat.inputs = (long)l;
    dictionary_getdeflong(inner, gensym("outputs"), &l, 0);
    flat.outputs = (long)l;

    flat.layerShape = dictGetLongs(inner, "layer_shape");
    flat.weights = dictGetFloats(inner, "weights");
    flat.biases = dictGetFloats(inner, "biases");

    dictionary_getdeflong(inner, gensym("memory_count"), &l, 0);
    flat.memoryCount = (long)l;
    flat.memInputs = dictGetFloats(inner, "memory_inputs");
    flat.memActions = dictGetFloats(inner, "memory_actions");
    flat.memRewards = dictGetFloats(inner, "memory_rewards");
    flat.memAges = dictGetFloats(inner, "memory_ages");

    flat.input = dictGetFloats(inner, "input");
    flat.action = dictGetFloats(inner, "action");

    dictionary_getdeffloat(inner, gensym("lrscale"), &f, 1.0);
    flat.lrscale = (float)f;
    dictionary_getdeffloat(inner, gensym("rewardscale"), &f, 1.0);
    flat.rewardscale = (float)f;
    dictionary_getdeffloat(inner, gensym("noise"), &f, 0.2);
    flat.noise = (float)f;
    dictionary_getdeflong(inner, gensym("traindivisor"), &l, 1);
    flat.traindivisor = (long)l;
    dictionary_getdeflong(inner, gensym("storemode"), &l, 2);
    flat.storemode = (long)l;

    // The file has to describe this object, not just a valid engine.
    if (flat.inputs != x->n_inputs || flat.outputs != x->n_outputs) {
        object_error((t_object*)x, "%s is %ld in / %ld out, this object is %ld in / %ld out",
                     what, flat.inputs, flat.outputs, x->n_inputs, x->n_outputs);
        return false;
    }

    std::string error;
    if (!nisps_flat::unflatten(flat, st, error)) {
        object_error((t_object*)x, "%s: %s", what, error.c_str());
        return false;
    }
    return true;
}

// Reads the whole file: the live state, then every slot. A bad slot fails the
// whole read rather than being dropped quietly - a file that silently loses
// half its presets is worse than one that says what is wrong with it.
bool dictToState(t_nisps* x, t_dictionary* root, NISPSState& st, std::map<long, NISPSState>& slots) {
    t_object* innerObj = nullptr;
    if (dictionary_getdictionary(root, gensym(kRootKey), &innerObj) != MAX_ERR_NONE || !innerObj) {
        object_error((t_object*)x, "not a nisps state file (no \"%s\" entry)", kRootKey);
        return false;
    }
    t_dictionary* inner = (t_dictionary*)innerObj;

    if (!innerToState(x, inner, "state file", st)) return false;

    slots.clear();
    t_object* slotObj = nullptr;
    if (dictionary_getdictionary(inner, gensym("slots"), &slotObj) != MAX_ERR_NONE || !slotObj) {
        return true; // no slots in this file
    }
    t_dictionary* slotDict = (t_dictionary*)slotObj;

    long numKeys = 0;
    t_symbol** keys = nullptr;
    if (dictionary_getkeys(slotDict, &numKeys, &keys) != MAX_ERR_NONE || !keys) return true;

    bool ok = true;
    for (long i = 0; i < numKeys && ok; ++i) {
        const long slot = atol(keys[i]->s_name);
        if (slot < 1) {
            object_error((t_object*)x, "slot name \"%s\" is not a positive number", keys[i]->s_name);
            ok = false;
            break;
        }
        t_object* oneObj = nullptr;
        if (dictionary_getdictionary(slotDict, keys[i], &oneObj) != MAX_ERR_NONE || !oneObj) {
            object_error((t_object*)x, "slot %ld is not a dictionary", slot);
            ok = false;
            break;
        }
        char what[64];
        snprintf(what, sizeof(what), "slot %ld", slot);
        NISPSState slotState;
        if (!innerToState(x, (t_dictionary*)oneObj, what, slotState)) {
            ok = false;
            break;
        }
        slots[slot] = std::move(slotState);
    }
    dictionary_freekeys(slotDict, numKeys, keys);
    if (!ok) slots.clear();
    return ok;
}

} // namespace

void nisps_write(t_nisps* x, t_symbol* s, long argc, t_atom* argv) {
    defer_low(x, (method)nisps_dowrite, s, (short)argc, argv);
}

void nisps_dowrite(t_nisps* x, t_symbol* s, long argc, t_atom* argv) {
    char filename[MAX_FILENAME_CHARS];
    short path = 0;
    t_fourcc type = 0;

    // A name in the message wins; then @file; only otherwise a dialog. This
    // is how pattrstorage behaves, and it is what makes an autoloaded file
    // saveable again with a bare `write`.
    if (argc && atom_gettype(argv) == A_SYM) {
        strncpy_zero(filename, atom_getsym(argv)->s_name, MAX_FILENAME_CHARS);
    } else if (x->file && x->file != gensym("") && *x->file->s_name) {
        strncpy_zero(filename, x->file->s_name, MAX_FILENAME_CHARS);
    } else {
        strncpy_zero(filename, "nisps-state.json", MAX_FILENAME_CHARS);
        if (saveasdialog_extended(filename, &path, &type, NULL, 0)) return; // cancelled
    }

    NISPSState st;
    std::map<long, NISPSState> slots;
    {
        std::lock_guard<std::mutex> guard(x->state->lock);
        x->state->core.captureState(st);
        slots = x->state->slots; // copied under the lock, written out below
    }

    t_dictionary* d = stateToDict(st, slots);
    const t_max_err err = dictionary_write(d, filename, path);
    object_free(d);

    if (err != MAX_ERR_NONE)
        object_error((t_object*)x, "could not write %s (error %ld)", filename, (long)err);
    else
        object_post((t_object*)x, "wrote %s (%ld memories, %ld slots)", filename,
                    (long)st.memory.size(), (long)slots.size());
}

void nisps_read(t_nisps* x, t_symbol* s, long argc, t_atom* argv) {
    defer_low(x, (method)nisps_doread, s, (short)argc, argv);
}

void nisps_doread(t_nisps* x, t_symbol* s, long argc, t_atom* argv) {
    char filename[MAX_FILENAME_CHARS];
    short path = 0;
    t_fourcc type = 0;

    if (argc && atom_gettype(argv) == A_SYM) {
        strncpy_zero(filename, atom_getsym(argv)->s_name, MAX_FILENAME_CHARS);
        if (locatefile_extended(filename, &path, &type, NULL, 0)) {
            object_error((t_object*)x, "can't find %s", filename);
            return;
        }
    } else if (x->file && x->file != gensym("") && *x->file->s_name) {
        strncpy_zero(filename, x->file->s_name, MAX_FILENAME_CHARS);
        if (locatefile_extended(filename, &path, &type, NULL, 0)) {
            object_error((t_object*)x, "can't find %s", filename);
            return;
        }
    } else {
        filename[0] = '\0';
        if (open_dialog(filename, &path, &type, NULL, 0)) return; // cancelled
    }

    t_dictionary* d = nullptr;
    if (dictionary_read(filename, path, &d) != MAX_ERR_NONE || !d) {
        object_error((t_object*)x, "could not read %s as a dictionary", filename);
        if (d) object_free(d);
        return;
    }

    NISPSState st;
    std::map<long, NISPSState> slots;
    const bool parsed = dictToState(x, d, st, slots); // reports its own reason on failure
    object_free(d);
    if (!parsed) return;

    bool restored = false;
    {
        std::lock_guard<std::mutex> guard(x->state->lock);
        restored = x->state->core.restoreState(st);
        if (restored) x->state->slots = std::move(slots);
    }
    if (!restored) {
        object_error((t_object*)x, "%s did not match this engine's shape; nothing was changed", filename);
        return;
    }

    nisps_adopt_params(x, st);

    object_post((t_object*)x, "read %s (%ld memories, %ld slots)", filename,
                (long)st.memory.size(), (long)slots.size());
    nisps_report_memsize(x);
    nisps_slots(x);
}

// --- slots ------------------------------------------------------------------
//
// pattrstorage's model: numbered snapshots kept in memory, jumped between
// live, and saved inside the one file. A slot here holds everything `write`
// holds - the network, the replay memory and the engine parameters - so
// recalling one is a complete change of mapping, not just a parameter set.

// Push a restored state's parameters into the attributes. They are the source
// of truth: the tick writes them into the engine on every pass, so a restore
// that skipped this would be undone within milliseconds.
static void nisps_adopt_params(t_nisps* x, const NISPSState& st) {
    x->lrscale = st.learningRateScale;
    x->rewardscale = st.rewardScale;
    x->noise = st.noiseLevel;
    x->traindivisor = (long)st.optimiseDivisor;
    x->storemode = st.memoryStoreMode;
}

void nisps_store(t_nisps* x, long n) {
    if (n < 1) {
        object_error((t_object*)x, "slot numbers start at 1");
        return;
    }
    NISPSState st;
    size_t count = 0;
    {
        std::lock_guard<std::mutex> guard(x->state->lock);
        x->state->core.captureState(st);
        count = st.memory.size();
        x->state->slots[n] = std::move(st);
    }
    object_post((t_object*)x, "stored slot %ld (%ld memories)", n, (long)count);

    t_atom a;
    atom_setlong(&a, n);
    outlet_anything(x->out_info, gensym("slot"), 1, &a);
}

void nisps_recall(t_nisps* x, long n) {
    if (n < 1) {
        object_error((t_object*)x, "slot numbers start at 1");
        return;
    }
    bool found = false;
    bool restored = false;
    NISPSState st;
    {
        std::lock_guard<std::mutex> guard(x->state->lock);
        auto it = x->state->slots.find(n);
        if (it != x->state->slots.end()) {
            found = true;
            st = it->second;
            restored = x->state->core.restoreState(st);
        }
    }
    if (!found) {
        object_warn((t_object*)x, "slot %ld is empty", n);
        return;
    }
    if (!restored) {
        object_error((t_object*)x, "slot %ld does not fit this engine; nothing was changed", n);
        return;
    }

    nisps_adopt_params(x, st);
    nisps_report_memsize(x);

    t_atom a;
    atom_setlong(&a, n);
    outlet_anything(x->out_info, gensym("slot"), 1, &a);
}

void nisps_erase(t_nisps* x, long n) {
    bool erased = false;
    {
        std::lock_guard<std::mutex> guard(x->state->lock);
        erased = (x->state->slots.erase(n) > 0);
    }
    if (!erased) object_warn((t_object*)x, "slot %ld is already empty", n);
    nisps_slots(x);
}

void nisps_slots(t_nisps* x) {
    std::vector<t_atom> out;
    {
        std::lock_guard<std::mutex> guard(x->state->lock);
        out.resize(x->state->slots.size());
        long i = 0;
        for (const auto& entry : x->state->slots) atom_setlong(&out[i++], entry.first);
    }
    outlet_anything(x->out_info, gensym("slots"), (short)out.size(), out.empty() ? NULL : out.data());
}

// Max calls this once the patcher has finished loading, which is the point at
// which the file is actually findable relative to it. An object created by
// scripting never gets a loadbang, so such a patch should send `read` itself.
void nisps_loadbang(t_nisps* x) {
    if (!x->autoload) return;
    if (!x->file || x->file == gensym("") || !*x->file->s_name) return;

    t_atom a;
    atom_setsym(&a, x->file);
    nisps_read(x, gensym("read"), 1, &a); // defers to the main thread itself
}
