{
 "patcher": {
  "fileversion": 1,
  "appversion": {
   "major": 8,
   "minor": 5,
   "revision": 0,
   "architecture": "x64",
   "modernui": 1
  },
  "classnamespace": "box",
  "rect": [
   100.0,
   100.0,
   780.0,
   660.0
  ],
  "bglocked": 0,
  "openinpresentation": 0,
  "default_fontsize": 12.0,
  "default_fontface": 0,
  "default_fontname": "Arial",
  "gridonopen": 1,
  "gridsize": [
   15.0,
   15.0
  ],
  "gridsnaponopen": 1,
  "objectsnaponopen": 1,
  "statusbarvisible": 2,
  "toolbarvisible": 1,
  "boxes": [
   {
    "box": {
     "id": "obj-1",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20.0,
      15.0,
      720.0,
      33.0
     ],
     "text": "meml.ratioseq~ - ratio-based rhythm generator",
     "fontsize": 20.0
    }
   },
   {
    "box": {
     "id": "obj-2",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20.0,
      50.0,
      740.0,
      60.0
     ],
     "text": "One sequence of the RatioSeqEngine behind MODE_MEMLCELIUM. A cycle is cut into slices proportional to @ratios and each slice fires for @pw of its own length, so 1 2 1 gives short, long, short. @ampratios is a second pattern choosing velocity 127 or 64."
    }
   },
   {
    "box": {
     "id": "obj-3",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      30.0,
      120.0,
      320.0,
      20.0
     ],
     "text": "bar phase - or leave unpatched for @bpm"
    }
   },
   {
    "box": {
     "id": "obj-4",
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      30.0,
      145.0,
      60.0,
      22.0
     ],
     "outlettype": [
      "",
      "bang"
     ]
    }
   },
   {
    "box": {
     "id": "obj-5",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      30.0,
      175.0,
      91.0,
      22.0
     ],
     "text": "phasor~ 0.5",
     "outlettype": [
      "signal"
     ]
    }
   },
   {
    "box": {
     "id": "obj-6",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      260.0,
      120.0,
      320.0,
      20.0
     ],
     "text": "parameters (all are attributes)"
    }
   },
   {
    "box": {
     "id": "obj-7",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      260.0,
      145.0,
      170.0,
      22.0
     ],
     "text": "ratios 1 2 1",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-8",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      435.0,
      145.0,
      320.0,
      20.0
     ],
     "text": "slice lengths"
    }
   },
   {
    "box": {
     "id": "obj-9",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      260.0,
      173.0,
      170.0,
      22.0
     ],
     "text": "ratios 3 1 1 2",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-10",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      435.0,
      173.0,
      320.0,
      20.0
     ],
     "text": "any number of them"
    }
   },
   {
    "box": {
     "id": "obj-11",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      260.0,
      201.0,
      170.0,
      22.0
     ],
     "text": "ampratios 1 3",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-12",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      435.0,
      201.0,
      320.0,
      20.0
     ],
     "text": "velocity pattern"
    }
   },
   {
    "box": {
     "id": "obj-13",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      260.0,
      229.0,
      170.0,
      22.0
     ],
     "text": "mul 2.",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-14",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      435.0,
      229.0,
      320.0,
      20.0
     ],
     "text": "cycles per bar (1 2 4 8)"
    }
   },
   {
    "box": {
     "id": "obj-15",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      260.0,
      257.0,
      170.0,
      22.0
     ],
     "text": "offset 0.25",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-16",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      435.0,
      257.0,
      320.0,
      20.0
     ],
     "text": "phase offset"
    }
   },
   {
    "box": {
     "id": "obj-17",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      260.0,
      285.0,
      170.0,
      22.0
     ],
     "text": "pw 0.25",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-18",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      435.0,
      285.0,
      320.0,
      20.0
     ],
     "text": "pulse width per slice"
    }
   },
   {
    "box": {
     "id": "obj-19",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      260.0,
      313.0,
      170.0,
      22.0
     ],
     "text": "norm 0.1 0.6 0.3 0.4 0.2 0.8 0.1",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-20",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      435.0,
      313.0,
      320.0,
      20.0
     ],
     "text": "NN vector: ratios, mul, offset, amp ratios"
    }
   },
   {
    "box": {
     "id": "obj-21",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      260.0,
      341.0,
      170.0,
      22.0
     ],
     "text": "bpm 96",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-22",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      435.0,
      341.0,
      320.0,
      20.0
     ],
     "text": "internal tempo when unpatched"
    }
   },
   {
    "box": {
     "id": "obj-23",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 3,
     "patching_rect": [
      30.0,
      385.0,
      160.0,
      22.0
     ],
     "text": "meml.ratioseq~ 1 2 1",
     "outlettype": [
      "signal",
      "signal",
      "int"
     ]
    }
   },
   {
    "box": {
     "id": "obj-24",
     "maxclass": "scope~",
     "numinlets": 2,
     "numoutlets": 0,
     "patching_rect": [
      30.0,
      425.0,
      200.0,
      80.0
     ]
    }
   },
   {
    "box": {
     "id": "obj-25",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      30.0,
      510.0,
      220.0,
      20.0
     ],
     "text": "outlet 0: trigger gate"
    }
   },
   {
    "box": {
     "id": "obj-26",
     "maxclass": "scope~",
     "numinlets": 2,
     "numoutlets": 0,
     "patching_rect": [
      250.0,
      425.0,
      200.0,
      80.0
     ]
    }
   },
   {
    "box": {
     "id": "obj-27",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      250.0,
      510.0,
      220.0,
      20.0
     ],
     "text": "outlet 1: amp gate"
    }
   },
   {
    "box": {
     "id": "obj-28",
     "maxclass": "number",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      470.0,
      425.0,
      50.0,
      22.0
     ],
     "outlettype": [
      "",
      "bang"
     ]
    }
   },
   {
    "box": {
     "id": "obj-29",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      530.0,
      427.0,
      240.0,
      20.0
     ],
     "text": "outlet 2: velocity, 0 on release"
    }
   },
   {
    "box": {
     "id": "obj-30",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      470.0,
      455.0,
      280.0,
      20.0
     ],
     "text": "-> makenote -> noteout for MIDI"
    }
   },
   {
    "box": {
     "id": "obj-31",
     "maxclass": "newobj",
     "numinlets": 3,
     "numoutlets": 2,
     "patching_rect": [
      470.0,
      480.0,
      112.0,
      22.0
     ],
     "text": "makenote 0 200",
     "outlettype": [
      "int",
      "int"
     ]
    }
   },
   {
    "box": {
     "id": "obj-32",
     "maxclass": "newobj",
     "numinlets": 3,
     "numoutlets": 0,
     "patching_rect": [
      470.0,
      510.0,
      63.0,
      22.0
     ],
     "text": "noteout",
     "outlettype": []
    }
   },
   {
    "box": {
     "id": "obj-33",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      30.0,
      540.0,
      400.0,
      20.0
     ],
     "text": "or hear the gate directly"
    }
   },
   {
    "box": {
     "id": "obj-34",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      30.0,
      565.0,
      49.0,
      22.0
     ],
     "text": "*~ 1.",
     "outlettype": [
      "signal"
     ]
    }
   },
   {
    "box": {
     "id": "obj-35",
     "maxclass": "newobj",
     "numinlets": 3,
     "numoutlets": 1,
     "patching_rect": [
      30.0,
      595.0,
      105.0,
      22.0
     ],
     "text": "slide~ 1 1200",
     "outlettype": [
      "signal"
     ]
    }
   },
   {
    "box": {
     "id": "obj-36",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      130.0,
      565.0,
      84.0,
      22.0
     ],
     "text": "cycle~ 220",
     "outlettype": [
      "signal"
     ]
    }
   },
   {
    "box": {
     "id": "obj-37",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      30.0,
      625.0,
      40.0,
      22.0
     ],
     "text": "*~",
     "outlettype": [
      "signal"
     ]
    }
   },
   {
    "box": {
     "id": "obj-38",
     "maxclass": "ezdac~",
     "numinlets": 2,
     "numoutlets": 0,
     "patching_rect": [
      250.0,
      620.0,
      45.0,
      45.0
     ]
    }
   },
   {
    "box": {
     "id": "obj-39",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      310.0,
      632.0,
      300.0,
      20.0
     ],
     "text": "turn audio on"
    }
   }
  ],
  "lines": [
   {
    "patchline": {
     "source": [
      "obj-4",
      0
     ],
     "destination": [
      "obj-5",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-5",
      0
     ],
     "destination": [
      "obj-23",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-7",
      0
     ],
     "destination": [
      "obj-23",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-9",
      0
     ],
     "destination": [
      "obj-23",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-11",
      0
     ],
     "destination": [
      "obj-23",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-13",
      0
     ],
     "destination": [
      "obj-23",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-15",
      0
     ],
     "destination": [
      "obj-23",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-17",
      0
     ],
     "destination": [
      "obj-23",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-19",
      0
     ],
     "destination": [
      "obj-23",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-21",
      0
     ],
     "destination": [
      "obj-23",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-23",
      0
     ],
     "destination": [
      "obj-24",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-23",
      1
     ],
     "destination": [
      "obj-26",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-23",
      2
     ],
     "destination": [
      "obj-28",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-28",
      0
     ],
     "destination": [
      "obj-31",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-31",
      0
     ],
     "destination": [
      "obj-32",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-31",
      1
     ],
     "destination": [
      "obj-32",
      1
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-23",
      0
     ],
     "destination": [
      "obj-34",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-34",
      0
     ],
     "destination": [
      "obj-35",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-35",
      0
     ],
     "destination": [
      "obj-37",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-36",
      0
     ],
     "destination": [
      "obj-37",
      1
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-37",
      0
     ],
     "destination": [
      "obj-38",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-37",
      0
     ],
     "destination": [
      "obj-38",
      1
     ]
    }
   }
  ],
  "dependency_cache": [],
  "autosave": 0
 }
}
