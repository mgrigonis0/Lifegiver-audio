// lifegiver~ — Max wrapper around the Lifegiver engine (one object per device).
//
// Argument: the device prefix. The patch writes "lifegiver~ ---lfg"; Live turns "---" into a per-device number. Only
//   used in the Max window line; everything saved lives in the device's own hidden parameter (state.h).
// Inlets:  0 = left audio + every message, 1 = right audio. Messages: see Controller::message, plus
//   init                     the device finished loading (live.thisdevice)
// Outlets: 0 left audio, 1 right audio, 2 info for the patch:
//   getstate                 bang the hidden [pattr state] (it answers through [prepend statelist])
//   statelist <numbers>      store this list in [pattr state]
//   param <name> <value>     set a Live parameter (the role buttons' starting places)
//   takes / facts / knobs / moves / vars <numbers>   for the view (numbers only: no new symbols per update)
// Threads: the audio thread runs Processor::process (never allocates); the main thread runs the Controller (messages
// and a 15 ms clock); a worker thread runs the analysis and the solver (long jobs must never freeze Max's main thread).
#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

#include "../../engine/controller.h"

#include <condition_variable>
#include <cstdio>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace lg;

struct t_lifegiver;

struct Worker {
    std::thread th;
    std::mutex m;
    std::condition_variable cv;
    std::deque<std::pair<std::function<void()>, std::function<void()>>> jobs;   // work (worker), done (main)
    std::deque<std::function<void()>> done;
    bool quit = false;
};

class Glue : public Host {
public:
    t_lifegiver* x;
    explicit Glue(t_lifegiver* o) : x(o) {}
    void requestState() override;
    void storeState(const std::vector<double>& v) override;
    void log(const std::string& t) override { ::post("%s", t.c_str()); }
    void ui(const std::string& sel, const std::vector<double>& v) override;
    void param(const std::string& name, double v) override;
    void post(std::function<void()> work, std::function<void()> done) override;
};

struct t_lifegiver {
    t_pxobject ob;
    Processor* pr;
    Controller* ctl;
    Glue* glue;
    Worker* wk;
    std::recursive_mutex* mx;    // the controller is only touched on the main thread; this guards clock vs messages
    void* infoOut;
    void* tickClock;
    t_symbol* prefix;
};

static t_class* s_class = nullptr;

static double now_ms() { return systimer_gettime(); }

void Glue::requestState() { outlet_anything(x->infoOut, gensym("getstate"), 0, nullptr); }

void Glue::storeState(const std::vector<double>& v) {
    std::vector<t_atom> at(v.size());
    for (size_t i = 0; i < v.size(); i++) atom_setfloat(&at[i], v[i]);
    outlet_anything(x->infoOut, gensym("statelist"), (short)at.size(), at.data());
}

void Glue::ui(const std::string& sel, const std::vector<double>& v) {
    std::vector<t_atom> at(v.size());
    for (size_t i = 0; i < v.size(); i++) atom_setfloat(&at[i], v[i]);
    outlet_anything(x->infoOut, gensym(sel.c_str()), (short)at.size(), at.data());
}

void Glue::param(const std::string& name, double v) {
    t_atom at[2];
    atom_setsym(&at[0], gensym(name.c_str()));
    atom_setfloat(&at[1], v);
    outlet_anything(x->infoOut, gensym("param"), 2, at);
}

void Glue::post(std::function<void()> work, std::function<void()> done) {
    {
        std::lock_guard<std::mutex> lk(x->wk->m);
        x->wk->jobs.emplace_back(std::move(work), std::move(done));
    }
    x->wk->cv.notify_one();
}

static void worker_run(Worker* w) {
    for (;;) {
        std::pair<std::function<void()>, std::function<void()>> j;
        {
            std::unique_lock<std::mutex> lk(w->m);
            w->cv.wait(lk, [w] { return w->quit || !w->jobs.empty(); });
            if (w->quit) return;
            j = std::move(w->jobs.front());
            w->jobs.pop_front();
        }
        j.first();
        std::lock_guard<std::mutex> lk(w->m);
        w->done.push_back(std::move(j.second));
    }
}

// ---------------------------------------------------------------------------------------------------
static void lg_tick(t_lifegiver* x) {
    {
        std::deque<std::function<void()>> done;
        {
            std::lock_guard<std::mutex> lk(x->wk->m);
            done.swap(x->wk->done);
        }
        std::lock_guard<std::recursive_mutex> g(*x->mx);
        for (auto& d : done) d();          // worker results, on the main thread
        x->ctl->tick(now_ms());
    }
    clock_fdelay(x->tickClock, 15.);
}

static void lg_anything(t_lifegiver* x, t_symbol* s, long argc, t_atom* argv) {
    std::vector<Controller::Atom> a;
    a.reserve((size_t)argc);
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
    std::lock_guard<std::recursive_mutex> g(*x->mx);
    x->ctl->init(now_ms());
}

static void lg_int(t_lifegiver* x, long v) { (void)x; (void)v; }      // stray numbers: nothing to do
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
        const char* t[3] = {"(signal) Left out", "(signal) Right out", "Info for the patch and the view"};
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
    x->wk = new Worker();
    x->wk->th = std::thread(worker_run, x->wk);
    x->tickClock = clock_new(x, (method)lg_tick);
    clock_fdelay(x->tickClock, 50.);
    post("lifegiver: engine %s ready, device %s", VERSION, x->prefix->s_name);
    return x;
}

static void lg_free(t_lifegiver* x) {
    dsp_free((t_pxobject*)x);
    clock_unset(x->tickClock);
    object_free(x->tickClock);
    {
        std::lock_guard<std::mutex> lk(x->wk->m);
        x->wk->quit = true;
    }
    x->wk->cv.notify_one();
    if (x->wk->th.joinable()) x->wk->th.join();
    // results that never reached the main thread refer to the controller: dropped before it goes
    x->wk->jobs.clear();
    x->wk->done.clear();
    delete x->wk;
    {
        // plans still queued between the threads
        Report r;
        while (x->pr->reps.pop(r)) if (r.type == R_GARBAGE_PLAN) delete (Plan*)r.ptr;
        Cmd c;
        while (x->pr->cmds.pop(c)) if (c.type == C_PLAN) delete (Plan*)c.ptr;
        x->pr->freeKept();
    }
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
    class_addmethod(c, (method)lg_int, "int", A_LONG, 0);
    class_addmethod(c, (method)lg_float, "float", A_FLOAT, 0);
    class_addmethod(c, (method)lg_anything, "anything", A_GIMME, 0);
    class_dspinit(c);
    class_register(CLASS_BOX, c);
    s_class = c;
}
