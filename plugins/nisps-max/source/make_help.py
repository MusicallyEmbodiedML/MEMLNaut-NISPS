#!/usr/bin/env python3
"""Generate help/nisps.maxhelp. A .maxhelp is a JSON patcher; laying it out in
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


def build():
    p = Patch()
    p.comment(20, 15, 700, "nisps - Neural Interactive Shaping of Parameter Spaces", h=33.0, fontsize=20.0)
    p.comment(20, 50, 720,
              "Maps an input vector to an output vector through a small neural network that you shape by "
              "feedback: send 'like' when you hear something good and 'dislike' when you don't, and the "
              "mapping around the current input reorganises itself. Arguments: number of inputs, number of "
              "outputs. Ticks internally at @interval ms (default 5); outputs the action list whenever it changes.",
              h=75.0)

    # ---- input: a 2-slider multislider, already 0..1, outputs a list
    p.comment(30, 135, 220, "drag: the 2 inputs (0..1)")
    src = p.box("multislider", 30, 160, 120, nin=1, nout=2, outlettype=["", ""], h=120.0,
                size=2, setminmax=[0.0, 1.0], candicane=1, orientation=1,
                slidercolor=[0.5, 0.5, 0.5, 1.0])
    norm = src

    # ---- gestures
    p.comment(190, 135, 200, "gestures")
    gestures = ["like", "dislike", "randomise", "clear", "jolt 1", "jolt 0"]
    gesture_ids = [p.message(190, 160 + 28 * i, t, 80) for i, t in enumerate(gestures)]
    p.comment(275, 160, 150, "store liked pair", h=20.0)
    p.comment(275, 188, 150, "store disliked pair")
    p.comment(275, 216, 150, "scramble the network")
    p.comment(275, 244, 150, "forget everything")
    p.comment(275, 272, 170, "hold: morph weights (learning paused)")
    p.comment(275, 300, 150, "release the jolt")

    # ---- attributes
    p.comment(450, 135, 250, "attributes")
    attrs = [("noise 0.2", "exploration noise 0..1 (0 = off)"),
             ("lrscale 1.", "learning rate multiplier"),
             ("rewardscale 1.", "like/dislike strength"),
             ("traindivisor 4", "train every Nth tick"),
             ("storemode 2", "0 add 1-3 replace5/10/15 4-5 decay10/20"),
             ("interval 5.", "tick period in ms"),
             ("active 0", "stop the clock; bang = one tick")]
    attr_ids = []
    for i, (t, c) in enumerate(attrs):
        attr_ids.append(p.message(450, 160 + 28 * i, t, 110))
        p.comment(565, 160 + 28 * i, 190, c)
    bang = p.box("button", 450, 160 + 28 * len(attrs), 24, nin=1, nout=1, outlettype=["bang"], h=24.0)
    p.comment(480, 160 + 28 * len(attrs) + 2, 250, "bang: tick once (use with active 0)")

    # save / load
    y_file = 160 + 28 * (len(attrs) + 1)
    p.comment(450, y_file, 300, "save / load what it has learned")
    file_msgs = [p.message(450, y_file + 25, "write", 60),
                 p.message(515, y_file + 25, "read", 60),
                 p.message(450, y_file + 53, "write mymap.json", 130),
                 p.message(450, y_file + 81, "read mymap.json", 130)]
    p.comment(580, y_file + 27, 180, "dialog")
    p.comment(585, y_file + 55, 180, "straight to a file")

    # slots: numbered snapshots, pattrstorage style
    y_slot = y_file + 115
    p.comment(450, y_slot, 320, "slots: whole mappings you can jump between")
    slot_msgs = [p.message(450, y_slot + 25, "store 1", 70),
                 p.message(525, y_slot + 25, "recall 1", 75),
                 p.message(605, y_slot + 25, "store 2", 70),
                 p.message(450, y_slot + 53, "recall 2", 75),
                 p.message(530, y_slot + 53, "erase 2", 70),
                 p.message(605, y_slot + 53, "slots", 55)]
    file_msgs.extend(slot_msgs)
    p.comment(450, y_slot + 81, 320, "right outlet answers: slots 1 2 / slot 1")

    # ---- the object
    obj = p.newobj(30, 395, "nisps 2 4 @file mymap.json @autoload 0", 1, 3, ["", "int", ""], w=250.0)
    p.connect(norm, obj)
    for m in gesture_ids + attr_ids + file_msgs:
        p.connect(m, obj)
    p.connect(bang, obj)

    # ---- outputs
    ms = p.box("multislider", 30, 440, 200, nin=1, nout=2, outlettype=["", ""], h=100.0,
               size=4, setminmax=[0.0, 1.0], orientation=1, slidercolor=[0.2, 0.5, 0.9, 1.0])
    p.connect(obj, ms, 0, 0)
    p.comment(30, 545, 200, "outlet 0: the 4 outputs, 0..1")
    unpack = p.newobj(250, 440, "unpack 0. 0. 0. 0.", 1, 4, ["float", "float", "float", "float"])
    p.connect(obj, unpack, 0, 0)
    nums = [p.flonum(250 + 65 * i, 470, 60) for i in range(4)]
    for i, n in enumerate(nums):
        p.connect(unpack, n, i, 0)
    mem = p.box("number", 300, 355, 60, nin=1, nout=2, outlettype=["", "bang"])
    p.connect(obj, mem, 1, 0)
    p.comment(365, 357, 160, "outlet 1: memory size")
    info = p.newobj(430, 355, "print nisps", 1, 0, [])
    p.connect(obj, info, 2, 0)
    p.comment(520, 357, 200, "outlet 2: slot replies")

    # ---- hear it: output 0 -> pitch, output 1 -> level
    p.comment(250, 505, 300, "hear it: output 0 -> pitch, output 1 -> level")
    freq = p.newobj(250, 530, "scale 0. 1. 110. 880. 4.", 6, 1, [""])
    osc = p.newobj(250, 560, "cycle~", 2, 1, ["signal"])
    lvl = p.newobj(340, 530, "scale 0. 1. 0. 0.2", 6, 1, [""])
    gain = p.newobj(250, 590, "*~ 0.", 2, 1, ["signal"])
    dac = p.box("ezdac~", 340, 585, 45, nin=2, nout=0, h=45.0)
    p.connect(nums[0], freq)
    p.connect(freq, osc)
    p.connect(nums[1], lvl)
    p.connect(lvl, gain, 0, 1)
    p.connect(osc, gain)
    p.connect(gain, dac, 0, 0)
    p.connect(gain, dac, 0, 1)
    p.comment(400, 600, 300, "turn audio on, then like/dislike as you move")
    return p.json()


def check(data):
    ids = {b["box"]["id"]: b["box"] for b in data["patcher"]["boxes"]}
    for l in data["patcher"]["lines"]:
        (s, so), (d, di) = l["patchline"]["source"], l["patchline"]["destination"]
        assert s in ids and d in ids, (s, d)
        assert so < ids[s]["numoutlets"], f"{s} outlet {so}"
        assert di < ids[d]["numinlets"], f"{d} inlet {di}"


if __name__ == "__main__":
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "help", "nisps.maxhelp")
    data = build()
    check(data)
    with open(out, "w") as f:
        json.dump(data, f, indent=1)
        f.write("\n")
    print(f"wrote {os.path.normpath(out)}: {len(data['patcher']['boxes'])} boxes, {len(data['patcher']['lines'])} connections")
