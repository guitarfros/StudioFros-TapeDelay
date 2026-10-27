// StudioFros - Tape Delay
// Copyright (C) 2026 GuitarFros
// SPDX-License-Identifier: GPL-3.0-or-later
#include "PresetManager.h"
#include "Params.h"

namespace sf
{
namespace
{
    struct Override { const char* id; float value; };   // value in the parameter's real range

    struct Factory
    {
        const char* name;
        std::vector<Override> values;
    };

    // Parameters that describe the *sound* (power / bypass are not part of a preset).
    const char* const kPresetParams[] = {
        ids::input, ids::mode, ids::repeatRate, ids::intensity, ids::wowFlutter, ids::saturation,
        ids::tapeAge, ids::syncHost, ids::note, ids::bass, ids::treble, ids::echoVol, ids::reverbVol,
        ids::output, ids::carryover, ids::stereo
    };

    const std::vector<Factory>& factoryPresets()
    {
        static const std::vector<Factory> list = {
            { "Init", {} },
            { "Slapback", { { ids::mode, 7 }, { ids::repeatRate, 0.85f }, { ids::intensity, 0.12f },
                            { ids::wowFlutter, 0.10f }, { ids::saturation, 0.35f }, { ids::tapeAge, 0.20f },
                            { ids::echoVol, 0.70f }, { ids::reverbVol, 0.0f } } },
            { "Studio Plate Echo", { { ids::mode, 1 }, { ids::repeatRate, 0.45f }, { ids::intensity, 0.40f },
                            { ids::wowFlutter, 0.15f }, { ids::saturation, 0.30f }, { ids::tapeAge, 0.30f },
                            { ids::echoVol, 0.55f }, { ids::reverbVol, 0.35f } } },
            { "Dub Echo", { { ids::mode, 1 }, { ids::repeatRate, 0.40f }, { ids::intensity, 0.62f },
                            { ids::wowFlutter, 0.25f }, { ids::saturation, 0.45f }, { ids::tapeAge, 0.40f },
                            { ids::echoVol, 0.75f }, { ids::reverbVol, 0.35f }, { ids::treble, -3.f } } },
            { "Ambient Wash", { { ids::mode, 1 }, { ids::repeatRate, 0.15f }, { ids::intensity, 0.78f },
                            { ids::wowFlutter, 0.40f }, { ids::saturation, 0.30f }, { ids::tapeAge, 0.55f },
                            { ids::echoVol, 0.60f }, { ids::reverbVol, 0.60f }, { ids::treble, -4.f } } },
            { "Worn Tape", { { ids::mode, 11 }, { ids::repeatRate, 0.50f }, { ids::intensity, 0.50f },
                            { ids::wowFlutter, 0.70f }, { ids::saturation, 0.60f }, { ids::tapeAge, 0.90f },
                            { ids::echoVol, 0.70f }, { ids::reverbVol, 0.0f } } },
            { "Host 1/8 Echo", { { ids::mode, 7 }, { ids::syncHost, 1 }, { ids::note, 1 }, { ids::intensity, 0.40f },
                            { ids::wowFlutter, 0.15f }, { ids::saturation, 0.30f }, { ids::tapeAge, 0.25f },
                            { ids::echoVol, 0.60f }, { ids::reverbVol, 0.0f } } },
            { "Host Dotted 1/8", { { ids::mode, 5 }, { ids::syncHost, 1 }, { ids::note, 2 }, { ids::intensity, 0.45f },
                            { ids::wowFlutter, 0.15f }, { ids::saturation, 0.35f }, { ids::tapeAge, 0.30f },
                            { ids::echoVol, 0.60f }, { ids::reverbVol, 0.25f } } },
            { "Spring Space", { { ids::mode, 14 }, { ids::reverbVol, 0.80f }, { ids::echoVol, 0.0f } } },
        };
        return list;
    }

    void setReal (juce::RangedAudioParameter* p, float realValue)
    {
        if (p != nullptr)
            p->setValueNotifyingHost (p->convertTo0to1 (realValue));
    }
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state) {}

juce::File PresetManager::getUserFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("GuitarFros")
               .getChildFile ("StudioFros Tape Delay")
               .getChildFile ("Presets");
}

juce::File PresetManager::fileFor (const juce::String& name) const
{
    return getUserFolder().getChildFile (juce::File::createLegalFileName (name) + ".sfpreset");
}

juce::StringArray PresetManager::getFactoryNames() const
{
    juce::StringArray names;
    for (auto& f : factoryPresets()) names.add (f.name);
    return names;
}

juce::StringArray PresetManager::getUserNames() const
{
    juce::StringArray names;
    auto folder = getUserFolder();
    if (folder.isDirectory())
        for (auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.sfpreset"))
            names.add (f.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}

void PresetManager::resetToDefaults()
{
    for (auto* id : kPresetParams)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->getDefaultValue());
}

void PresetManager::initPreset()
{
    resetToDefaults();
    setCurrent ("Init", true);
}

bool PresetManager::loadFactory (int index)
{
    auto& list = factoryPresets();
    if (index < 0 || index >= (int) list.size()) return false;

    resetToDefaults();
    for (auto& o : list[(size_t) index].values)
        setReal (apvts.getParameter (o.id), o.value);

    setCurrent (list[(size_t) index].name, true);
    return true;
}

bool PresetManager::saveUser (const juce::String& name)
{
    auto cleaned = name.trim();
    if (cleaned.isEmpty()) return false;

    auto folder = getUserFolder();
    if (! folder.createDirectory().wasOk()) return false;

    juce::XmlElement xml ("StudioFrosPreset");
    xml.setAttribute ("name", cleaned);
    xml.setAttribute ("version", 1);
    for (auto* id : kPresetParams)
        if (auto* p = apvts.getParameter (id))
            xml.setAttribute (id, (double) p->convertFrom0to1 (p->getValue()));

    if (! xml.writeTo (fileFor (cleaned))) return false;
    setCurrent (cleaned, false);
    return true;
}

bool PresetManager::loadUser (const juce::String& name)
{
    auto file = fileFor (name);
    if (! file.existsAsFile()) return false;

    auto xml = juce::parseXML (file);
    if (xml == nullptr || ! xml->hasTagName ("StudioFrosPreset")) return false;

    resetToDefaults();
    for (auto* id : kPresetParams)
        if (xml->hasAttribute (id))
            setReal (apvts.getParameter (id), (float) xml->getDoubleAttribute (id));

    setCurrent (name, false);
    return true;
}

bool PresetManager::deleteUser (const juce::String& name)
{
    auto file = fileFor (name);
    const bool ok = file.existsAsFile() && file.deleteFile();
    if (ok && ! currentIsFactory && currentName == name)
        setCurrent ("Init", true);
    return ok;
}
} // namespace sf
