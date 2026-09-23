#!/usr/bin/env python3
"""Generate the help patchers for this package. A .maxhelp is a JSON patcher; laying it out in
code keeps outlet indexes and box ids consistent. Re-run after interface changes:

    python3 source/make_help.py
"""
import json
import os

BOX_H = 22.0


class Patch:
    def __init__(self, width=760.0, height=640.0):
        self.boxes, self.lines, self.n = [], [], 0
        self.width, self.height = width, height

    def box(self, maxclass, x, y, w, text=None, nin=1, nout=1, outlettype=None, h=BOX_H, **extra):
        self.n += 1
        bid = f"obj-{self.n}"
        b = {"id": bid, "maxclass": maxclass, "numinlets": nin, "numoutlets": nout,
             "patching_rect": [float(x), float(y), float(w), float(h)]}
        if text is not None:
            b["text"] = text
        if outlettype is not None:
            b["outlettype"] = outlettype
        b.update(extra)
        self.boxes.append({"box": b})
        return bid

    def comment(self, x, y, w, text, h=20.0, **extra):
        return self.box("comment", x, y, w, text, nin=1, nout=0, h=h, **extra)

    def message(self, x, y, text, w=None):
        return self.box("message", x, y, w or max(40.0, 7.0 * len(text) + 14), text, nin=2, nout=1, outlettype=[""])

    def newobj(self, x, y, text, nin, nout, outlettype, w=None):
        return self.box("newobj", x, y, w or max(40.0, 7.0 * len(text) + 14), text, nin=nin, nout=nout, outlettype=outlettype)

    def flonum(self, x, y, w=60):
        return self.box("flonum", x, y, w, nin=1, nout=2, outlettype=["", "bang"])

    def connect(self, src, dst, so=0, di=0):
        self.lines.append({"patchline": {"source": [src, so], "destination": [dst, di]}})

    def json(self):
        return {"patcher": {
            "fileversion": 1,
            "appversion": {"major": 8, "minor": 5, "revision": 0, "architecture": "x64", "modernui": 1},
            "classnamespace": "box",
            "rect": [100.0, 100.0, self.width, self.height],
            "bglocked": 0, "openinpresentation": 0,
            "default_fontsize": 12.0, "default_fontface": 0, "default_fontname": "Arial",
            "gridonopen": 1, "gridsize": [15.0, 15.0], "gridsnaponopen": 1, "objectsnaponopen": 1,
            "statusbarvisible": 2, "toolbarvisible": 1,
            "boxes": self.boxes, "lines": self.lines,
            "dependency_cache": [], "autosave": 0,
        }}



def check(data):
    """Every connection must reference a real box and an in-range outlet/inlet."""
    ids = {b["box"]["id"]: b["box"] for b in data["patcher"]["boxes"]}
    for l in data["patcher"]["lines"]:
        (s, so), (d, di) = l["patchline"]["source"], l["patchline"]["destination"]
        assert s in ids and d in ids, (s, d)
        assert so < ids[s]["numoutlets"], f"{s} outlet {so}"
        assert di < ids[d]["numinlets"], f"{d} inlet {di}"


def euclid_patch():
    p = Patch(width=780.0, height=660.0)
    p.comment(20, 15, 720, "meml.euclid~ - Euclidean rhythm generator", h=33.0, fontsize=20.0)
    p.comment(20, 50, 740,
              "n steps per cycle with k pulses spread evenly over them, rotated by offset, each pulse "
              "lasting pw of one step. The generator from MODE_ELYSIAMORFS / MODE_MEMLCELIUM. Drive it "
              "with a phasor~ for tempo and sync; with the signal inlet unpatched it free-runs at @bpm.",
              h=60.0)

    # clock source
    p.comment(30, 120, 300, "one ramp per cycle - or leave unpatched for @bpm")
    rate = p.flonum(30, 145, 60)
    ph = p.newobj(30, 175, "phasor~ 0.5", 2, 1, ["signal"])
    p.connect(rate, ph)

    # parameters
    p.comment(260, 120, 300, "parameters (all are attributes)")
    params = [("n $1", "steps per cycle", 1, 32, 8),
              ("k $1", "pulses", 1, 32, 3),
              ("offset $1", "rotation in steps", 0, 31, 0)]
    y = 145
    for text, label, lo, hi, init in params:
        num = p.box("number", 260, y, 50, nin=1, nout=2, outlettype=["", "bang"], minimum=lo, maximum=hi)
        msg = p.message(320, y, text, 90)
        p.connect(num, msg)
        p.comment(415, y, 200, label)
        globals().setdefault("param_msgs", []).append(msg)
        y += 28
    pwnum = p.flonum(260, y, 50)
    pwmsg = p.message(320, y, "pw $1", 90)
    p.connect(pwnum, pwmsg)
    p.comment(415, y, 240, "pulse width, fraction of a step")
    y += 28
    normmsg = p.message(260, y, "norm 0.5 0.4 0.2", 150)
    p.comment(415, y, 300, "NN-style 0..1 vector: n, k, offset")
    y += 28
    bpmmsg = p.message(260, y, "bpm 96", 90)
    p.comment(415, y, 300, "internal tempo, when inlet 0 is unpatched")

    obj = p.newobj(30, 300, "meml.euclid~ 8 3", 1, 3, ["signal", "bang", "int"], w=140.0)
    p.connect(ph, obj)
    for m in globals().get("param_msgs", []) + [pwmsg, normmsg, bpmmsg]:
        p.connect(m, obj)
    globals()["param_msgs"] = []

    # outputs
    scope = p.box("scope~", 30, 345, 200, nin=2, nout=0, h=90.0)
    p.connect(obj, scope, 0, 0)
    p.comment(30, 440, 220, "outlet 0: gate signal")
    bang = p.box("button", 250, 345, 24, nin=1, nout=1, outlettype=["bang"], h=24.0)
    p.connect(obj, bang, 1, 0)
    p.comment(280, 347, 200, "outlet 1: bang per pulse")
    step = p.box("number", 250, 385, 50, nin=1, nout=2, outlettype=["", "bang"])
    p.connect(obj, step, 2, 0)
    p.comment(310, 387, 200, "outlet 2: step index")

    # hear it
    p.comment(30, 470, 400, "hear it: the gate plucks a decaying sine")
    click = p.newobj(30, 495, "*~ 1.", 2, 1, ["signal"])
    env = p.newobj(30, 525, "slide~ 1 800", 3, 1, ["signal"])
    osc = p.newobj(120, 525, "cycle~ 330", 2, 1, ["signal"])
    amp = p.newobj(30, 555, "*~", 2, 1, ["signal"])
    dac = p.box("ezdac~", 30, 590, 45, nin=2, nout=0, h=45.0)
    p.connect(obj, click, 0, 0)
    p.connect(click, env)
    p.connect(env, amp, 0, 0)
    p.connect(osc, amp, 0, 1)
    p.connect(amp, dac, 0, 0)
    p.connect(amp, dac, 0, 1)
    p.comment(90, 600, 300, "turn audio on")
    return p.json()


def ratioseq_patch():
    p = Patch(width=780.0, height=660.0)
    p.comment(20, 15, 720, "meml.ratioseq~ - ratio-based rhythm generator", h=33.0, fontsize=20.0)
    p.comment(20, 50, 740,
              "One sequence of the RatioSeqEngine behind MODE_MEMLCELIUM. A cycle is cut into slices "
              "proportional to @ratios and each slice fires for @pw of its own length, so 1 2 1 gives "
              "short, long, short. @ampratios is a second pattern choosing velocity 127 or 64.",
              h=60.0)

    p.comment(30, 120, 320, "bar phase - or leave unpatched for @bpm")
    rate = p.flonum(30, 145, 60)
    ph = p.newobj(30, 175, "phasor~ 0.5", 2, 1, ["signal"])
    p.connect(rate, ph)

    p.comment(260, 120, 320, "parameters (all are attributes)")
    msgs = []
    y = 145
    for text, label in [("ratios 1 2 1", "slice lengths"),
                        ("ratios 3 1 1 2", "any number of them"),
                        ("ampratios 1 3", "velocity pattern"),
                        ("mul 2.", "cycles per bar (1 2 4 8)"),
                        ("offset 0.25", "phase offset"),
                        ("pw 0.25", "pulse width per slice"),
                        ("norm 0.1 0.6 0.3 0.4 0.2 0.8 0.1", "NN vector: ratios, mul, offset, amp ratios"),
                        ("bpm 96", "internal tempo when unpatched")]:
        msgs.append(p.message(260, y, text, 170))
        p.comment(435, y, 320, label)
        y += 28

    obj = p.newobj(30, 385, "meml.ratioseq~ 1 2 1", 1, 3, ["signal", "signal", "int"], w=160.0)
    p.connect(ph, obj)
    for m in msgs:
        p.connect(m, obj)

    scope = p.box("scope~", 30, 425, 200, nin=2, nout=0, h=80.0)
    p.connect(obj, scope, 0, 0)
    p.comment(30, 510, 220, "outlet 0: trigger gate")
    scope2 = p.box("scope~", 250, 425, 200, nin=2, nout=0, h=80.0)
    p.connect(obj, scope2, 1, 0)
    p.comment(250, 510, 220, "outlet 1: amp gate")
    vel = p.box("number", 470, 425, 50, nin=1, nout=2, outlettype=["", "bang"])
    p.connect(obj, vel, 2, 0)
    p.comment(530, 427, 240, "outlet 2: velocity, 0 on release")

    p.comment(470, 455, 280, "-> makenote -> noteout for MIDI")
    mk = p.newobj(470, 480, "makenote 0 200", 3, 2, ["int", "int"])
    nt = p.newobj(470, 510, "noteout", 3, 0, [])
    p.connect(vel, mk)
    p.connect(mk, nt, 0, 0)
    p.connect(mk, nt, 1, 1)

    p.comment(30, 540, 400, "or hear the gate directly")
    click = p.newobj(30, 565, "*~ 1.", 2, 1, ["signal"])
    env = p.newobj(30, 595, "slide~ 1 1200", 3, 1, ["signal"])
    osc = p.newobj(130, 565, "cycle~ 220", 2, 1, ["signal"])
    amp = p.newobj(30, 625, "*~", 2, 1, ["signal"])
    dac = p.box("ezdac~", 250, 620, 45, nin=2, nout=0, h=45.0)
    p.connect(obj, click, 0, 0)
    p.connect(click, env)
    p.connect(env, amp, 0, 0)
    p.connect(osc, amp, 0, 1)
    p.connect(amp, dac, 0, 0)
    p.connect(amp, dac, 0, 1)
    p.comment(310, 632, 300, "turn audio on")
    return p.json()


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    for name, builder in (("meml.euclid~", euclid_patch), ("meml.ratioseq~", ratioseq_patch)):
        data = builder()
        check(data)
        out = os.path.join(here, "..", "help", name + ".maxhelp")
        with open(out, "w") as f:
            json.dump(data, f, indent=1)
            f.write("\n")
        print(f"wrote {os.path.normpath(out)}: {len(data['patcher']['boxes'])} boxes, {len(data['patcher']['lines'])} connections")
