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
   760.0,
   640.0
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
      700.0,
      33.0
     ],
     "text": "nisps - Neural Interactive Shaping of Parameter Spaces",
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
      720.0,
      75.0
     ],
     "text": "Maps an input vector to an output vector through a small neural network that you shape by feedback: send 'like' when you hear something good and 'dislike' when you don't, and the mapping around the current input reorganises itself. Arguments: number of inputs, number of outputs. Ticks internally at @interval ms (default 5); outputs the action list whenever it changes."
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
      135.0,
      220.0,
      20.0
     ],
     "text": "drag: the 2 inputs (0..1)"
    }
   },
   {
    "box": {
     "id": "obj-4",
     "maxclass": "multislider",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      30.0,
      160.0,
      120.0,
      120.0
     ],
     "outlettype": [
      "",
      ""
     ],
     "size": 2,
     "setminmax": [
      0.0,
      1.0
     ],
     "candicane": 1,
     "orientation": 1,
     "slidercolor": [
      0.5,
      0.5,
      0.5,
      1.0
     ]
    }
   },
   {
    "box": {
     "id": "obj-5",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      190.0,
      135.0,
      200.0,
      20.0
     ],
     "text": "gestures"
    }
   },
   {
    "box": {
     "id": "obj-6",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      190.0,
      160.0,
      80.0,
      22.0
     ],
     "text": "like",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-7",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      190.0,
      188.0,
      80.0,
      22.0
     ],
     "text": "dislike",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-8",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      190.0,
      216.0,
      80.0,
      22.0
     ],
     "text": "randomise",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-9",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      190.0,
      244.0,
      80.0,
      22.0
     ],
     "text": "clear",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-10",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      190.0,
      272.0,
      80.0,
      22.0
     ],
     "text": "jolt 1",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-11",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      190.0,
      300.0,
      80.0,
      22.0
     ],
     "text": "jolt 0",
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
      275.0,
      160.0,
      150.0,
      20.0
     ],
     "text": "store liked pair"
    }
   },
   {
    "box": {
     "id": "obj-13",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      275.0,
      188.0,
      150.0,
      20.0
     ],
     "text": "store disliked pair"
    }
   },
   {
    "box": {
     "id": "obj-14",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      275.0,
      216.0,
      150.0,
      20.0
     ],
     "text": "scramble the network"
    }
   },
   {
    "box": {
     "id": "obj-15",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      275.0,
      244.0,
      150.0,
      20.0
     ],
     "text": "forget everything"
    }
   },
   {
    "box": {
     "id": "obj-16",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      275.0,
      272.0,
      170.0,
      20.0
     ],
     "text": "hold: morph weights (learning paused)"
    }
   },
   {
    "box": {
     "id": "obj-17",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      275.0,
      300.0,
      150.0,
      20.0
     ],
     "text": "release the jolt"
    }
   },
   {
    "box": {
     "id": "obj-18",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      450.0,
      135.0,
      250.0,
      20.0
     ],
     "text": "attributes"
    }
   },
   {
    "box": {
     "id": "obj-19",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      450.0,
      160.0,
      110.0,
      22.0
     ],
     "text": "noise 0.2",
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
      565.0,
      160.0,
      190.0,
      20.0
     ],
     "text": "exploration noise 0..1 (0 = off)"
    }
   },
   {
    "box": {
     "id": "obj-21",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      450.0,
      188.0,
      110.0,
      22.0
     ],
     "text": "lrscale 1.",
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
      565.0,
      188.0,
      190.0,
      20.0
     ],
     "text": "learning rate multiplier"
    }
   },
   {
    "box": {
     "id": "obj-23",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      450.0,
      216.0,
      110.0,
      22.0
     ],
     "text": "rewardscale 1.",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-24",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      565.0,
      216.0,
      190.0,
      20.0
     ],
     "text": "like/dislike strength"
    }
   },
   {
    "box": {
     "id": "obj-25",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      450.0,
      244.0,
      110.0,
      22.0
     ],
     "text": "traindivisor 4",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-26",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      565.0,
      244.0,
      190.0,
      20.0
     ],
     "text": "train every Nth tick"
    }
   },
   {
    "box": {
     "id": "obj-27",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      450.0,
      272.0,
      110.0,
      22.0
     ],
     "text": "storemode 2",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-28",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      565.0,
      272.0,
      190.0,
      20.0
     ],
     "text": "0 add 1-3 replace5/10/15 4-5 decay10/20"
    }
   },
   {
    "box": {
     "id": "obj-29",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      450.0,
      300.0,
      110.0,
      22.0
     ],
     "text": "interval 5.",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-30",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      565.0,
      300.0,
      190.0,
      20.0
     ],
     "text": "tick period in ms"
    }
   },
   {
    "box": {
     "id": "obj-31",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      450.0,
      328.0,
      110.0,
      22.0
     ],
     "text": "active 0",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-32",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      565.0,
      328.0,
      190.0,
      20.0
     ],
     "text": "stop the clock; bang = one tick"
    }
   },
   {
    "box": {
     "id": "obj-33",
     "maxclass": "button",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      450.0,
      356.0,
      24.0,
      24.0
     ],
     "outlettype": [
      "bang"
     ]
    }
   },
   {
    "box": {
     "id": "obj-34",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      480.0,
      358.0,
      250.0,
      20.0
     ],
     "text": "bang: tick once (use with active 0)"
    }
   },
   {
    "box": {
     "id": "obj-35",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      30.0,
      395.0,
      100.0,
      22.0
     ],
     "text": "nisps 2 4",
     "outlettype": [
      "",
      "int"
     ]
    }
   },
   {
    "box": {
     "id": "obj-36",
     "maxclass": "multislider",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      30.0,
      440.0,
      200.0,
      100.0
     ],
     "outlettype": [
      "",
      ""
     ],
     "size": 4,
     "setminmax": [
      0.0,
      1.0
     ],
     "orientation": 1,
     "slidercolor": [
      0.2,
      0.5,
      0.9,
      1.0
     ]
    }
   },
   {
    "box": {
     "id": "obj-37",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      30.0,
      545.0,
      200.0,
      20.0
     ],
     "text": "outlet 0: the 4 outputs, 0..1"
    }
   },
   {
    "box": {
     "id": "obj-38",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 4,
     "patching_rect": [
      250.0,
      440.0,
      140.0,
      22.0
     ],
     "text": "unpack 0. 0. 0. 0.",
     "outlettype": [
      "float",
      "float",
      "float",
      "float"
     ]
    }
   },
   {
    "box": {
     "id": "obj-39",
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      250.0,
      470.0,
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
     "id": "obj-40",
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      315.0,
      470.0,
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
     "id": "obj-41",
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      380.0,
      470.0,
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
     "id": "obj-42",
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      445.0,
      470.0,
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
     "id": "obj-43",
     "maxclass": "number",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      550.0,
      395.0,
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
     "id": "obj-44",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      615.0,
      395.0,
      140.0,
      20.0
     ],
     "text": "outlet 1: memory size"
    }
   },
   {
    "box": {
     "id": "obj-45",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      250.0,
      505.0,
      300.0,
      20.0
     ],
     "text": "hear it: output 0 -> pitch, output 1 -> level"
    }
   },
   {
    "box": {
     "id": "obj-46",
     "maxclass": "newobj",
     "numinlets": 6,
     "numoutlets": 1,
     "patching_rect": [
      250.0,
      530.0,
      182.0,
      22.0
     ],
     "text": "scale 0. 1. 110. 880. 4.",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-47",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      250.0,
      560.0,
      56.0,
      22.0
     ],
     "text": "cycle~",
     "outlettype": [
      "signal"
     ]
    }
   },
   {
    "box": {
     "id": "obj-48",
     "maxclass": "newobj",
     "numinlets": 6,
     "numoutlets": 1,
     "patching_rect": [
      340.0,
      530.0,
      140.0,
      22.0
     ],
     "text": "scale 0. 1. 0. 0.2",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "obj-49",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      250.0,
      590.0,
      49.0,
      22.0
     ],
     "text": "*~ 0.",
     "outlettype": [
      "signal"
     ]
    }
   },
   {
    "box": {
     "id": "obj-50",
     "maxclass": "ezdac~",
     "numinlets": 2,
     "numoutlets": 0,
     "patching_rect": [
      340.0,
      585.0,
      45.0,
      45.0
     ]
    }
   },
   {
    "box": {
     "id": "obj-51",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      400.0,
      600.0,
      300.0,
      20.0
     ],
     "text": "turn audio on, then like/dislike as you move"
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
      "obj-35",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-6",
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
      "obj-7",
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
      "obj-8",
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
      "obj-9",
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
      "obj-10",
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
      "obj-11",
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
      "obj-19",
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
      "obj-21",
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
      "obj-23",
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
      "obj-25",
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
      "obj-27",
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
      "obj-29",
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
      "obj-31",
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
      "obj-33",
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
      "obj-36",
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
      "obj-38",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-38",
      0
     ],
     "destination": [
      "obj-39",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-38",
      1
     ],
     "destination": [
      "obj-40",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-38",
      2
     ],
     "destination": [
      "obj-41",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-38",
      3
     ],
     "destination": [
      "obj-42",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-35",
      1
     ],
     "destination": [
      "obj-43",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-39",
      0
     ],
     "destination": [
      "obj-46",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-46",
      0
     ],
     "destination": [
      "obj-47",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-40",
      0
     ],
     "destination": [
      "obj-48",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-48",
      0
     ],
     "destination": [
      "obj-49",
      1
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-47",
      0
     ],
     "destination": [
      "obj-49",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-49",
      0
     ],
     "destination": [
      "obj-50",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-49",
      0
     ],
     "destination": [
      "obj-50",
      1
     ]
    }
   }
  ],
  "dependency_cache": [],
  "autosave": 0
 }
}
