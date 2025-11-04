
#include "src/memllib/utils/perf.hpp"
#include "src/memllib/interface/MIDIInOut.hpp"
#include "src/memllib/audio/AudioAppBase.hpp"
#include "src/memllib/audio/AudioDriver.hpp"
#include "src/memllib/hardware/memlnaut/MEMLNaut.hpp"
#include <memory>
#include "hardware/structs/bus_ctrl.h"
#include "PAFSynthAudioApp.hpp"
#include "src/memllib/examples/InterfaceRL.hpp"
#include "src/memllib/hardware/memlnaut/display/XYPadView.hpp"  
#include "src/memllib/hardware/memlnaut/display/MessageView.hpp"
#include "src/memllib/hardware/memlnaut/display/VoiceSpaceSelectView.hpp"


#define INTERFACE_TYPE InterfaceRL

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
std::shared_ptr<INTERFACE_TYPE> APP_SRAM interface;

std::shared_ptr<MIDIInOut> APP_SRAM midi_interf;

// Statically allocated, properly aligned storage in AUDIO_MEM for objects
alignas(PAFSynthAudioApp<>) char AUDIO_MEM audio_app_mem[sizeof(PAFSynthAudioApp<>)];
std::shared_ptr<PAFSynthAudioApp<> > __scratch_y("audio") audio_app;

// Inter-core communication
volatile bool APP_SRAM core_0_ready = false;
volatile bool APP_SRAM core_1_ready = false;
volatile bool APP_SRAM serial_ready = false;
volatile bool APP_SRAM interface_ready = false;

std::array<String, PAFSynthAudioApp<>::nVoiceSpaces> voiceSpaceList;



// We're only bound to the joystick inputs (x, y, rotate)
constexpr size_t kN_InputParams = 3;

// Add these macros near other globals
#define MEMORY_BARRIER() __sync_synchronize()
#define WRITE_VOLATILE(var, val) \
  do { \
    MEMORY_BARRIER(); \
    (var) = (val); \
    MEMORY_BARRIER(); \
  } while (0)
#define READ_VOLATILE(var) ({ MEMORY_BARRIER(); typeof(var) __temp = (var); MEMORY_BARRIER(); __temp; })


// struct repeating_timer APP_SRAM timerDisplay;
// inline bool __not_in_flash_func(displayUpdate)(__unused struct repeating_timer *t) {
//     scr.update();
//     return true;
// }

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

  {
    auto temp_interface = std::make_shared<INTERFACE_TYPE>();
    temp_interface->setup(kN_InputParams, PAFSynthAudioApp<>::kN_Params);
    MEMORY_BARRIER();
    interface = temp_interface;
    MEMORY_BARRIER();
  }
  // Setup interface with memory barrier protection
  WRITE_VOLATILE(interface_ready, true);
  // Bind interface after ensuring it's fully initialized
  interface->bindInterface(false);
  Serial.println("Bound interface to MEMLNaut.");


  midi_interf = std::make_shared<MIDIInOut>();
  midi_interf->Setup(0);
  midi_interf->SetMIDISendChannel(1);
  Serial.println("MIDI setup complete.");
  if (midi_interf) {
    midi_interf->SetNoteCallback([interface](bool noteon, uint8_t note_number, uint8_t vel_value) {
      if (noteon) {
        uint8_t midimsg[2] = { note_number, vel_value };
        queue_try_add(&audio_app->qMIDINoteOn, &midimsg);
      }else{
        uint8_t midimsg[2] = { note_number, vel_value };
        queue_try_add(&audio_app->qMIDINoteOff, &midimsg);
      }
      // Serial.printf("MIDI Note %d: %d %d\n", note_number, vel_value, noteon);
    });
    // Serial.println("MIDI note callback set.");

    interface->bindMIDI(midi_interf);
  }



  WRITE_VOLATILE(core_0_ready, true);
  while (!READ_VOLATILE(core_1_ready)) {
    MEMORY_BARRIER();
    delay(1);
  }

  std::shared_ptr<XYPadView> noteTrigView = std::make_shared<XYPadView>("Play", TFT_SILVER);

  // Cache MIDI notes being echoed
  static bool is_playing_note = false;
  static uint8_t last_note_number = 0;

  noteTrigView->SetOnTouchCallback([](float x, float y) {
      // Serial.printf("Note trigger at: %.2f, %.2f\n", x, y);
      if (audio_app) {
          // If a note is already playing, stop it
          if (is_playing_note) {
              midi_interf->sendNoteOff(last_note_number, 0);
              is_playing_note = false;
          }
          int noteVel = static_cast<uint8_t>(powf(y * (1.f/127.f), 0.5f) * 127.f);
          uint8_t midimsg[2] = {static_cast<uint8_t>(x * 127.f), noteVel};
          queue_try_add(&audio_app->qMIDINoteOn, &midimsg);
          midi_interf->sendNoteOn(midimsg[0], midimsg[1]);
          last_note_number = midimsg[0];
          is_playing_note = true; // Set flag to indicate a note is playing
      }
  });
  noteTrigView->SetOnTouchReleaseCallback([](float x, float y) {
      // Serial.printf("Note release at: %.2f, %.2f\n", x, y);
      if (audio_app) {
          uint8_t midimsg[2] = {last_note_number,0};
          queue_try_add(&audio_app->qMIDINoteOff, &midimsg);
          midi_interf->sendNoteOff(last_note_number, 0);
          is_playing_note = false; // Reset flag when note is released
      }
  });

  std::shared_ptr<VoiceSpaceSelectView> voiceSpaceSelectView;
  voiceSpaceSelectView = std::make_shared<VoiceSpaceSelectView>("Voice Spaces");
  
  MEMLNaut::Instance()->disp->InsertViewAfter(interface->rlStatsView, voiceSpaceSelectView);
  voiceSpaceSelectView->setOptions(voiceSpaceList); //set by core 1 on startup
  voiceSpaceSelectView->setNewVoiceCallback(
    [](size_t idx) {
      // Serial.println(idx);
      audio_app->setVoiceSpace(idx);
    }
  );

  MEMLNaut::Instance()->disp->AddView(noteTrigView);

  std::shared_ptr<MessageView> helpView = std::make_shared<MessageView>("Help");
  helpView->post("PAF synth NISPS");
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
    MEMLNaut::Instance()->loop();
    PERF_END(MLSTATS);
    , ML_INFERENCE_PERIOD_US)

  PERIODIC_RUN_US(
    static size_t blip_counter=0;
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
    }
    , 100000)

  

}


void AUDIO_FUNC(audio_block_callback)(float in[][kBufferSize], float out[][kBufferSize], size_t n_channels, size_t n_frames) {
  for (size_t i = 0; i < n_frames; ++i) {

    stereosample_t x{
      in[0][i],
      in[1][i]
    },
      y;

    // Audio processing
    if (audio_app) {
      y = audio_app->Process(x);
    }

    out[0][i] = y.L;
    out[1][i] = y.R;
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


  // Create audio app with memory barrier protection
  {
    PAFSynthAudioApp<>* audio_raw = new (audio_app_mem) PAFSynthAudioApp<>();
    audio_raw->Setup(AudioDriver::GetSampleRate(), interface);

    // shared_ptr with custom deleter calling only the destructor (control block still allocates)
    auto audio_deleter = [](PAFSynthAudioApp<>* p) {
      if (p) p->~PAFSynthAudioApp<>();
    };
    std::shared_ptr<PAFSynthAudioApp<>> temp_audio_app(audio_raw, audio_deleter);

    MEMORY_BARRIER();
    audio_app = temp_audio_app;
    MEMORY_BARRIER();
  }

  AudioDriver::SetBlockCallback(audio_block_callback);
  // Start audio driver
  AudioDriver::Setup();
  // AudioDriver::SetBlockCallback(audio_block_callback);
  voiceSpaceList = audio_app->getVoiceSpaceNames();

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
    audio_app->loop();
    , ML_INFERENCE_PERIOD_US)
  
  PERIODIC_RUN_US(
    midi_interf->Poll();
    , 10000)

// #if 1 //test ARP
//   PERIODIC_RUN_US(
//     static size_t arpCount=0;
//     static size_t noteIndex=30;

//     , 100000)
// #endif

}
