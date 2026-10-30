#pragma once

#include "AcidEngine.h"

namespace skacid
{
const std::vector<skazi::Preset>& presets();
skazi::Preset randomPreset (juce::Random& rng);

// Writes a pattern into a preset's values (or a fresh map)
void putPattern (std::map<juce::String, float>& values, const Pattern& p);
} // namespace skacid
