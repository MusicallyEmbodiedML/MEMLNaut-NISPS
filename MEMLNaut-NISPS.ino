
//hardware
#include "src/memllib/utils/perf.hpp"
#include "src/memllib/interface/MIDIInOut.hpp"
#include "src/memllib/audio/AudioDriver.hpp"
#include "src/memllib/hardware/memlnaut/MEMLNaut.hpp"
#include "hardware/structs/bus_ctrl.h"
#include <memory>

//sound
#include "src/memllib/audio/AudioAppBase.hpp"
#include "PAFSynthAudioApp.hpp"
#include "ChannelStripAudioApp.hpp"

//interface
// #include "src/memllib/examples/InterfaceRL.hpp"
#include "src/memllib/hardware/memlnaut/display/XYPadView.hpp"
#include "src/memllib/hardware/memlnaut/display/MessageView.hpp"
#include "src/memllib/hardware/memlnaut/display/VoiceSpaceSelectView.hpp"

//modes
#include "modes/MEMLNautMode.hpp"
#include "modes/MEMLNautModePAFSynth.hpp"
#include "modes/MEMLNautModeChannelStrip.hpp"
#include "modes/MEMLNautModeSoundAnalysisMIDI.hpp"
#include "modes/MEMLNautModeXIASRI.hpp"

//hook up the memlnaut mode 

// #define MEMLNAUT_MODE_TYPE MEMLNautModeSoundAnalysisMIDI
// #define MEMLNAUT_MODE_TYPE MEMLNautModeXIASRI
#define MEMLNAUT_MODE_TYPE MEMLNautModeChannelStrip

MEMLNAUT_MODE_TYPE AUDIO_MEM MEMLNautModeHub;

// MEMLNAUT_MODE_TYPE AUDIO_MEM soundAnalysisMIDIMode;
// MEMLNautModeChannelStrip AUDIO_MEM channelStripMode;
// MEMLNautModePAFSynth AUDIO_MEM pafSynthMode;

MEMLNautMode auto* AUDIO_MEM currentMode = &MEMLNautModeHub;


#define APP_SRAM __not_in_flash("app")

bool core1_disable_systick = true;
bool core1_separate_stack = true;

uint32_t get_rosc_entropy_seed(int bits) {
  uint32_t seed = 0;
  for (int i = 0; i < bits; ++i) {
    // Wait for a bit of time to allow jitter to accumulate
    busy_wait_us_32(5);
    // Pull LSB from ROSC rand output
    seed <<= 1;
    seed |= (rosc_hw->randombit & 1);
  }
  return seed;
}


// Global objects

std::shared_ptr<MIDIInOut> APP_SRAM midi_interf;

// Inter-core communication
volatile bool APP_SRAM core_0_ready = false;
volatile bool APP_SRAM core_1_ready = false;
volatile bool APP_SRAM serial_ready = false;
volatile bool APP_SRAM interface_ready = false;





// Add these macros near other globals
#define MEMORY_BARRIER() __sync_synchronize()
#define WRITE_VOLATILE(var, val) \
  do { \
    MEMORY_BARRIER(); \
    (var) = (val); \
    MEMORY_BARRIER(); \
  } while (0)
#define READ_VOLATILE(var) ({ MEMORY_BARRIER(); typeof(var) __temp = (var); MEMORY_BARRIER(); __temp; })


void setup() {
  set_sys_clock_khz(AudioDriver::GetSysClockSpeed(), true);

  bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS | BUSCTRL_BUS_PRIORITY_PROC1_BITS;

  uint32_t seed = get_rosc_entropy_seed(32);
  srand(seed);

  Serial.begin(115200);
  // while (!Serial) {}
  Serial.println("Serial initialised.");
  WRITE_VOLATILE(serial_ready, true);

  // Setup board
  MEMLNaut::Initialize();
  pinMode(33, OUTPUT);

  currentMode->setupInterface();

  // Setup interface with memory barrier protection
  WRITE_VOLATILE(interface_ready, true);
  Serial.println("Bound interface to MEMLNaut.");

  midi_interf = std::make_shared<MIDIInOut>();
  Serial.println("MIDI setup complete.");
  if (midi_interf) {
    currentMode->setupMIDI(midi_interf);
  }

  WRITE_VOLATILE(core_0_ready, true);
  while (!READ_VOLATILE(core_1_ready)) {
    MEMORY_BARRIER();
    delay(1);
  }


  currentMode->addViews();

  std::shared_ptr<MessageView> helpView = std::make_shared<MessageView>("Help");
  String title = currentMode->getHelpTitle();
  helpView->post(title);
  helpView->post("TA: Down: Clear replay memory");
  helpView->post("MA: Up: Randomise / Down: Jolt ");
  helpView->post("MB: Up: Positive reward");
  helpView->post("MB: Down: Negative reward");
  helpView->post("X: Learning rate");
  helpView->post("Y: Reward Scale");
  helpView->post("Z: Exploration noise");
  helpView->post("Joystick: Explore / SW: Drag sound");
  MEMLNaut::Instance()->disp->AddView(helpView);

  MEMLNaut::Instance()->addSystemInfoView();

  Serial.println("Finished initialising core 0.");
}

PERF_DECLARE(MLSTATS);

#define ML_INFERENCE_PERIOD_US 5000

void loop() {

  PERIODIC_RUN_US(
    PERF_BEGIN(MLSTATS);
    currentMode->processAnalysisParams();
    MEMLNaut::Instance()->loop();
    PERF_END(MLSTATS);
    , ML_INFERENCE_PERIOD_US)

  //show profiling stats
  PERIODIC_RUN_US(
    static size_t blip_counter = 0;
    if (blip_counter++ > 10) {
      blip_counter = 0;
      Serial.println(".");
      // Blink LED
      digitalWrite(33, HIGH);
      constexpr float audioHeadroomMul = 1.0 / (1000000 * 48.0 / kSampleRate);
      Serial.printf("ml: %d, aud: %d, q: %f\n", PERF_GET_MEAN(MLSTATS), AUDIOLOOP_MEAN, AUDIOLOOP_MEAN * audioHeadroomMul);
    } else {
      // Un-blink LED
      digitalWrite(33, LOW);
    },
    100000)
}


void AUDIO_FUNC(audio_block_callback)(float in[][kBufferSize], float out[][kBufferSize], size_t n_channels, size_t n_frames) {

  for (size_t i = 0; i < n_frames; ++i) {

    stereosample_t x{
      in[0][i],
      in[1][i]
    },
      y;

    y = currentMode->process(x);

    out[0][i] = y.L;
    out[1][i] = y.R;

      // PERIODIC_RUN(
      //   Serial.printf("x: %f\n", x.L + x.R);
      //   , 100);

    currentMode->analyse(x);
  }
}


void setup1() {
  while (!READ_VOLATILE(serial_ready)) {
    MEMORY_BARRIER();
    delay(1);
  }

  while (!READ_VOLATILE(interface_ready)) {
    MEMORY_BARRIER();
    delay(1);
  }


  currentMode->setupAudio(AudioDriver::GetSampleRate());

  AudioDriver::SetBlockCallback(audio_block_callback);

  // Start audio driver
  AudioDriver::Setup();

  WRITE_VOLATILE(core_1_ready, true);
  while (!READ_VOLATILE(core_0_ready)) {
    MEMORY_BARRIER();
    delay(1);
  }

  Serial.println("Finished initialising core 1.");
}


void loop1() {
  // Audio app parameter processing loop
  PERIODIC_RUN_US(
    currentMode->loop();
    , ML_INFERENCE_PERIOD_US)

  PERIODIC_RUN_US(
    midi_interf->Poll();
    , 10000)
}
