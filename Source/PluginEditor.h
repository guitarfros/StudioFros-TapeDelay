// StudioFros - Tape Delay
// Copyright (C) 2026 GuitarFros
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "GuiComponents.h"

class StudioFrosEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit StudioFrosEditor (StudioFrosProcessor&);
    ~StudioFrosEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        float cx = 0, cy = 0, size = 0;   // design coordinates (1920 x 1280)
    };

    void timerCallback() override;
    void addKnob (const char* paramId, const juce::String& tooltip, float cx, float cy, float size);
    void bindBoolSwitch (sf::gui::LeverSwitch& sw, const char* paramId,
                         std::unique_ptr<juce::ParameterAttachment>& att, const juce::String& tip);

    // presets
    void showPresetMenu();
    void stepPreset (int direction);
    void doSave();
    void doSaveAs();
    void doDelete();
    void commitSaveAs (const juce::String& name);
    void commitDelete (const juce::String& name);
    void flash (const juce::String& message);
    void refreshPresetBar();

    StudioFrosProcessor& proc;

    juce::Image background, scaledBackground;
    sf::gui::TapeLookAndFeel lnf;
    juce::TooltipWindow tooltipWindow { this, 650 };

    std::vector<std::unique_ptr<Knob>> knobs;
    std::unique_ptr<sf::gui::ModeKnob> modeKnob;

    // LEDs
    sf::gui::Led peakLed { juce::Colour (0xffff2a1f) };
    sf::gui::Led head1Led { juce::Colour (0xff4cff4c) }, head2Led { juce::Colour (0xff4cff4c) },
                 head3Led { juce::Colour (0xff4cff4c) }, reverbLed { juce::Colour (0xffffa31a) };

    // switches
    sf::gui::LeverSwitch syncSwitch { 2 }, noteSwitch { 3 }, carrySwitch { 2 }, stereoSwitch { 2 };
    std::unique_ptr<juce::ParameterAttachment> syncAtt, noteAtt, carryAtt, stereoAtt, powerAtt, bypassAtt;

    sf::gui::LampButton powerButton { juce::Colour (0xff2bff3c) };
    sf::gui::LampButton bypassButton { juce::Colour (0xffffa21f) };

    // presets
    sf::gui::PresetBar presetBar;
    sf::gui::PanelButton saveButton, saveAsButton, deleteButton, initButton;
    juce::String flashText;
    juce::uint32 flashUntil = 0;

    std::atomic<float>* modeValue = nullptr;
    std::atomic<float>* powerValue = nullptr;
    std::atomic<float>* bypassValue = nullptr;
    float peakHold = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StudioFrosEditor)
};
