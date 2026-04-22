#pragma once

#include <concepts>
#include <memory>
#include <span>
#include "WString.h"
#include "../src/memllib/interface/InterfaceBase.hpp"
#include "../src/memllib/interface/MIDIInOut.hpp"
#include "../src/memllib/audio/AudioDriver.hpp"

template<typename T>
concept MEMLNautMode = requires(T proc) {
  {proc.getHelpTitle()} -> std::same_as<String>;
  // {proc.getNParams()} -> std::same_as<size_t>;
  // {proc.setVoiceSpace(size_t{})} -> std::same_as<void>;
  {proc.setupMIDI(std::shared_ptr<MIDIInOut>{})} -> std::same_as<void>;
  {proc.addViews()} -> std::same_as<void>;
  {proc.setupAudio(float{})} -> std::same_as<void>;
  {proc.loop()} -> std::same_as<void>;
  // {proc.getVoiceSpaceList()} -> std::same_as<std::span<String>>;
  {proc.process(stereosample_t{})} -> std::same_as<stereosample_t>;
  {proc.analyse(stereosample_t{})} -> std::same_as<void>;
  {proc.processAnalysisParams()} -> std::same_as<void>;
  // {proc.getNMIDICtrlOutputs()} -> std::same_as<size_t>;
  {proc.setupInterface()} -> std::same_as<void>;
  {proc.loopCore0()} -> std::same_as<void>;
  requires std::same_as<decltype(T::kN_InputParams), const size_t>;
  requires std::same_as<decltype(T::kDesiredSampleRate), const size_t>;
};
