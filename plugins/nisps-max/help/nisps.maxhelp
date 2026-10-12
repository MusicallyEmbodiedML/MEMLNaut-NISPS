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
   860.0,
   620.0
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
  "showontab": 0,
  "showrootpatcherontab": 0,
  "digest": "Neural Interactive Shaping of Parameter Spaces",
  "description": "Maps an input vector to an output vector through a small neural network shaped by like/dislike feedback.",
  "tags": "machine learning, mapping",
  "boxes": [
   {
    "box": {
     "maxclass": "newobj",
     "text": "js helpstarter.js nisps",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "fontname": "Arial",
     "fontsize": 13.0,
     "style": "",
     "patching_rect": [
      445.0,
      350.0,
      190.0,
      23.0
     ],
     "saved_object_attributes": {
      "filename": "helpstarter",
      "parameter_enable": 0
     },
     "id": "root-1"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "p basic",
     "numinlets": 0,
     "numoutlets": 0,
     "fontname": "Arial",
     "fontsize": 13.0,
     "patching_rect": [
      10.0,
      85.0,
      60.0,
      23.0
     ],
     "varname": "basic_tab",
     "style": "",
     "saved_object_attributes": {
      "description": "",
      "digest": "",
      "fontsize": 13.0,
      "globalpatchername": "",
      "tags": ""
     },
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
       860.0,
       620.0
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
      "showontab": 1,
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
         "text": "Maps an input vector to an output vector through a small neural network that you shape by feedback: send 'like' when you hear something good and 'dislike' when you don't, and the mapping around the current input reorganises itself. Arguments: number of inputs, number of outputs. Ticks internally at @interval ms (default 5); outputs the action list whenever it changes. See the storage tab for saving and loading, and the slots tab for jumping between mappings."
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
         "numoutlets": 4,
         "patching_rect": [
          30.0,
          395.0,
          110.0,
          22.0
         ],
         "text": "nisps 2 4",
         "outlettype": [
          "",
          "int",
          "",
          "float"
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
          300.0,
          355.0,
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
          365.0,
          357.0,
          160.0,
          20.0
         ],
         "text": "outlet 1: memory size"
        }
       },
       {
        "box": {
         "id": "obj-45",
         "maxclass": "newobj",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          430.0,
          355.0,
          91.0,
          22.0
         ],
         "text": "print nisps",
         "outlettype": []
        }
       },
       {
        "box": {
         "id": "obj-46",
         "maxclass": "comment",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          520.0,
          357.0,
          150.0,
          20.0
         ],
         "text": "outlet 2: slot replies"
        }
       },
       {
        "box": {
         "id": "obj-47",
         "maxclass": "flonum",
         "numinlets": 1,
         "numoutlets": 2,
         "patching_rect": [
          680.0,
          355.0,
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
         "id": "obj-48",
         "maxclass": "comment",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          680.0,
          377.0,
          170.0,
          20.0
         ],
         "text": "outlet 3: training error"
        }
       },
       {
        "box": {
         "id": "obj-49",
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
         "id": "obj-50",
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
         "id": "obj-51",
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
         "id": "obj-52",
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
         "id": "obj-53",
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
         "id": "obj-54",
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
         "id": "obj-55",
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
          "obj-35",
          2
         ],
         "destination": [
          "obj-45",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-35",
          3
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
          "obj-39",
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
          "obj-50",
          0
         ],
         "destination": [
          "obj-51",
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
          "obj-52",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-52",
          0
         ],
         "destination": [
          "obj-53",
          1
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-51",
          0
         ],
         "destination": [
          "obj-53",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-53",
          0
         ],
         "destination": [
          "obj-54",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-53",
          0
         ],
         "destination": [
          "obj-54",
          1
         ]
        }
       }
      ],
      "dependency_cache": [],
      "autosave": 0
     },
     "id": "root-2"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "p storage",
     "numinlets": 0,
     "numoutlets": 0,
     "fontname": "Arial",
     "fontsize": 13.0,
     "patching_rect": [
      80.0,
      85.0,
      60.0,
      23.0
     ],
     "varname": "storage_tab",
     "style": "",
     "saved_object_attributes": {
      "description": "",
      "digest": "",
      "fontsize": 13.0,
      "globalpatchername": "",
      "tags": ""
     },
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
       860.0,
       620.0
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
      "showontab": 1,
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
          800.0,
          33.0
         ],
         "text": "Saving and loading",
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
          800.0,
          75.0
         ],
         "text": "write captures everything the object has learned - the network's weights and biases, the replay memory with each experience's age, the live position and its mapped action, and the engine parameters - as a Max dictionary written to JSON, the same file idea as pattrstorage. read loads one back. A file that does not match this object's shape is refused outright, with the reason in the Max window, and the running engine is left alone."
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
          140.0,
          300.0,
          20.0
         ],
         "text": "with a dialog"
        }
       },
       {
        "box": {
         "id": "obj-4",
         "maxclass": "message",
         "numinlets": 2,
         "numoutlets": 1,
         "patching_rect": [
          30.0,
          165.0,
          60.0,
          22.0
         ],
         "text": "write",
         "outlettype": [
          ""
         ]
        }
       },
       {
        "box": {
         "id": "obj-5",
         "maxclass": "message",
         "numinlets": 2,
         "numoutlets": 1,
         "patching_rect": [
          100.0,
          165.0,
          60.0,
          22.0
         ],
         "text": "read",
         "outlettype": [
          ""
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
          30.0,
          205.0,
          340.0,
          20.0
         ],
         "text": "straight to a named file"
        }
       },
       {
        "box": {
         "id": "obj-7",
         "maxclass": "message",
         "numinlets": 2,
         "numoutlets": 1,
         "patching_rect": [
          30.0,
          230.0,
          140.0,
          22.0
         ],
         "text": "write mymap.json",
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
          30.0,
          258.0,
          140.0,
          22.0
         ],
         "text": "read mymap.json",
         "outlettype": [
          ""
         ]
        }
       },
       {
        "box": {
         "id": "obj-9",
         "maxclass": "comment",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          180.0,
          232.0,
          300.0,
          20.0
         ],
         "text": "written beside the patcher"
        }
       },
       {
        "box": {
         "id": "obj-10",
         "maxclass": "comment",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          180.0,
          260.0,
          300.0,
          20.0
         ],
         "text": "found along Max's search path"
        }
       },
       {
        "box": {
         "id": "obj-11",
         "maxclass": "comment",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          30.0,
          300.0,
          400.0,
          20.0
         ],
         "text": "or name the file once, as an attribute:"
        }
       },
       {
        "box": {
         "id": "obj-12",
         "maxclass": "message",
         "numinlets": 2,
         "numoutlets": 1,
         "patching_rect": [
          30.0,
          325.0,
          130.0,
          22.0
         ],
         "text": "file mymap.json",
         "outlettype": [
          ""
         ]
        }
       },
       {
        "box": {
         "id": "obj-13",
         "maxclass": "message",
         "numinlets": 2,
         "numoutlets": 1,
         "patching_rect": [
          170.0,
          325.0,
          100.0,
          22.0
         ],
         "text": "autoload 1",
         "outlettype": [
          ""
         ]
        }
       },
       {
        "box": {
         "id": "obj-14",
         "maxclass": "message",
         "numinlets": 2,
         "numoutlets": 1,
         "patching_rect": [
          280.0,
          325.0,
          100.0,
          22.0
         ],
         "text": "autoload 0",
         "outlettype": [
          ""
         ]
        }
       },
       {
        "box": {
         "id": "obj-15",
         "maxclass": "comment",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          30.0,
          353.0,
          780.0,
          45.0
         ],
         "text": "With @file set, a bare write or read uses it instead of opening a dialog, so a patch can save with one message box. @autoload 1 (the default) reads @file when the patcher finishes loading. An object created by scripting never gets a loadbang, so such a patch should send read itself."
        }
       },
       {
        "box": {
         "id": "obj-16",
         "maxclass": "newobj",
         "numinlets": 1,
         "numoutlets": 4,
         "patching_rect": [
          30.0,
          420.0,
          250.0,
          22.0
         ],
         "text": "nisps 2 4 @file mymap.json @autoload 0",
         "outlettype": [
          "",
          "int",
          "",
          "float"
         ]
        }
       },
       {
        "box": {
         "id": "obj-17",
         "maxclass": "number",
         "numinlets": 1,
         "numoutlets": 2,
         "patching_rect": [
          300.0,
          460.0,
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
         "id": "obj-18",
         "maxclass": "comment",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          365.0,
          462.0,
          300.0,
          20.0
         ],
         "text": "memory size, reported after a read"
        }
       },
       {
        "box": {
         "id": "obj-19",
         "maxclass": "newobj",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          30.0,
          460.0,
          91.0,
          22.0
         ],
         "text": "print nisps",
         "outlettype": []
        }
       },
       {
        "box": {
         "id": "obj-20",
         "maxclass": "comment",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          120.0,
          462.0,
          200.0,
          20.0
         ],
         "text": "slot replies"
        }
       },
       {
        "box": {
         "id": "obj-21",
         "maxclass": "comment",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          30.0,
          505.0,
          800.0,
          20.0
         ],
         "text": "The file is plain JSON, so it opens in [dict] or any text editor:"
        }
       },
       {
        "box": {
         "id": "obj-22",
         "maxclass": "comment",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          30.0,
          528.0,
          800.0,
          40.0
         ],
         "text": "{ \"nisps\" : { \"version\" : 1, \"inputs\" : 2, \"outputs\" : 4, \"layer_shape\" : [2, 16, ...], \"weights\" : [...], \"biases\" : [...], \"memory_count\" : 12, ... } }"
        }
       },
       {
        "box": {
         "id": "obj-23",
         "maxclass": "comment",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          30.0,
          572.0,
          800.0,
          32.0
         ],
         "text": "These are the object's own files; they are not interchangeable with models saved on MEMLNaut hardware, whose binary format stores raw size_t values of a different width."
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
          "obj-16",
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
          "obj-16",
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
          "obj-16",
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
          "obj-16",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-12",
          0
         ],
         "destination": [
          "obj-16",
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
          "obj-16",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-14",
          0
         ],
         "destination": [
          "obj-16",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-16",
          1
         ],
         "destination": [
          "obj-17",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-16",
          2
         ],
         "destination": [
          "obj-19",
          0
         ]
        }
       }
      ],
      "dependency_cache": [],
      "autosave": 0
     },
     "id": "root-3"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "p slots",
     "numinlets": 0,
     "numoutlets": 0,
     "fontname": "Arial",
     "fontsize": 13.0,
     "patching_rect": [
      150.0,
      85.0,
      60.0,
      23.0
     ],
     "varname": "slots_tab",
     "style": "",
     "saved_object_attributes": {
      "description": "",
      "digest": "",
      "fontsize": 13.0,
      "globalpatchername": "",
      "tags": ""
     },
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
       860.0,
       620.0
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
      "showontab": 1,
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
          800.0,
          33.0
         ],
         "text": "Slots",
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
          800.0,
          75.0
         ],
         "text": "store 1, store 2, ... keep numbered snapshots in memory, and recall <n> makes one live again. A slot holds everything a file holds - the network, the replay memory and the engine parameters - so recalling is a complete change of mapping rather than a parameter set: a set of slots is a set of instruments to move between mid-performance. Slots are written into the same file as the live state, so write and read carry them along."
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
          140.0,
          300.0,
          20.0
         ],
         "text": "capture what is playing now"
        }
       },
       {
        "box": {
         "id": "obj-4",
         "maxclass": "message",
         "numinlets": 2,
         "numoutlets": 1,
         "patching_rect": [
          30.0,
          165.0,
          70.0,
          22.0
         ],
         "text": "store 1",
         "outlettype": [
          ""
         ]
        }
       },
       {
        "box": {
         "id": "obj-5",
         "maxclass": "message",
         "numinlets": 2,
         "numoutlets": 1,
         "patching_rect": [
          110.0,
          165.0,
          70.0,
          22.0
         ],
         "text": "store 2",
         "outlettype": [
          ""
         ]
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
          165.0,
          70.0,
          22.0
         ],
         "text": "store 3",
         "outlettype": [
          ""
         ]
        }
       },
       {
        "box": {
         "id": "obj-7",
         "maxclass": "comment",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          30.0,
          205.0,
          300.0,
          20.0
         ],
         "text": "jump between them"
        }
       },
       {
        "box": {
         "id": "obj-8",
         "maxclass": "message",
         "numinlets": 2,
         "numoutlets": 1,
         "patching_rect": [
          30.0,
          230.0,
          75.0,
          22.0
         ],
         "text": "recall 1",
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
          115.0,
          230.0,
          75.0,
          22.0
         ],
         "text": "recall 2",
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
          200.0,
          230.0,
          75.0,
          22.0
         ],
         "text": "recall 3",
         "outlettype": [
          ""
         ]
        }
       },
       {
        "box": {
         "id": "obj-11",
         "maxclass": "comment",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          30.0,
          270.0,
          400.0,
          20.0
         ],
         "text": "housekeeping"
        }
       },
       {
        "box": {
         "id": "obj-12",
         "maxclass": "message",
         "numinlets": 2,
         "numoutlets": 1,
         "patching_rect": [
          30.0,
          295.0,
          70.0,
          22.0
         ],
         "text": "erase 2",
         "outlettype": [
          ""
         ]
        }
       },
       {
        "box": {
         "id": "obj-13",
         "maxclass": "message",
         "numinlets": 2,
         "numoutlets": 1,
         "patching_rect": [
          110.0,
          295.0,
          55.0,
          22.0
         ],
         "text": "slots",
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
          175.0,
          297.0,
          400.0,
          20.0
         ],
         "text": "erase empties a slot; slots reports what is occupied"
        }
       },
       {
        "box": {
         "id": "obj-15",
         "maxclass": "comment",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          30.0,
          335.0,
          800.0,
          45.0
         ],
         "text": "Recalling an empty slot warns and changes nothing. A slot whose shape does not fit this object is refused the same way a file is. If any slot in a file fails to load the whole read is refused, naming the slot - a file that silently lost half its presets would be worse."
        }
       },
       {
        "box": {
         "id": "obj-16",
         "maxclass": "newobj",
         "numinlets": 1,
         "numoutlets": 4,
         "patching_rect": [
          30.0,
          400.0,
          110.0,
          22.0
         ],
         "text": "nisps 2 4",
         "outlettype": [
          "",
          "int",
          "",
          "float"
         ]
        }
       },
       {
        "box": {
         "id": "obj-17",
         "maxclass": "multislider",
         "numinlets": 1,
         "numoutlets": 2,
         "patching_rect": [
          30.0,
          445.0,
          200.0,
          90.0
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
         "id": "obj-18",
         "maxclass": "comment",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          30.0,
          540.0,
          220.0,
          20.0
         ],
         "text": "the mapping, before and after a recall"
        }
       },
       {
        "box": {
         "id": "obj-19",
         "maxclass": "newobj",
         "numinlets": 1,
         "numoutlets": 3,
         "patching_rect": [
          260.0,
          445.0,
          126.0,
          22.0
         ],
         "text": "route slots slot",
         "outlettype": [
          "",
          "",
          ""
         ]
        }
       },
       {
        "box": {
         "id": "obj-20",
         "maxclass": "newobj",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          260.0,
          478.0,
          112.0,
          22.0
         ],
         "text": "print occupied",
         "outlettype": []
        }
       },
       {
        "box": {
         "id": "obj-21",
         "maxclass": "number",
         "numinlets": 1,
         "numoutlets": 2,
         "patching_rect": [
          400.0,
          478.0,
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
         "id": "obj-22",
         "maxclass": "comment",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          260.0,
          505.0,
          200.0,
          20.0
         ],
         "text": "reply to slots"
        }
       },
       {
        "box": {
         "id": "obj-23",
         "maxclass": "comment",
         "numinlets": 1,
         "numoutlets": 0,
         "patching_rect": [
          400.0,
          505.0,
          240.0,
          20.0
         ],
         "text": "slot <n> after store / recall"
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
          "obj-16",
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
          "obj-16",
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
          "obj-16",
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
          "obj-16",
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
          "obj-16",
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
          "obj-16",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-12",
          0
         ],
         "destination": [
          "obj-16",
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
          "obj-16",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-16",
          0
         ],
         "destination": [
          "obj-17",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-16",
          2
         ],
         "destination": [
          "obj-19",
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
          "obj-20",
          0
         ]
        }
       },
       {
        "patchline": {
         "source": [
          "obj-19",
          1
         ],
         "destination": [
          "obj-21",
          0
         ]
        }
       }
      ],
      "dependency_cache": [],
      "autosave": 0
     },
     "id": "root-4"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "p ?",
     "numinlets": 0,
     "numoutlets": 0,
     "fontname": "Arial",
     "fontsize": 13.0,
     "patching_rect": [
      220.0,
      85.0,
      60.0,
      23.0
     ],
     "varname": "q_tab",
     "style": "",
     "saved_object_attributes": {
      "description": "",
      "digest": "",
      "fontsize": 13.0,
      "globalpatchername": "",
      "tags": ""
     },
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
       860.0,
       620.0
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
      "showontab": 1,
      "boxes": [],
      "lines": [],
      "dependency_cache": [],
      "autosave": 0
     },
     "id": "root-5"
    }
   }
  ],
  "lines": [],
  "dependency_cache": [],
  "autosave": 0
 }
}
