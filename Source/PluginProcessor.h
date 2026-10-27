// StudioFros - Tape Delay
// Copyright (C) 2026 GuitarFros
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "TapeEngine.h"
#include "PresetManager.h"

class StudioFrosProcessor : public juce::AudioProcessor
{
public:
    StudioFrosProcessor();
    ~StudioFrosProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    sf::PresetManager presets;

    /** Peak of the signal that hits the tape (after INPUT), reset by the editor. */
    std::atomic<float> peakMeter { 0.f };

private:
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int count, double bpm);

    sf::TapeEngine engine;
    sf::ToneStack tone;
    juce::Reverb reverb;

    std::vector<float> feedL, feedR, echoL, echoR, revL, revR;

    using Smoothed = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;
    Smoothed inGainSm, feedEnableSm, revFeedSm, wetSm, dryGainSm, outGainSm, echoVolSm, revVolSm;

    bool needsClear = false;
    double currentSampleRate = 44100.0;

    std::atomic<float>* pInput = nullptr;
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pRepeatRate = nullptr;
    std::atomic<float>* pIntensity = nullptr;
    std::atomic<float>* pWow = nullptr;
    std::atomic<float>* pSat = nullptr;
    std::atomic<float>* pAge = nullptr;
    std::atomic<float>* pSync = nullptr;
    std::atomic<float>* pNote = nullptr;
    std::atomic<float>* pBass = nullptr;
    std::atomic<float>* pTreble = nullptr;
    std::atomic<float>* pEchoVol = nullptr;
    std::atomic<float>* pReverbVol = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pCarry = nullptr;
    std::atomic<float>* pStereo = nullptr;
    std::atomic<float>* pPower = nullptr;
    std::atomic<float>* pBypass = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StudioFrosProcessor)
};
