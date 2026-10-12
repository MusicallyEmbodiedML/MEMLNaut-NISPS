#!/usr/bin/env python3
"""Generate help/nisps.maxhelp. Re-run after any interface change:

    python3 source/make_help.py

A .maxhelp is a JSON patcher, and Max's tabbed help layout is a convention
inside it rather than a format of its own: the root patcher holds a
`js helpstarter <classname>` object plus one `p <name>` subpatcher per tab,
each carrying the varname `<name>_tab` and `showontab: 1`, with the root
setting `showontab: 0` / `showrootpatcherontab: 0` so only the tabs show.
That is how the help files shipped with the Max SDK are built (61 of the 110
there use it), which is why the tabs below are laid out that way rather than
with a `tab` object and patcher scripting.
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

    def patcher(self, rect=None, showontab=0):
        """The bare patcher dict: what goes inside a subpatcher box."""
        return {
            "fileversion": 1,
            "appversion": {"major": 8, "minor": 5, "revision": 0, "architecture": "x64", "modernui": 1},
            "classnamespace": "box",
            "rect": rect or [100.0, 100.0, self.width, self.height],
            "bglocked": 0, "openinpresentation": 0,
            "default_fontsize": 12.0, "default_fontface": 0, "default_fontname": "Arial",
            "gridonopen": 1, "gridsize": [15.0, 15.0], "gridsnaponopen": 1, "objectsnaponopen": 1,
            "statusbarvisible": 2, "toolbarvisible": 1,
            "showontab": showontab,
            "boxes": self.boxes, "lines": self.lines,
            "dependency_cache": [], "autosave": 0,
        }

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


def basic_tab():
    p = Patch()
    p.comment(20, 15, 700, "nisps - Neural Interactive Shaping of Parameter Spaces", h=33.0, fontsize=20.0)
    p.comment(20, 50, 720,
              "Maps an input vector to an output vector through a small neural network that you shape by "
              "feedback: send 'like' when you hear something good and 'dislike' when you don't, and the "
              "mapping around the current input reorganises itself. Arguments: number of inputs, number of "
              "outputs. Ticks internally at @interval ms (default 5); outputs the action list whenever it changes. "
              "See the storage tab for saving and loading, and the slots tab for jumping between mappings.",
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

    # ---- the object
    obj = p.newobj(30, 395, "nisps 2 4", 1, 4, ["", "int", "", "float"], w=110.0)
    p.connect(norm, obj)
    for m in gesture_ids + attr_ids:
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
    p.comment(520, 357, 150, "outlet 2: slot replies")
    err = p.flonum(680, 355, 60)
    p.connect(obj, err, 3, 0)
    p.comment(680, 377, 170, "outlet 3: training error")

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
    return p


def storage_tab():
    """Saving and loading: write / read, @file and @autoload."""
    p = Patch()
    p.comment(20, 15, 800, "Saving and loading", h=33.0, fontsize=20.0)
    p.comment(20, 50, 800,
              "write captures everything the object has learned - the network's weights and biases, the "
              "replay memory with each experience's age, the live position and its mapped action, and the "
              "engine parameters - as a Max dictionary written to JSON, the same file idea as pattrstorage. "
              "read loads one back. A file that does not match this object's shape is refused outright, with "
              "the reason in the Max window, and the running engine is left alone.",
              h=75.0)

    p.comment(30, 140, 300, "with a dialog")
    dlg = [p.message(30, 165, "write", 60), p.message(100, 165, "read", 60)]

    p.comment(30, 205, 340, "straight to a named file")
    named = [p.message(30, 230, "write mymap.json", 140),
             p.message(30, 258, "read mymap.json", 140)]
    p.comment(180, 232, 300, "written beside the patcher")
    p.comment(180, 260, 300, "found along Max's search path")

    p.comment(30, 300, 400, "or name the file once, as an attribute:")
    attr = [p.message(30, 325, "file mymap.json", 130),
            p.message(170, 325, "autoload 1", 100),
            p.message(280, 325, "autoload 0", 100)]
    p.comment(30, 353, 780,
              "With @file set, a bare write or read uses it instead of opening a dialog, so a patch can save "
              "with one message box. @autoload 1 (the default) reads @file when the patcher finishes loading. "
              "An object created by scripting never gets a loadbang, so such a patch should send read itself.",
              h=45.0)

    obj = p.newobj(30, 420, "nisps 2 4 @file mymap.json @autoload 0", 1, 4,
                   ["", "int", "", "float"], w=250.0)
    for m in dlg + named + attr:
        p.connect(m, obj)

    mem = p.box("number", 300, 460, 60, nin=1, nout=2, outlettype=["", "bang"])
    p.connect(obj, mem, 1, 0)
    p.comment(365, 462, 300, "memory size, reported after a read")
    pr = p.newobj(30, 460, "print nisps", 1, 0, [])
    p.connect(obj, pr, 2, 0)
    p.comment(120, 462, 200, "slot replies")

    p.comment(30, 505, 800, "The file is plain JSON, so it opens in [dict] or any text editor:", h=20.0)
    p.comment(30, 528, 800,
              '{ \"nisps\" : { \"version\" : 1, \"inputs\" : 2, \"outputs\" : 4, '
              '\"layer_shape\" : [2, 16, ...], \"weights\" : [...], \"biases\" : [...], '
              '\"memory_count\" : 12, ... } }',
              h=40.0)
    p.comment(30, 572, 800,
              "These are the object's own files; they are not interchangeable with models saved on MEMLNaut "
              "hardware, whose binary format stores raw size_t values of a different width.",
              h=32.0)
    return p


def slots_tab():
    """Numbered snapshots, the pattrstorage idea."""
    p = Patch()
    p.comment(20, 15, 800, "Slots", h=33.0, fontsize=20.0)
    p.comment(20, 50, 800,
              "store 1, store 2, ... keep numbered snapshots in memory, and recall <n> makes one live again. "
              "A slot holds everything a file holds - the network, the replay memory and the engine "
              "parameters - so recalling is a complete change of mapping rather than a parameter set: a set "
              "of slots is a set of instruments to move between mid-performance. Slots are written into the "
              "same file as the live state, so write and read carry them along.",
              h=75.0)

    p.comment(30, 140, 300, "capture what is playing now")
    stores = [p.message(30, 165, "store 1", 70), p.message(110, 165, "store 2", 70),
              p.message(190, 165, "store 3", 70)]

    p.comment(30, 205, 300, "jump between them")
    recalls = [p.message(30, 230, "recall 1", 75), p.message(115, 230, "recall 2", 75),
               p.message(200, 230, "recall 3", 75)]

    p.comment(30, 270, 400, "housekeeping")
    keep = [p.message(30, 295, "erase 2", 70), p.message(110, 295, "slots", 55)]
    p.comment(175, 297, 400, "erase empties a slot; slots reports what is occupied")

    p.comment(30, 335, 800,
              "Recalling an empty slot warns and changes nothing. A slot whose shape does not fit this "
              "object is refused the same way a file is. If any slot in a file fails to load the whole read "
              "is refused, naming the slot - a file that silently lost half its presets would be worse.",
              h=45.0)

    obj = p.newobj(30, 400, "nisps 2 4", 1, 4, ["", "int", "", "float"], w=110.0)
    for m in stores + recalls + keep:
        p.connect(m, obj)

    outs = p.box("multislider", 30, 445, 200, nin=1, nout=2, outlettype=["", ""], h=90.0,
                 size=4, setminmax=[0.0, 1.0], orientation=1, slidercolor=[0.2, 0.5, 0.9, 1.0])
    p.connect(obj, outs, 0, 0)
    p.comment(30, 540, 220, "the mapping, before and after a recall")

    route = p.newobj(260, 445, "route slots slot", 1, 3, ["", "", ""])
    p.connect(obj, route, 2, 0)
    occupied = p.newobj(260, 478, "print occupied", 1, 0, [])
    current = p.box("number", 400, 478, 60, nin=1, nout=2, outlettype=["", "bang"])
    p.connect(route, occupied, 0, 0)
    p.connect(route, current, 1, 0)
    p.comment(260, 505, 200, "reply to slots")
    p.comment(400, 505, 240, "slot <n> after store / recall")
    return p


TAB_RECT = [100.0, 100.0, 860.0, 620.0]


def tabbed_help(classname, tabs, digest="", description=""):
    """Assemble a Max-style tabbed help patcher.

    `tabs` is a list of (name, Patch). The name becomes the tab's label, so it
    is kept to a single word - it also forms the varname Max looks for.
    A final empty "?" tab is added, as every stock help file has one.
    """
    boxes = []
    n = 0

    def add(box):
        nonlocal n
        n += 1
        box["id"] = f"root-{n}"
        boxes.append({"box": box})

    # helpstarter draws the tab bar and opens the first tab
    add({"maxclass": "newobj", "text": f"js helpstarter.js {classname}",
         "numinlets": 1, "numoutlets": 1, "outlettype": [""],
         "fontname": "Arial", "fontsize": 13.0,
         "style": "",
         "patching_rect": [445.0, 350.0, 190.0, 23.0],
         "saved_object_attributes": {"filename": "helpstarter", "parameter_enable": 0}})

    x = 10.0
    for name, patch in list(tabs) + [("?", Patch())]:
        add({"maxclass": "newobj", "text": f"p {name}",
             "numinlets": 0, "numoutlets": 0,
             "fontname": "Arial", "fontsize": 13.0,
             "patching_rect": [x, 85.0, 60.0, 23.0],
             "varname": f"{name if name != '?' else 'q'}_tab",
             "style": "",
             "saved_object_attributes": {"description": "", "digest": "", "fontsize": 13.0,
                                         "globalpatchername": "", "tags": ""},
             "patcher": patch.patcher(rect=TAB_RECT, showontab=1)})
        x += 70.0

    return {"patcher": {
        "fileversion": 1,
        "appversion": {"major": 8, "minor": 5, "revision": 0, "architecture": "x64", "modernui": 1},
        "classnamespace": "box",
        "rect": TAB_RECT,
        "bglocked": 0, "openinpresentation": 0,
        "default_fontsize": 12.0, "default_fontface": 0, "default_fontname": "Arial",
        "gridonopen": 1, "gridsize": [15.0, 15.0], "gridsnaponopen": 1, "objectsnaponopen": 1,
        "statusbarvisible": 2, "toolbarvisible": 1,
        "showontab": 0, "showrootpatcherontab": 0,
        "digest": digest, "description": description, "tags": "machine learning, mapping",
        "boxes": boxes, "lines": [],
        "dependency_cache": [], "autosave": 0,
    }}


def check_root(data):
    """Structural check across the root patcher and every tab."""
    def check_patcher(p, where):
        ids = {b["box"]["id"]: b["box"] for b in p.get("boxes", [])}
        for l in p.get("lines", []):
            (src, so), (dst, di) = l["patchline"]["source"], l["patchline"]["destination"]
            assert src in ids and dst in ids, f"{where}: dangling patchline {src}->{dst}"
            assert so < ids[src]["numoutlets"], f"{where}: {src} has no outlet {so}"
            assert di < ids[dst]["numinlets"], f"{where}: {dst} has no inlet {di}"
        for b in p.get("boxes", []):
            if "patcher" in b["box"]:
                check_patcher(b["box"]["patcher"], where + "/" + b["box"].get("text", "?"))

    root = data["patcher"]
    check_patcher(root, "root")
    tabs = [b["box"] for b in root["boxes"] if str(b["box"].get("varname", "")).endswith("_tab")]
    assert tabs, "no tabs found"
    for t in tabs:
        assert t["patcher"]["showontab"] == 1, f"{t['text']} is not marked showontab"
    assert root["showontab"] == 0 and root["showrootpatcherontab"] == 0
    return [t["text"] for t in tabs]


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "..", "help", "nisps.maxhelp")
    data = tabbed_help(
        "nisps",
        [("basic", basic_tab()), ("storage", storage_tab()), ("slots", slots_tab())],
        digest="Neural Interactive Shaping of Parameter Spaces",
        description="Maps an input vector to an output vector through a small neural network "
                    "shaped by like/dislike feedback.")
    names = check_root(data)
    with open(out, "w") as f:
        json.dump(data, f, indent=1)
        f.write("\n")
    total = sum(len(b["box"].get("patcher", {}).get("boxes", [])) for b in data["patcher"]["boxes"])
    print(f"wrote {os.path.normpath(out)}: tabs {names}, {total} boxes across them")
