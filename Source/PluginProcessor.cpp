// StudioFros - Tape Delay
// Copyright (C) 2026 GuitarFros
// SPDX-License-Identifier: GPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace sf;

StudioFrosProcessor::StudioFrosProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout()),
      presets (apvts)
{
    pInput      = apvts.getRawParameterValue (ids::input);
    pMode       = apvts.getRawParameterValue (ids::mode);
    pRepeatRate = apvts.getRawParameterValue (ids::repeatRate);
    pIntensity  = apvts.getRawParameterValue (ids::intensity);
    pWow        = apvts.getRawParameterValue (ids::wowFlutter);
    pSat        = apvts.getRawParameterValue (ids::saturation);
    pAge        = apvts.getRawParameterValue (ids::tapeAge);
    pSync       = apvts.getRawParameterValue (ids::syncHost);
    pNote       = apvts.getRawParameterValue (ids::note);
    pBass       = apvts.getRawParameterValue (ids::bass);
    pTreble     = apvts.getRawParameterValue (ids::treble);
    pEchoVol    = apvts.getRawParameterValue (ids::echoVol);
    pReverbVol  = apvts.getRawParameterValue (ids::reverbVol);
    pOutput     = apvts.getRawParameterValue (ids::output);
    pCarry      = apvts.getRawParameterValue (ids::carryover);
    pStereo     = apvts.getRawParameterValue (ids::stereo);
    pPower      = apvts.getRawParameterValue (ids::power);
    pBypass     = apvts.getRawParameterValue (ids::bypass);
}

bool StudioFrosProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void StudioFrosProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate);
    tone.prepare (sampleRate);

    reverb.setSampleRate (sampleRate);
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.55f;
    rp.damping = 0.55f;
    rp.wetLevel = 0.33f;     // internally x3 -> unity
    rp.dryLevel = 0.0f;
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);
    reverb.reset();

    const auto chunk = (size_t) juce::jmax (samplesPerBlock, 512);
    for (auto* v : { &feedL, &feedR, &echoL, &echoR, &revL, &revR })
        v->assign (chunk, 0.f);

    auto setup = [sampleRate] (Smoothed& s, double seconds, float value)
    {
        s.reset (sampleRate, seconds);
        s.setCurrentAndTargetValue (value);
    };
    setup (inGainSm,     0.02, juce::Decibels::decibelsToGain (pInput->load()));
    setup (feedEnableSm, 0.01, 1.f);
    setup (revFeedSm,    0.01, 1.f);
    setup (wetSm,        0.03, 1.f);
    setup (dryGainSm,    0.02, 1.f);
    setup (outGainSm,    0.02, 1.f);
    setup (echoVolSm,    0.02, 0.f);
    setup (revVolSm,     0.02, 0.f);
    needsClear = false;
}

void StudioFrosProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
    {
        auto pos = ph->getPosition();
        if (pos.hasValue())
        {
            auto b = pos->getBpm();
            if (b.hasValue() && *b > 10.0) bpm = *b;
        }
    }

    const int total = buffer.getNumSamples();
    const int chunkSize = (int) feedL.size();
    for (int start = 0; start < total; start += chunkSize)
        processChunk (buffer, start, juce::jmin (chunkSize, total - start), bpm);
}

void StudioFrosProcessor::processChunk (juce::AudioBuffer<float>& buffer, int start, int m, double bpm)
{
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (numCh == 0 || m <= 0) return;

    float* chL = buffer.getWritePointer (0) + start;
    float* chR = numCh > 1 ? buffer.getWritePointer (1) + start : nullptr;
    const float* inL = chL;
    const float* inR = chR != nullptr ? chR : chL;

    // ---- parameters
    const bool power   = pPower->load()  > 0.5f;
    const bool bypass  = pBypass->load() > 0.5f;
    const bool carry   = pCarry->load()  > 0.5f;
    const bool stereo  = pStereo->load() > 0.5f && numCh == 2;
    const bool hostSync = pSync->load()  > 0.5f;
    const int modeIdx  = juce::jlimit (0, kNumModes - 1, (int) std::lround (pMode->load()));
    const auto& mode   = kModes[modeIdx];

    const bool active  = power && ! bypass;

    EchoParams ep;
    ep.headMask   = mode.headMask;
    ep.stereo     = stereo;
    ep.feedback   = pIntensity->load() * 1.12f;
    ep.wow        = pWow->load();
    ep.saturation = pSat->load();
    ep.age        = pAge->load();

    double baseSeconds;
    if (hostSync)
    {
        const int note = juce::jlimit (0, 2, (int) std::lround (pNote->load()));
        const double beat = 60.0 / bpm;
        baseSeconds = note == 0 ? beat : (note == 1 ? beat * 0.5 : beat * 0.75);
    }
    else
    {
        baseSeconds = (double) repeatRateToMs (pRepeatRate->load()) * 0.001;
    }
    ep.baseDelaySamples = (float) (baseSeconds * currentSampleRate);

    tone.set (pBass->load(), pTreble->load());

    const float outG = juce::Decibels::decibelsToGain (pOutput->load());
    inGainSm.setTargetValue (juce::Decibels::decibelsToGain (pInput->load()));
    outGainSm.setTargetValue (outG);
    echoVolSm.setTargetValue (pEchoVol->load() * 1.3f);
    revVolSm.setTargetValue (pReverbVol->load() * 1.2f);

    const bool wetOn = power && (! bypass || carry);
    wetSm.setTargetValue (wetOn ? 1.f : 0.f);
    feedEnableSm.setTargetValue (active ? 1.f : 0.f);
    revFeedSm.setTargetValue (active && mode.reverb ? 1.f : 0.f);
    dryGainSm.setTargetValue (active ? outG : 1.f);

    // ---- feed the tape
    float peak = 0.f;
    float* fL = feedL.data();
    float* fR = feedR.data();
    for (int i = 0; i < m; ++i)
    {
        const float g  = inGainSm.getNextValue();
        const float fe = feedEnableSm.getNextValue();
        const float l = inL[i], r = inR[i];
        peak = juce::jmax (peak, juce::jmax (std::abs (l), std::abs (r)) * g);
        fL[i] = l * g * fe;
        fR[i] = r * g * fe;
    }
    peakMeter.store (juce::jmax (peakMeter.load(), peak));

    engine.process (fL, fR, echoL.data(), echoR.data(), m, ep);

    // ---- reverb (always running so tails decay naturally)
    float* rL = revL.data();
    float* rR = revR.data();
    for (int i = 0; i < m; ++i)
    {
        const float rf = revFeedSm.getNextValue();
        rL[i] = (fL[i] + echoL[(size_t) i] * 0.6f) * rf;
        rR[i] = (fR[i] + echoR[(size_t) i] * 0.6f) * rf;
    }
    reverb.processStereo (rL, rR, m);

    // ---- wet = echo + reverb, then bass/treble
    float* eL = echoL.data();
    float* eR = echoR.data();
    for (int i = 0; i < m; ++i)
    {
        const float ev = echoVolSm.getNextValue();
        const float rv = revVolSm.getNextValue();
        eL[i] = eL[i] * ev + rL[i] * rv;
        eR[i] = eR[i] * ev + rR[i] * rv;
    }
    tone.process (eL, eR, m);

    // ---- mix
    for (int i = 0; i < m; ++i)
    {
        const float wet = wetSm.getNextValue() * outGainSm.getNextValue();
        const float dry = dryGainSm.getNextValue();

        if (chR != nullptr)
        {
            chL[i] = inL[i] * dry + eL[i] * wet;
            chR[i] = inR[i] * dry + eR[i] * wet;
        }
        else
        {
            chL[i] = inL[i] * dry + 0.5f * (eL[i] + eR[i]) * wet;
        }
    }

    // ---- when the wet path has faded out completely, wipe the tape & reverb
    if (! wetOn && wetSm.getCurrentValue() < 1.0e-4f)
    {
        if (! needsClear)
        {
            engine.reset();
            reverb.reset();
            needsClear = true;
        }
    }
    else
    {
        needsClear = false;
    }
}

juce::AudioProcessorEditor* StudioFrosProcessor::createEditor()
{
    return new StudioFrosEditor (*this);
}

void StudioFrosProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presets.getCurrentName(), nullptr);
    state.setProperty ("presetFactory", presets.isCurrentFactory(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void StudioFrosProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (vt);
            presets.setCurrent (vt.getProperty ("presetName", "Init").toString(),
                                (bool) vt.getProperty ("presetFactory", true));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StudioFrosProcessor();
}
