// StudioFros - Tape Delay
// Copyright (C) 2026 GuitarFros
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace sf
{
/** Factory presets (built in) + user presets (XML files in the user's
    application-data folder, e.g. ~/.config/GuitarFros/StudioFros Tape Delay/Presets). */
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    juce::StringArray getFactoryNames() const;
    juce::StringArray getUserNames() const;

    bool loadFactory (int index);
    bool loadUser (const juce::String& name);
    bool saveUser (const juce::String& name);
    bool deleteUser (const juce::String& name);
    void initPreset();

    const juce::String& getCurrentName() const noexcept { return currentName; }
    bool isCurrentFactory() const noexcept { return currentIsFactory; }
    void setCurrent (const juce::String& name, bool factory) { currentName = name; currentIsFactory = factory; }

    juce::File getUserFolder() const;

private:
    void resetToDefaults();
    juce::File fileFor (const juce::String& name) const;

    juce::AudioProcessorValueTreeState& apvts;
    juce::String currentName { "Init" };
    bool currentIsFactory = true;
};
} // namespace sf
