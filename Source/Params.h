// StudioFros - Tape Delay
// Copyright (C) 2026 GuitarFros
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

namespace sf
{
namespace ids
{
    inline constexpr const char* input      = "input";
    inline constexpr const char* mode       = "mode";
    inline constexpr const char* repeatRate = "repeatRate";
    inline constexpr const char* intensity  = "intensity";
    inline constexpr const char* wowFlutter = "wowFlutter";
    inline constexpr const char* saturation = "saturation";
    inline constexpr const char* tapeAge    = "tapeAge";
    inline constexpr const char* syncHost   = "syncHost";
    inline constexpr const char* note       = "note";
    inline constexpr const char* bass       = "bass";
    inline constexpr const char* treble     = "treble";
    inline constexpr const char* echoVol    = "echoVol";
    inline constexpr const char* reverbVol  = "reverbVol";
    inline constexpr const char* output     = "output";
    inline constexpr const char* carryover  = "carryover";
    inline constexpr const char* stereo     = "stereo";
    inline constexpr const char* power      = "power";
    inline constexpr const char* bypass     = "bypass";
}

//==============================================================================
/** One position of the MODE SELECTOR.  The angle is where the pointer sits on
    the artwork (degrees clockwise from 12 o'clock) - it matches the tick marks
    printed on the background image. */
struct ModeInfo
{
    const char* name;
    int   headMask;   // bit0 = head 1, bit1 = head 2, bit2 = head 3
    bool  reverb;
    float angleDeg;
};

inline constexpr int kNumModes = 15;
inline constexpr int kDefaultMode = 7;

inline constexpr ModeInfo kModes[kNumModes] =
{
    { "1+3 + Reverb",     5, true,  -157.5f },
    { "1+2+3 + Reverb",   7, true,  -135.0f },   // "Eco+Rev" on the panel
    { "2+3 + Reverb",     6, true,  -112.5f },   // "Eco"
    { "1+2 + Reverb",     3, true,   -90.0f },
    { "3 + Reverb",       4, true,   -67.5f },
    { "1 + Reverb",       1, true,   -45.0f },
    { "2 + Reverb",       2, true,   -22.5f },
    { "1",                1, false,    0.0f },
    { "2",                2, false,   22.5f },
    { "3",                4, false,   45.0f },
    { "1+2",              3, false,   67.5f },
    { "1+2+3",            7, false,   90.0f },
    { "2+3",              6, false,  112.5f },
    { "1+3",              5, false,  135.0f },
    { "Reverb only",      0, true,   157.5f },
};

//==============================================================================
/** REPEAT RATE: clockwise = faster tape = shorter delay (like the real thing).
    Returns the delay of head 1 in milliseconds. */
inline float repeatRateToMs (float rate01) noexcept
{
    constexpr float slowMs = 450.f, fastMs = 40.f;
    return slowMs * std::pow (fastMs / slowMs, rate01);
}

//==============================================================================
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    using Attr = AudioParameterFloatAttributes;

    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = [] (float v, int) { return String (roundToInt (v * 100.f)) + " %"; };
    auto dB  = [] (float v, int) { return String (v, 1) + " dB"; };

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::input, 1 }, "Input",
        NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f, Attr().withStringFromValueFunction (dB)));

    StringArray modeNames;
    for (auto& m : kModes) modeNames.add (m.name);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::mode, 1 }, "Mode Selector",
        modeNames, kDefaultMode));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::repeatRate, 1 }, "Repeat Rate",
        NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f,
        Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (repeatRateToMs (v))) + " ms"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::intensity, 1 }, "Intensity",
        NormalisableRange<float> (0.f, 1.f, 0.001f), 0.35f, Attr().withStringFromValueFunction (pct)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::wowFlutter, 1 }, "Wow & Flutter",
        NormalisableRange<float> (0.f, 1.f, 0.001f), 0.2f, Attr().withStringFromValueFunction (pct)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::saturation, 1 }, "Saturation",
        NormalisableRange<float> (0.f, 1.f, 0.001f), 0.3f, Attr().withStringFromValueFunction (pct)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::tapeAge, 1 }, "Tape Age",
        NormalisableRange<float> (0.f, 1.f, 0.001f), 0.25f, Attr().withStringFromValueFunction (pct)));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::syncHost, 1 }, "Sync (Free/Host)", false));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::note, 1 }, "Note",
        StringArray { "1/4", "1/8", "1/8 dotted" }, 1));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::bass, 1 }, "Bass",
        NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f, Attr().withStringFromValueFunction (dB)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::treble, 1 }, "Treble",
        NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f, Attr().withStringFromValueFunction (dB)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::echoVol, 1 }, "Echo Volume",
        NormalisableRange<float> (0.f, 1.f, 0.001f), 0.6f, Attr().withStringFromValueFunction (pct)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::reverbVol, 1 }, "Reverb Volume",
        NormalisableRange<float> (0.f, 1.f, 0.001f), 0.3f, Attr().withStringFromValueFunction (pct)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::output, 1 }, "Output",
        NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f, Attr().withStringFromValueFunction (dB)));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::carryover, 1 }, "Carryover", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::stereo, 1 }, "Stereo", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::power, 1 }, "Power", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::bypass, 1 }, "Bypass", false));

    return { p.begin(), p.end() };
}

} // namespace sf
