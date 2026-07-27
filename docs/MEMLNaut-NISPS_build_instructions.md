# MEMLNaut-NISPS Build Instructions
The following describes how to build the MEMLNaut-NISPS firmware using the Arduino IDE.
It references some information that is found [here](https://github.com/MusicallyEmbodiedML/memllib/blob/e291192d8e4f2fca7b79670c4df9c2ec8bdf03cd/README.md).

These build instructions assume you store your Arduino sketches in `${ARDUINO_SKETCH_DIR}`
and you store your Arduino libraries in `${ARDUINO_LIBRARY_DIR}`.

These instructions refer to some Arduino files that are normally hidden from file
browsers unless you enable viewing hidden files.
See [Find sketches, libraries, board cores, and other files on your computer](https://support.arduino.cc/hc/en-us/articles/4415103213714-Find-sketches-libraries-board-cores-and-other-files-on-your-computer)
for help finding most of them.

This document assumes you have access to a command line terminal and have the `git` source code control tool installed. It also assumes you have the Arduino IDE installed. The instructions were written for Arduino
IDE version 2.3.10.

There may come a time where the Arduino IDE changes enough so that these instructions
no longer work. Please file an issue (or better still, a pull request)
if you discover anything wrong with these instructions.

## Get the MEMLNaut Source Code
```
cd ${ARDUINO_SKETCH_DIR}
git clone https://github.com/MusicallyEmbodiedML/MEMLNaut-NISPS.git
cd MEMLNaut-NISPS
git submodule update --recursive --init
```

## Install the RP2350 Arduino Core
On PC and Linux, access the menu
File->Preferences...
On Mac systems, the menu is under
Arduino IDE->Preferences...

In the Settings tab, find "Additional board manager URLs:" Add
```
https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
```
to the box next to it and click OK.

Next, click on the Boards Manager icon on the left side of the screen and search for RP2350. The
Raspberry Pi Pico/RP2040/RP2350 core should be listed. Install it.

## Add C++20 compiler support to the core configuration
By default, the 5.70 version of this core supports C++17. The MEMLNaut-NISPS sketch requires C++20 or
later to build. You need to modify the `platform.txt` file for the Arduino core.
For example, on a Linux system, the core is stored in
```
${HOME}/.arduino15/packages/rp2040/hardware/rp2040/5.7.0
```
Use your favorite editor to open the `platform.txt` file and find the line in the file that reads

```
compiler.cpp.flags=-c {compiler.warning_flags} {compiler.defines} {compiler.flags} -MMD {compiler.includes} {build.flags.rtti} {build.flags.profile} -std=gnu++17 -g -pipe
```
Replace `-std=gnu++17` with `-std=gnu++20`.

Note that if you update the Arduino core in the board manager, then you may have to make this edit again.
There will probably come a time when this step is no longer required.

## Install other necessary libraries
Use the Library Manager to install the following libraries. The referenced versions were used
to compile this document. There may be newer versions by the time you read this. If the build fails
with newer versions, consider rolling back to the library versions referenced here.
- MIDI Library (by Francois Best, lathoub) : version 5.0.2
- TFT_eSPI (by Bodmer) : version 2.5.43
- TFT_eWidget (by Bodmer) : version 0.0.5.

## Configure the TFT_eSPI library
Copy the file
```
${ARDUINO_SKETCH_DIR}/MEMNaut-NISPS/src/memllib/hardware/memlnaut/Setup9999_MEMLNaut.h.renameme
```
to the directory 

```
${ARDUINO_LIBRARY_DIR}/TFT_eSPI/User_Setups/
```

and rename it to

```
${ARDUINO_LIBRARY_DIR}/TFT_eSPI/User_Setups/Setup9999_MEMLNaut.h
```

In your favorite editor, open the file `${ARDUINO_LIBRARY_DIR}/TFT_eSPI/User_Setup_Select.h`.

Comment out the line
```
#include <User_Setup.h>           // Default setup is root library folder`
```
and add the line 
```
#include <User_Setups/Setup9999_MEMLNaut.h>
```
Make sure there is only
the desired `*_Setup.h` file included and all others are commented out.

If you update the `TFT_eSPI` library, you will likely have to do this step over again.

## Open the sketch
In the Arduino IDE, use the File->Open menu to open the `MEMLNaut-NISPS.ino` sketch.

## Select the correct board target
Under the Tools->Board: menu, select "Raspberry Pi Pico/2040/2350->Solder Party RP2350 Stamp XL"

## Select the Build Optimization
Optimize this code as much as possible. Use the Menu item Tools->Optimize:"Optimize Even More (-O3)"

## Set aside space for the LittleFS flash file system
The input source selection is store in the file `/input_source.bin` in the RP2350's flash chip. You
need to set aside some space in the flash for the file system. The file is small. Use the Menu item
Tools->Flash Size "16MB (Sketch:16320KB, FS:64KB)"

## Choose the mode you wish to build
You can glean information about the various modes [here](https://musicallyembodiedml.github.io/posts/).

In the Arduino IDE, find the line in the `MEMLNaut-NISPS.ino` file that reads
```
//modes — uncomment exactly ONE to select the active mode:
```
Under that comment are a list of operating mode definitions. Uncomment the `#define` line for the mode you want and comment out the rest.

## Start the build
Click the Verify checkmark in the Arduino IDE. The code should compile, perhaps with some warnings.
See the next step if you are using an ARM-based Mac and the build failed.

## How to fix a build failure on an ARM-based Mac that is not running Rosetta
If you try to build on an ARM-based Macbook, the build may fail because the Arduino built-in ctags tool
is built for Intel-based Macs only. Installing Rosetta will likely fix this issue, but Apple will stop
supporting Rosetta soon. You may not want to
install it. Fortunately, a [post](https://forum.arduino.cc/t/how-to-avoid-installing-rosetta-on-an-apple-silicon-mac/1346623#p-8042292-step-2-install-ctags-2)
contains a fix that does not require Rosetta. Use the following steps, where `${ARDUINO_PATCH_DIR}`
is the directory you use for cloning the ctags source code.

1. If you do not have the Xcode command line tools installed, open a terminal window and run this command
```
xcode-select --install
```

2. Get the Arduino ctags source code.

```
cd ${ARDUINO_PATCH_DIR}
git clone https://github.com/arduino/ctags
```
3. Edit the file `${ARDUINO_PATCH_DIR}/ctags/general.h` with your favorite source code editor. Find the line

```
# define __unused__  __attribute__((unused))
```

and replace it with

```
# define __unused__
```

4. Build and install the ctags code as follows

```
cd ctags  # Navigate to the cloned ctags directory. Omit this step if your terminal is already in the ctags directory
./configure
make
mv ~/Library/Arduino15/packages/builtin/tools/ctags/5.8-arduino11/ctags ~/Library/Arduino15/packages/builtin/tools/cctags/5.8-arduino11/ctags.bak
cp ctags ~/Library/Arduino15/packages/builtin/tools/ctags/5.8-arduino11/
```

Note that if you update the Arduino IDE, you will probably have to update the ctags tool again. Eventually,
Arduino will release an IDE with built-in tools compiled for ARM-based Macs. Hopefully this happens
soon so that this step will not be required.

## Export the .uf2 file and program the MEMLNaut
In the Arduino IDE, use the menu Sketch->Export Compiled Binary. The IDE will create the
```
build/rp2040.rp2040.solderparty_rp2350_stamp_xl/
```
directory. The directory contains the build output, including the `MEMLNaut-NISPS.ino.uf2` file. You may program this .uf2
file to the MEMLNaut hardware using the RP2350's bootloader:

1. Connect MEMLNaut hardware to a computer via a USB cable
2. Press and hold the MEMLNaut BOOTSEL button.
3. Press and release the MEMLNaut Reset button.
4. Release the MEMLNaut BOOTSEL button.
5. The computer will mount the MEMLNaut hardware as a USB drive called
`RP2350`. Copy the .uf2 file to this USB drive.
6. The MEMLNaut hardware will reboot and run the firmware on the .uf2 file. Your operating system may complain that the `RP2350` USB drive was unplugged
before it was safely ejected. You may ignore this warning. The MEMLNaut
hardware is also now ready to run with the Arduino IDE if you choose to do so.
