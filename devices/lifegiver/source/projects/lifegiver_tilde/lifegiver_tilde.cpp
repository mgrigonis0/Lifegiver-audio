// lifegiver~ — Max wrapper around the Lifegiver engine (one object per device).
//
// Argument: the device prefix. The patch writes "lifegiver~ ---lfg"; Live turns "---" into a per-device number, so
//   every name the object makes (its state dict) is unique per device and two Lifegivers never share anything.
// Inlets:  0 = left audio + every message, 1 = right audio. Messages (see Controller::message), plus
//   init                     the device finished loading (live.thisdevice): pull the saved state
//   dictionary <name>        the saved state, from the pattr
// Outlets: 0 left audio, 1 right audio, 2 info for the patch: getstate, dictionary <name> (to the pattr)
#include "ext.h"
#include "ext_obex.h"
#include "ext_dictobj.h"
#include "z_dsp.h"

#include "../../engine/controller.h"

#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

using namespace lg;

struct t_lifegiver;

class Glue : public Host {
public:
    t_lifegiver* x;
    explicit Glue(t_lifegiver* o) : x(o) {}
    void saveState() override;
    void log(const std::string& t) override;
};

struct t_lifegiver {
    t_pxobject ob;
    Processor* pr;
    Controller* ctl;
    Glue* glue;
    std::recursive_mutex* mx;    // the controller is only touched on the main thread; this guards clock vs messages
    void* infoOut;
    void* tickClock;
    t_symbol* prefix;
    t_dictionary* stateDict;
    t_symbol* stateName;
    bool initDone, gotState, saving;
    double initAt;
    uint64_t lastHash;
};

static t_class* s_class = nullptr;

static double now_ms() { return systimer_gettime(); }

// ---------------------------------------------------------------------------------------------------
void Glue::saveState() {
    if (!x->stateDict) return;
    StateDoc d;
    x->ctl->writeState(d);
    // nothing changed since the last save: no new step in Live's undo history
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](const void* p, size_t n) { const unsigned char* c = (const unsigned char*)p; for (size_t i = 0; i < n; i++) { h ^= c[i]; h *= 1099511628211ull; } };
    for (auto& kv : d.num) { mix(kv.first.data(), kv.first.size()); mix(kv.second.data(), kv.second.size() * sizeof(double)); }
    for (auto& kv : d.str) { mix(kv.first.data(), kv.first.size()); mix(kv.second.data(), kv.second.size()); }
    if (h == x->lastHash) return;
    x->lastHash = h;
    dictionary_clear(x->stateDict);
    std::vector<t_atom> at;
    for (auto& kv : d.num) {
        at.resize(kv.second.size());
        for (size_t i = 0; i < kv.second.size(); i++) atom_setfloat(&at[i], kv.second[i]);
        if (at.empty()) { t_atom z; atom_setfloat(&z, 0); dictionary_appendatoms(x->stateDict, gensym(kv.first.c_str()), 0, &z); }
        else dictionary_appendatoms(x->stateDict, gensym(kv.first.c_str()), (long)at.size(), at.data());
    }
    for (auto& kv : d.str) dictionary_appendsym(x->stateDict, gensym(kv.first.c_str()), gensym(kv.second.c_str()));
    x->saving = true;
    t_atom a; atom_setsym(&a, x->stateName);
    outlet_anything(x->infoOut, gensym("dictionary"), 1, &a);
    x->saving = false;
}

void Glue::log(const std::string& t) { object_post((t_object*)x, "%s", t.c_str()); }

// ---------------------------------------------------------------------------------------------------
static void lg_tick(t_lifegiver* x) {
    const double ms = now_ms();
    {
        std::lock_guard<std::recursive_mutex> g(*x->mx);
        // nothing came from the set within 1.5 s of loading: a new device
        if (x->initDone && !x->gotState && ms >= x->initAt + 1500) { x->gotState = true; x->ctl->noState(); }
        x->ctl->tick(ms);
    }
    clock_fdelay(x->tickClock, 15.);
}

static void lg_anything(t_lifegiver* x, t_symbol* s, long argc, t_atom* argv) {
    std::vector<Controller::Atom> a;
    for (long i = 0; i < argc; i++) {
        Controller::Atom m;
        if (atom_gettype(argv + i) == A_SYM) { m.num = false; m.s = atom_getsym(argv + i)->s_name; }
        else m.d = atom_getfloat(argv + i);
        a.push_back(m);
    }
    std::lock_guard<std::recursive_mutex> g(*x->mx);
    x->ctl->message(s->s_name, a);
}

static void lg_init(t_lifegiver* x) {
    if (x->initDone) return;
    x->initDone = true;
    x->initAt = now_ms();
    outlet_anything(x->infoOut, gensym("getstate"), 0, nullptr);   // the pattr answers with "dictionary <name>"
}

static void lg_dictionary(t_lifegiver* x, t_symbol* name) {
    // our own dict echoing back, or a second answer: the state is read once per load
    if (x->saving || name == x->stateName || x->gotState) return;
    t_dictionary* d = dictobj_findregistered_retain(name);
    if (!d) return;
    StateDoc doc;
    long n = 0;
    t_symbol** keys = nullptr;
    dictionary_getkeys(d, &n, &keys);
    for (long i = 0; i < n; i++) {
        t_symbol* k = keys[i];
        if (dictionary_entryisstring(d, k)) {
            t_symbol* v = nullptr;
            dictionary_getsym(d, k, &v);
            if (v) doc.str[k->s_name] = v->s_name;
            continue;
        }
        long ac = 0;
        t_atom* av = nullptr;
        if (dictionary_getatoms(d, k, &ac, &av) == MAX_ERR_NONE) {
            if (ac == 1 && atom_gettype(av) == A_SYM) { doc.str[k->s_name] = atom_getsym(av)->s_name; continue; }
            std::vector<double> v;
            for (long j = 0; j < ac; j++) v.push_back(atom_getfloat(av + j));
            doc.num[k->s_name] = v;
        }
    }
    if (keys) dictionary_freekeys(d, n, keys);
    dictobj_release(d);
    std::lock_guard<std::recursive_mutex> g(*x->mx);
    x->gotState = true;
    x->ctl->readState(doc);
}

static void lg_int(t_lifegiver* x, long v) { (void)x; (void)v; }      // pattr sends a bare 0 when nothing was saved
static void lg_float(t_lifegiver* x, double v) { (void)x; (void)v; }

// ---------------------------------------------------------------------------------------------------
static void lg_perform64(t_lifegiver* x, t_object*, double** ins, long numins, double** outs, long numouts, long n, long, void*) {
    if (numins < 2 || numouts < 2) return;
    x->pr->process(ins[0], ins[1], outs[0], outs[1], n);
}

static void lg_dsp64(t_lifegiver* x, t_object* dsp64, short*, double samplerate, long, long) {
    x->pr->prepare(samplerate);
    object_method(dsp64, gensym("dsp_add64"), x, lg_perform64, 0, NULL);
}

static void lg_assist(t_lifegiver*, void*, long m, long a, char* s) {
    if (m == ASSIST_INLET) std::snprintf(s, 256, "%s", a == 0 ? "(signal) Left in, control messages" : "(signal) Right in");
    else {
        const char* t[3] = {"(signal) Left out", "(signal) Right out", "Info for the patch"};
        std::snprintf(s, 256, "%s", t[a < 0 ? 0 : (a > 2 ? 2 : a)]);
    }
}

static void* lg_new(t_symbol*, long argc, t_atom* argv) {
    t_lifegiver* x = (t_lifegiver*)object_alloc(s_class);
    if (!x) return nullptr;
    dsp_setup((t_pxobject*)x, 2);
    x->infoOut = outlet_new((t_object*)x, NULL);
    outlet_new((t_object*)x, "signal");
    outlet_new((t_object*)x, "signal");
    x->ob.z_misc |= Z_NO_INPLACE;

    // "lifegiver~ ---lfg" arrives as e.g. "012lfg": the prefix is what replaced "---" (kept as text, zeros and all)
    x->prefix = gensym("lfg");
    if (argc > 0 && atom_gettype(argv) == A_SYM) {
        std::string a = atom_getsym(argv)->s_name;
        if (a.size() >= 3 && a.compare(a.size() - 3, 3, "lfg") == 0) a.resize(a.size() - 3);
        x->prefix = gensym(a.c_str());
    }
    x->mx = new std::recursive_mutex();
    x->pr = new Processor();
    x->pr->prepare(sys_getsr());
    x->glue = new Glue(x);
    x->ctl = new Controller(x->pr, x->glue);
    x->initDone = false; x->gotState = false; x->saving = false;
    x->initAt = 0; x->lastHash = 0;
    char dn[256];
    std::snprintf(dn, sizeof dn, "%slgstate", x->prefix->s_name);
    x->stateName = gensym(dn);
    x->stateDict = dictionary_new();
    x->stateDict = dictobj_register(x->stateDict, &x->stateName);
    x->tickClock = clock_new(x, (method)lg_tick);
    clock_fdelay(x->tickClock, 50.);
    post("lifegiver: engine %s ready, device %s", VERSION, x->prefix->s_name);
    return x;
}

static void lg_free(t_lifegiver* x) {
    dsp_free((t_pxobject*)x);
    clock_unset(x->tickClock);
    object_free(x->tickClock);
    if (x->stateDict) dictobj_unregister(x->stateDict);
    delete x->ctl;
    delete x->glue;
    delete x->pr;
    delete x->mx;
}

void ext_main(void*) {
    t_class* c = class_new("lifegiver~", (method)lg_new, (method)lg_free, (long)sizeof(t_lifegiver), 0L, A_GIMME, 0);
    class_addmethod(c, (method)lg_dsp64, "dsp64", A_CANT, 0);
    class_addmethod(c, (method)lg_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)lg_init, "init", 0);
    class_addmethod(c, (method)lg_dictionary, "dictionary", A_SYM, 0);
    class_addmethod(c, (method)lg_int, "int", A_LONG, 0);
    class_addmethod(c, (method)lg_float, "float", A_FLOAT, 0);
    class_addmethod(c, (method)lg_anything, "anything", A_GIMME, 0);
    class_dspinit(c);
    class_register(CLASS_BOX, c);
    s_class = c;
}
