#pragma once

#include <juce_core/juce_core.h>

// A factory preset lists only the parameters it changes, in real units (Hz, dB, %, choice index).
// Everything it leaves out goes back to its default when the preset loads.
struct Preset
{
    juce::String category, name;
    std::vector<std::pair<juce::String, float>> values;
};

const std::vector<Preset>& factoryPresets();
