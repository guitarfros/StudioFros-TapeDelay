// StudioFros - Tape Delay
// Copyright (C) 2026 GuitarFros
// SPDX-License-Identifier: GPL-3.0-or-later
#include "PluginEditor.h"
#include "BinaryData.h"

using namespace sf;

namespace
{
    // The artwork is 1920 x 1280; every control is positioned in those coordinates
    // and scaled when the window is resized.
    constexpr float kDesignW = 1920.f;
    constexpr float kDesignH = 1280.f;
}

StudioFrosEditor::StudioFrosEditor (StudioFrosProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    background = juce::ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);

    modeValue   = proc.apvts.getRawParameterValue (ids::mode);
    powerValue  = proc.apvts.getRawParameterValue (ids::power);
    bypassValue = proc.apvts.getRawParameterValue (ids::bypass);

    // ---- knobs ----------------------------------------------------------------
    // INPUT section
    addKnob (ids::input, "Input level (into the tape)", 309, 538, 165);

    // TEMPO / TAPE row
    addKnob (ids::repeatRate, "Repeat Rate (tape speed: clockwise = shorter delay)", 1021, 373, 160);
    addKnob (ids::intensity,  "Intensity (feedback - turn up for self-oscillation)", 1177, 373, 160);
    addKnob (ids::wowFlutter, "Wow & Flutter",                                        1334, 373, 160);
    addKnob (ids::saturation, "Saturation (tape compression)",                        1491, 373, 160);
    addKnob (ids::tapeAge,    "Tape Age (HF loss, hiss, dropouts)",                   1647, 373, 160);

    // MIX / TONE row - same five columns as the TEMPO / TAPE row; every knob has its own caption
    addKnob (ids::bass,      "Bass",          1021, 614, 160);
    addKnob (ids::treble,    "Treble",        1177, 614, 160);
    addKnob (ids::output,    "Output level",  1334, 614, 160);
    addKnob (ids::echoVol,   "Echo Volume",   1491, 614, 160);
    addKnob (ids::reverbVol, "Reverb Volume", 1647, 614, 160);

    // ---- mode selector ------------------------------------------------------
    modeKnob = std::make_unique<gui::ModeKnob> (*proc.apvts.getParameter (ids::mode));
    modeKnob->setTooltip ("Mode Selector - heads 1/2/3, with or without reverb (drag around the dial or use the mouse wheel)");
    addAndMakeVisible (*modeKnob);

    // ---- LEDs ----------------------------------------------------------------
    for (auto* l : { &peakLed, &head1Led, &head2Led, &head3Led, &reverbLed })
        addAndMakeVisible (*l);

    // ---- switches (one physical switch per function) -----------------------------
    bindBoolSwitch (syncSwitch, ids::syncHost, syncAtt,
                    "Sync: left = FREE (delay from Repeat Rate), right = HOST (locked to the host tempo, see Note)");

    noteAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (ids::note),
                                                           [this] (float v) { noteSwitch.setState ((int) std::lround (v)); });
    noteSwitch.setTooltip ("Note value used in HOST sync: left = 1/4, centre = 1/8, right = dotted 1/8");
    noteSwitch.onChange = [this] (int s) { noteAtt->setValueAsCompleteGesture ((float) s); };
    noteAtt->sendInitialUpdate();
    addAndMakeVisible (noteSwitch);

    bindBoolSwitch (carrySwitch, ids::carryover, carryAtt,
                    "Carryover: left = OFF (echoes stop when bypassed), right = ON (echoes ring out)");
    bindBoolSwitch (stereoSwitch, ids::stereo, stereoAtt,
                    "Left = MONO (echo centred), right = STEREO (heads 1 / 2 / 3 spread left / centre / right)");

    // ---- power / bypass --------------------------------------------------------------
    powerButton.setTooltip ("Power - off = unit completely out of the signal path");
    powerAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (ids::power),
                                                            [this] (float v) { powerButton.setOn (v > 0.5f); });
    powerButton.onClick = [this] { powerAtt->setValueAsCompleteGesture (powerValue->load() > 0.5f ? 0.f : 1.f); };
    powerAtt->sendInitialUpdate();
    addAndMakeVisible (powerButton);

    bypassButton.setTooltip ("Bypass (lit = bypassed)");
    bypassAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (ids::bypass),
                                                             [this] (float v) { bypassButton.setOn (v > 0.5f); });
    bypassButton.onClick = [this] { bypassAtt->setValueAsCompleteGesture (bypassValue->load() > 0.5f ? 0.f : 1.f); };
    bypassAtt->sendInitialUpdate();
    addAndMakeVisible (bypassButton);

    // ---- presets ------------------------------------------------------------------------
    presetBar.setTooltip ("Click: preset list.  Arrows: previous / next");
    presetBar.onMenu = [this] { showPresetMenu(); };
    presetBar.onStep = [this] (int d) { stepPreset (d); };
    addAndMakeVisible (presetBar);

    saveButton.setTooltip ("Save (overwrites the current user preset)");
    saveAsButton.setTooltip ("Save as a new user preset");
    deleteButton.setTooltip ("Delete the current user preset");
    initButton.setTooltip ("Reset every control to its default");
    saveButton.onClick   = [this] { doSave(); };
    saveAsButton.onClick = [this] { doSaveAs(); };
    deleteButton.onClick = [this] { doDelete(); };
    initButton.onClick   = [this] { proc.presets.initPreset(); flash ("Init"); };
    for (auto* b : { &saveButton, &saveAsButton, &deleteButton, &initButton })
        addAndMakeVisible (*b);

    // ---- window ------------------------------------------------------------------------------
    setResizable (true, true);
    setResizeLimits (480, 320, 1920, 1280);
    getConstrainer()->setFixedAspectRatio ((double) kDesignW / (double) kDesignH);
    setSize (960, 640);

    refreshPresetBar();
    startTimerHz (30);
}

StudioFrosEditor::~StudioFrosEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void StudioFrosEditor::addKnob (const char* paramId, const juce::String& tooltip, float cx, float cy, float size)
{
    auto k = std::make_unique<Knob>();
    k->cx = cx; k->cy = cy; k->size = size;

    auto& s = k->slider;
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    juce::Slider::RotaryParameters rp;
    rp.startAngleRadians = juce::MathConstants<float>::pi * 1.25f;
    rp.endAngleRadians   = juce::MathConstants<float>::pi * 2.75f;
    rp.stopAtEnd = true;
    s.setRotaryParameters (rp);
    s.setMouseDragSensitivity (200);
    s.setPopupDisplayEnabled (true, true, this);
    s.setTooltip (tooltip);

    auto* param = proc.apvts.getParameter (paramId);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, s);
    if (param != nullptr)
        s.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    addAndMakeVisible (s);
    knobs.push_back (std::move (k));
}

void StudioFrosEditor::bindBoolSwitch (gui::LeverSwitch& sw, const char* paramId,
                                       std::unique_ptr<juce::ParameterAttachment>& att, const juce::String& tip)
{
    auto* param = proc.apvts.getParameter (paramId);
    jassert (param != nullptr);

    att = std::make_unique<juce::ParameterAttachment> (*param, [&sw] (float v) { sw.setState (v > 0.5f ? 1 : 0); });

    auto* attPtr = att.get();
    sw.onChange = [attPtr] (int newState) { attPtr->setValueAsCompleteGesture (newState > 0 ? 1.f : 0.f); };
    sw.setTooltip (tip);
    att->sendInitialUpdate();
    addAndMakeVisible (sw);
}

//==============================================================================
void StudioFrosEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    if (scaledBackground.isValid())
        g.drawImageAt (scaledBackground, 0, 0);
    else if (background.isValid())
        g.drawImage (background, getLocalBounds().toFloat());
}

void StudioFrosEditor::resized()
{
    const float s = (float) getWidth() / kDesignW;

    if (background.isValid() && getWidth() > 0 && getHeight() > 0)
        scaledBackground = background.rescaled (getWidth(), getHeight(), juce::Graphics::highResamplingQuality);

    auto box = [s] (float cx, float cy, float w, float h)
    {
        return juce::Rectangle<int> (juce::roundToInt ((cx - w * 0.5f) * s), juce::roundToInt ((cy - h * 0.5f) * s),
                                     juce::roundToInt (w * s), juce::roundToInt (h * s));
    };

    for (auto& k : knobs)
        k->slider.setBounds (box (k->cx, k->cy, k->size, k->size));

    modeKnob->setBounds (box (692, 508, 331, 331));

    // LEDs (component is 2.4x the lamp so the glow has room)
    peakLed.setBounds  (box (313, 361, 22 * 2 * 2.4f, 22 * 2 * 2.4f));
    head1Led.setBounds (box (545, 763, 26 * 2 * 2.4f, 26 * 2 * 2.4f));
    head2Led.setBounds (box (643, 763, 26 * 2 * 2.4f, 26 * 2 * 2.4f));
    head3Led.setBounds (box (742, 763, 26 * 2 * 2.4f, 26 * 2 * 2.4f));
    reverbLed.setBounds (box (842, 763, 26 * 2 * 2.4f, 26 * 2 * 2.4f));

    // switches: the artwork's two captions (FREE/HOST, OFF/ON, MONO/STEREO) now label the
    // two ends of ONE switch, which sits between them
    auto sw = [&box] (gui::LeverSwitch& c, float cx, float cy) { c.setBounds (box (cx, cy, 72.f, 84.f)); };
    sw (syncSwitch,   1099,  797);
    sw (noteSwitch,   1334,  797);
    sw (carrySwitch,   813, 1050);
    sw (stereoSwitch, 1060, 1050);

    powerButton.setBounds (box (316, 1040, 230, 230));
    bypassButton.setBounds (box (556, 1040, 230, 230));

    presetBar.setBounds (box (1445, 962, 456, 58));
    saveButton.setBounds (box (1262, 1036, 92, 50));
    saveAsButton.setBounds (box (1384, 1036, 92, 50));
    deleteButton.setBounds (box (1505, 1036, 92, 50));
    initButton.setBounds (box (1627, 1036, 92, 50));
}

//==============================================================================
void StudioFrosEditor::timerCallback()
{
    // PEAK lamp: instant attack, ~0.3 s decay
    const float pk = proc.peakMeter.exchange (0.f);
    if (pk > 0.89f) peakHold = 1.f;
    else            peakHold *= 0.88f;
    peakLed.setLevel (peakHold > 0.08f ? peakHold : 0.f);

    // head / reverb lamps follow the mode selector
    const bool active = powerValue->load() > 0.5f && bypassValue->load() < 0.5f;
    const int idx = juce::jlimit (0, kNumModes - 1, (int) std::lround (modeValue->load()));
    const auto& m = kModes[idx];
    head1Led.setLevel (active && (m.headMask & 1) ? 1.f : 0.f);
    head2Led.setLevel (active && (m.headMask & 2) ? 1.f : 0.f);
    head3Led.setLevel (active && (m.headMask & 4) ? 1.f : 0.f);
    reverbLed.setLevel (active && m.reverb ? 1.f : 0.f);

    refreshPresetBar();
}

//==============================================================================
// Presets
void StudioFrosEditor::refreshPresetBar()
{
    if (flashUntil != 0 && juce::Time::getMillisecondCounter() > flashUntil)
        flashUntil = 0;

    presetBar.setText (flashUntil != 0 ? flashText : proc.presets.getCurrentName());
}

void StudioFrosEditor::flash (const juce::String& message)
{
    flashText = message;
    flashUntil = juce::Time::getMillisecondCounter() + 1400;
    refreshPresetBar();
}

void StudioFrosEditor::showPresetMenu()
{
    auto factory = proc.presets.getFactoryNames();
    auto user = proc.presets.getUserNames();
    const auto current = proc.presets.getCurrentName();
    const bool currentFactory = proc.presets.isCurrentFactory();

    juce::PopupMenu menu;
    menu.addSectionHeader ("Factory");
    for (int i = 0; i < factory.size(); ++i)
        menu.addItem (1000 + i, factory[i], true, currentFactory && factory[i] == current);

    menu.addSeparator();
    menu.addSectionHeader ("User");
    if (user.isEmpty())
        menu.addItem (-1, "(none yet - use SAVE AS)", false, false);
    for (int i = 0; i < user.size(); ++i)
        menu.addItem (2000 + i, user[i], true, ! currentFactory && user[i] == current);

    juce::Component::SafePointer<StudioFrosEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBar),
                        [safe, factory, user] (int result)
                        {
                            if (safe == nullptr || result <= 0) return;
                            if (result >= 2000)      safe->proc.presets.loadUser (user[result - 2000]);
                            else if (result >= 1000) safe->proc.presets.loadFactory (result - 1000);
                            safe->refreshPresetBar();
                        });
}

void StudioFrosEditor::stepPreset (int direction)
{
    auto factory = proc.presets.getFactoryNames();
    auto user = proc.presets.getUserNames();
    const int nf = factory.size(), total = nf + user.size();
    if (total == 0) return;

    int index = 0;
    if (proc.presets.isCurrentFactory())
        index = juce::jmax (0, factory.indexOf (proc.presets.getCurrentName()));
    else
        index = nf + juce::jmax (0, user.indexOf (proc.presets.getCurrentName()));

    index = (index + direction + total) % total;
    if (index < nf) proc.presets.loadFactory (index);
    else            proc.presets.loadUser (user[index - nf]);
    refreshPresetBar();
}

void StudioFrosEditor::doSave()
{
    if (proc.presets.isCurrentFactory())
    {
        doSaveAs();   // factory presets cannot be overwritten
        return;
    }
    flash (proc.presets.saveUser (proc.presets.getCurrentName()) ? "Saved" : "Save failed");
}

void StudioFrosEditor::doSaveAs()
{
    auto* w = new juce::AlertWindow ("Save preset", "Preset name:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.presets.isCurrentFactory() ? juce::String() : proc.presets.getCurrentName());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<StudioFrosEditor> safe (this);
    w->enterModalState (true,
                        juce::ModalCallbackFunction::create ([safe, w] (int result)
                        {
                            if (result == 1 && safe != nullptr)
                                safe->commitSaveAs (w->getTextEditorContents ("name"));
                        }),
                        true);
}

void StudioFrosEditor::commitSaveAs (const juce::String& name)
{
    const auto cleaned = name.trim();
    if (cleaned.isEmpty()) return;

    if (proc.presets.getFactoryNames().contains (cleaned, true))
    {
        flash ("Name is reserved");
        return;
    }
    flash (proc.presets.saveUser (cleaned) ? "Saved" : "Save failed");
}

void StudioFrosEditor::doDelete()
{
    if (proc.presets.isCurrentFactory())
    {
        flash ("Factory preset");
        return;
    }

    const auto name = proc.presets.getCurrentName();
    juce::Component::SafePointer<StudioFrosEditor> safe (this);
    juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOkCancel (
                                      juce::MessageBoxIconType::QuestionIcon, "Delete preset",
                                      "Delete \"" + name + "\"?", "Delete", "Cancel", this),
                                  [safe, name] (int result)
                                  {
                                      if (result == 1 && safe != nullptr)
                                          safe->commitDelete (name);
                                  });
}

void StudioFrosEditor::commitDelete (const juce::String& name)
{
    flash (proc.presets.deleteUser (name) ? "Deleted" : "Delete failed");
}
