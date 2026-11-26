#pragma once
#include "Processor.h"

class LoopMasterEditor : public juce::AudioProcessorEditor {
 public:
  explicit LoopMasterEditor(LoopMasterProcessor&);
  ~LoopMasterEditor() override;
  void resized() override;
  void paint(juce::Graphics&) override;

 private:
  struct Body;
  std::unique_ptr<Body> body_;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopMasterEditor)
};
