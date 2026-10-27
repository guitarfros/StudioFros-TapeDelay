// StudioFros - Tape Delay
// Copyright (C) 2026 GuitarFros
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Custom controls drawn on top of the rack artwork (the artwork itself has
// the panel, labels and screws but no knobs, LEDs or switches).
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "Params.h"
#include "BinaryData.h"

namespace sf::gui
{
using namespace juce;

constexpr float twoPi = MathConstants<float>::twoPi;

//==============================================================================
/** Black Bakelite-style knob with knurled edge and a white pointer. */
inline void drawKnobFace (Graphics& g, Point<float> c, float r, float pointerAngle)
{
    {
        Path shadowPath;
        shadowPath.addEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        DropShadow (Colours::black.withAlpha (0.65f), (int) (r * 0.45f),
                    { (int) (r * 0.06f), (int) (r * 0.16f) }).drawForPath (g, shadowPath);
    }

    ColourGradient outer (Colour (0xff3b3b3b), c.x - r * 0.7f, c.y - r * 0.8f,
                          Colour (0xff050505), c.x + r * 0.7f, c.y + r * 0.9f, false);
    g.setGradientFill (outer);
    g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);

    // knurling
    g.setColour (Colours::white.withAlpha (0.16f));
    constexpr int teeth = 64;
    for (int i = 0; i < teeth; ++i)
    {
        const float a = (float) i * twoPi / (float) teeth;
        g.drawLine (Line<float> (c.getPointOnCircumference (r * 0.90f, a),
                                 c.getPointOnCircumference (r * 0.985f, a)), 1.0f);
    }

    // cap
    const float rc = r * 0.80f;
    ColourGradient cap (Colour (0xff363636), c.x - rc * 0.5f, c.y - rc * 0.6f,
                        Colour (0xff0b0b0b), c.x + rc * 0.6f, c.y + rc * 0.8f, false);
    g.setGradientFill (cap);
    g.fillEllipse (c.x - rc, c.y - rc, 2 * rc, 2 * rc);
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawEllipse (c.x - rc, c.y - rc, 2 * rc, 2 * rc, 1.0f);

    // pointer
    Path pointer;
    pointer.startNewSubPath (c.getPointOnCircumference (r * 0.20f, pointerAngle));
    pointer.lineTo (c.getPointOnCircumference (r * 0.93f, pointerAngle));
    g.setColour (Colour (0xfff2ede2));
    g.strokePath (pointer, PathStrokeType (jmax (2.0f, r * 0.09f), PathStrokeType::curved, PathStrokeType::rounded));
}

/** Round indicator lamp with glow.  level 0..1 */
inline void drawLamp (Graphics& g, Point<float> c, float radius, Colour colour, float level)
{
    level = jlimit (0.f, 1.f, level);

    if (level > 0.02f)
    {
        ColourGradient glow (colour.withAlpha (0.50f * level), c.x, c.y,
                             colour.withAlpha (0.0f), c.x + radius * 2.3f, c.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (c.x - radius * 2.3f, c.y - radius * 2.3f, radius * 4.6f, radius * 4.6f);
    }

    g.setColour (Colour (0xff0a0a0a));
    g.fillEllipse (c.x - radius, c.y - radius, 2 * radius, 2 * radius);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawEllipse (c.x - radius, c.y - radius, 2 * radius, 2 * radius, 1.0f);

    const float lr = radius * 0.78f;
    const Colour dim = colour.withMultipliedBrightness (0.20f).withMultipliedSaturation (0.8f);
    const Colour lit = colour.brighter (0.25f);
    const Colour mix = dim.interpolatedWith (lit, level);

    ColourGradient lens (mix.brighter (0.6f * level), c.x - lr * 0.2f, c.y - lr * 0.25f,
                         mix.darker (0.5f), c.x - lr * 0.2f + lr * 1.25f, c.y - lr * 0.25f, true);
    g.setGradientFill (lens);
    g.fillEllipse (c.x - lr, c.y - lr, 2 * lr, 2 * lr);

    g.setColour (Colours::white.withAlpha (0.12f + 0.40f * level));
    g.fillEllipse (c.x - lr * 0.55f, c.y - lr * 0.72f, lr * 0.55f, lr * 0.32f);
}

//==============================================================================
class TapeLookAndFeel : public LookAndFeel_V4
{
public:
    TapeLookAndFeel()
    {
        setColour (TooltipWindow::backgroundColourId, Colour (0xff141414));
        setColour (TooltipWindow::textColourId, Colour (0xfff0e6d2));
        setColour (TooltipWindow::outlineColourId, Colour (0xff5a4b36));
        setColour (PopupMenu::backgroundColourId, Colour (0xff161412));
        setColour (PopupMenu::textColourId, Colour (0xfff0e6d2));
        setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff6b4a1f));
        setColour (PopupMenu::headerTextColourId, Colour (0xffd89b3c));
    }

    void drawRotarySlider (Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, Slider&) override
    {
        const auto bounds = Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        const auto c = bounds.getCentre();
        const float size = jmin (bounds.getWidth(), bounds.getHeight());
        const float r = size * 0.33f;

        // tick marks around the knob
        g.setColour (Colour (0xffcfc6b5).withAlpha (0.70f));
        constexpr int numTicks = 11;
        for (int i = 0; i < numTicks; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (numTicks - 1);
            g.drawLine (Line<float> (c.getPointOnCircumference (r * 1.20f, a),
                                     c.getPointOnCircumference (r * 1.38f, a)),
                        jmax (1.0f, size * 0.012f));
        }

        drawKnobFace (g, c, r, startAngle + pos * (endAngle - startAngle));
    }
};

//==============================================================================
class Led : public Component
{
public:
    explicit Led (Colour c) : colour (c) { setInterceptsMouseClicks (false, false); }

    void setLevel (float newLevel)
    {
        newLevel = jlimit (0.f, 1.f, newLevel);
        if (std::abs (newLevel - level) > 0.01f) { level = newLevel; repaint(); }
    }

    void paint (Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        drawLamp (g, b.getCentre(), jmin (b.getWidth(), b.getHeight()) * 0.5f / 2.4f, colour, level);
    }

private:
    Colour colour;
    float level = 0.f;
};

//==============================================================================
/** Illuminated push button (POWER / BYPASS).  It is built on the same weathered plate with
    four screws and the same chrome nut as the toggle switches (Assets/button_plate.png),
    with a coloured glass lens fitted in the middle, so it matches the rest of the panel. */
class LampButton : public Component, public SettableTooltipClient
{
public:
    explicit LampButton (Colour c) : colour (c)
    {
        setMouseCursor (MouseCursor::PointingHandCursor);
        plate = ImageCache::getFromMemory (BinaryData::button_plate_png, BinaryData::button_plate_pngSize);
    }

    std::function<void()> onClick;

    void setOn (bool shouldBeOn) { if (on != shouldBeOn) { on = shouldBeOn; repaint(); } }

    void paint (Graphics& g) override
    {
        if (! plate.isValid()) return;

        // the plate image is 204 x 176 px and its nut centre is at (101, 88)
        const auto bounds = getLocalBounds().toFloat();
        const float plateW = bounds.getWidth() * 0.87f;
        const float k = plateW / 204.f;
        const Rectangle<float> plateRect (bounds.getCentreX() - plateW * 0.5f,
                                          bounds.getCentreY() - 88.f * k, plateW, 176.f * k);
        const Point<float> c (plateRect.getX() + 101.f * k, plateRect.getY() + 88.f * k);

        DropShadow (Colours::black.withAlpha (0.7f), (int) (10.f * k), { 0, (int) (4.f * k) })
            .drawForRectangle (g, plateRect.reduced (plateRect.getWidth() * 0.07f).toNearestInt());

        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (plate, plateRect);

        // light spilling over the plate
        if (on)
        {
            ColourGradient spill (colour.withAlpha (0.38f), c.x, c.y, colour.withAlpha (0.f), c.x + 92.f * k, c.y, true);
            g.setGradientFill (spill);
            g.fillEllipse (c.x - 92.f * k, c.y - 92.f * k, 184.f * k, 184.f * k);
        }

        // lens seat, then the glass
        const float seat = 26.5f * k;
        g.setColour (Colour (0xff070707));
        g.fillEllipse (c.x - seat, c.y - seat, 2 * seat, 2 * seat);

        const float lr = (pressed ? 22.f : 24.f) * k;
        const Colour dim = colour.withMultipliedBrightness (0.20f).withMultipliedSaturation (0.85f);
        const Colour lit = colour.brighter (0.2f);
        const Colour glass = pressed ? (on ? lit.darker (0.2f) : dim.darker (0.3f)) : (on ? lit : dim);

        ColourGradient lens (glass.brighter (on ? 0.8f : 0.3f), c.x - lr * 0.15f, c.y - lr * 0.2f,
                             glass.darker (0.6f), c.x - lr * 0.15f + lr * 1.3f, c.y - lr * 0.2f, true);
        g.setGradientFill (lens);
        g.fillEllipse (c.x - lr, c.y - lr, 2 * lr, 2 * lr);

        // fresnel rings moulded into the glass
        g.setColour (Colours::black.withAlpha (on ? 0.16f : 0.30f));
        for (float f : { 0.38f, 0.62f, 0.84f })
            g.drawEllipse (c.x - lr * f, c.y - lr * f, 2 * lr * f, 2 * lr * f, jmax (1.0f, k * 0.9f));

        // small, soft reflection
        Path arc;
        arc.addCentredArc (c.x, c.y, lr * 0.78f, lr * 0.78f, 0.f, degreesToRadians (-75.f), degreesToRadians (-20.f), true);
        g.setColour (Colours::white.withAlpha (on ? 0.38f : 0.16f));
        g.strokePath (arc, PathStrokeType (jmax (1.5f, k * 2.2f), PathStrokeType::curved, PathStrokeType::rounded));
    }

    void mouseDown (const MouseEvent&) override { pressed = true; repaint(); }
    void mouseUp (const MouseEvent& e) override
    {
        pressed = false; repaint();
        if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
    }

private:
    Colour colour;
    Image plate;
    bool on = false, pressed = false;
};

//==============================================================================
/** Toggle switch drawn from real photographic assets (Assets/switch_*.png).
    2 states: 0 = lever left, 1 = lever right.   3 states: left / centre / right. */
class LeverSwitch : public Component, public SettableTooltipClient
{
public:
    explicit LeverSwitch (int numStatesToUse) : numStates (numStatesToUse)
    {
        setMouseCursor (MouseCursor::PointingHandCursor);
        imgLeft  = ImageCache::getFromMemory (BinaryData::switch_left_png,  BinaryData::switch_left_pngSize);
        imgMid   = ImageCache::getFromMemory (BinaryData::switch_mid_png,   BinaryData::switch_mid_pngSize);
        imgRight = ImageCache::getFromMemory (BinaryData::switch_right_png, BinaryData::switch_right_pngSize);
    }

    /** Called with the state the user asked for; the owner writes it to the parameter. */
    std::function<void (int)> onChange;

    void setState (int s) { s = jlimit (0, numStates - 1, s); if (s != state) { state = s; repaint(); } }
    int getState() const noexcept { return state; }

    void paint (Graphics& g) override
    {
        const Image& img = (state == 0) ? imgLeft
                         : (state == numStates - 1) ? imgRight
                                                    : imgMid;
        if (! img.isValid()) return;

        const auto area = RectanglePlacement (RectanglePlacement::centred)
                              .appliedTo (Rectangle<float> (0.f, 0.f, (float) img.getWidth(), (float) img.getHeight()),
                                          getLocalBounds().toFloat().reduced (3.f));

        DropShadow (Colours::black.withAlpha (0.65f), 9, { 0, 3 })
            .drawForRectangle (g, area.reduced (area.getWidth() * 0.06f).toNearestInt());

        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (img, area);
    }

    void mouseUp (const MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition()) || ! onChange) return;

        int next;
        if (numStates == 2)
            next = 1 - state;                                   // flip the switch
        else
        {
            const float x = e.position.x / (float) getWidth();  // pick left / centre / right
            next = x < 0.34f ? 0 : (x > 0.66f ? numStates - 1 : 1);
        }
        onChange (next);
    }

private:
    int numStates, state = 0;
    Image imgLeft, imgMid, imgRight;
};

//==============================================================================
class PanelButton : public Component, public SettableTooltipClient
{
public:
    PanelButton() { setMouseCursor (MouseCursor::PointingHandCursor); }
    std::function<void()> onClick;

    void paint (Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.5f);
        const float corner = b.getHeight() * 0.14f;

        g.setColour (Colours::black.withAlpha (0.65f));
        g.fillRoundedRectangle (b.translated (0.f, pressed ? 1.f : 3.f), corner);

        ColourGradient fill (pressed ? Colour (0xff1a1a1a) : Colour (0xff3a3a3a), b.getX(), b.getY(),
                             Colour (0xff0d0d0d), b.getX(), b.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (b.translated (0.f, pressed ? 2.f : 0.f), corner);

        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (b.translated (0.f, pressed ? 2.f : 0.f), corner, 1.2f);
    }

    void mouseDown (const MouseEvent&) override { pressed = true; repaint(); }
    void mouseUp (const MouseEvent& e) override
    {
        pressed = false; repaint();
        if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
    }

private:
    bool pressed = false;
};

//==============================================================================
/** The 15-position MODE SELECTOR.  Follow the mouse around the dial, or use the wheel. */
class ModeKnob : public Component, public SettableTooltipClient
{
public:
    explicit ModeKnob (RangedAudioParameter& param)
        : attachment (param, [this] (float v)
                      {
                          index = jlimit (0, kNumModes - 1, (int) std::lround (v));
                          repaint();
                      })
    {
        attachment.sendInitialUpdate();
    }

    void paint (Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const auto c = b.getCentre();
        const float size = jmin (b.getWidth(), b.getHeight());
        const float r = size * 0.37f;

        for (int i = 0; i < kNumModes; ++i)
        {
            const float a = degreesToRadians (kModes[i].angleDeg);
            const bool sel = (i == index);
            g.setColour (sel ? Colour (0xffffe2a8) : Colour (0xffcfc6b5).withAlpha (0.65f));
            g.drawLine (Line<float> (c.getPointOnCircumference (r * 1.05f, a),
                                     c.getPointOnCircumference (r * (sel ? 1.26f : 1.20f), a)),
                        jmax (1.5f, size * (sel ? 0.012f : 0.008f)));
        }

        drawKnobFace (g, c, r, degreesToRadians (kModes[index].angleDeg));
    }

    void mouseDown (const MouseEvent& e) override
    {
        attachment.beginGesture();
        gestureOpen = true;
        setFrom (e.position);
    }
    void mouseDrag (const MouseEvent& e) override { setFrom (e.position); }
    void mouseUp (const MouseEvent&) override
    {
        if (gestureOpen) { attachment.endGesture(); gestureOpen = false; }
    }

    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override
    {
        if (w.deltaY == 0.f) return;
        const int next = jlimit (0, kNumModes - 1, index + (w.deltaY > 0.f ? 1 : -1));
        if (next != index) attachment.setValueAsCompleteGesture ((float) next);
    }

    void mouseDoubleClick (const MouseEvent&) override
    {
        attachment.setValueAsCompleteGesture ((float) kDefaultMode);
    }

private:
    void setFrom (Point<float> pos)
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        const float ang = radiansToDegrees (std::atan2 (pos.x - c.x, -(pos.y - c.y)));   // 0 = up, clockwise
        int best = 0; float bestD = 1.0e9f;
        for (int i = 0; i < kNumModes; ++i)
        {
            float d = std::abs (ang - kModes[i].angleDeg);
            if (d > 180.f) d = 360.f - d;
            if (d < bestD) { bestD = d; best = i; }
        }
        if (best != index) attachment.setValueAsPartOfGesture ((float) best);
    }

    int index = kDefaultMode;
    bool gestureOpen = false;
    ParameterAttachment attachment;
};

//==============================================================================
/** Little amber "display" that shows the current preset.  Click = list, arrows = prev / next. */
class PresetBar : public Component, public SettableTooltipClient
{
public:
    PresetBar() { setMouseCursor (MouseCursor::PointingHandCursor); }

    std::function<void()> onMenu;
    std::function<void (int)> onStep;   // -1 / +1

    void setText (const String& t)
    {
        if (t != text) { text = t; repaint(); }
    }

    void paint (Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.5f);
        const float corner = b.getHeight() * 0.18f;

        g.setColour (Colour (0xff050403));
        g.fillRoundedRectangle (b, corner);
        g.setColour (Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (b, corner, 1.3f);

        const Colour amber (0xffe8a23a);
        const float h = b.getHeight();
        g.setFont (h * 0.50f);

        g.setColour (amber.withAlpha (0.55f));
        g.drawText (juce::String::charToString (0x25C0), getLocalBounds().removeFromLeft ((int) (getWidth() * 0.12f)),
                    Justification::centred);
        g.drawText (juce::String::charToString (0x25B6), getLocalBounds().removeFromRight ((int) (getWidth() * 0.12f)),
                    Justification::centred);

        g.setColour (amber);
        g.drawText (text.toUpperCase(), getLocalBounds().reduced ((int) (getWidth() * 0.13f), 0),
                    Justification::centred, true);
    }

    void mouseUp (const MouseEvent& e) override
    {
        const float x = e.position.x / (float) getWidth();
        if (x < 0.12f)       { if (onStep) onStep (-1); }
        else if (x > 0.88f)  { if (onStep) onStep (+1); }
        else                 { if (onMenu) onMenu(); }
    }

private:
    String text;
};

} // namespace sf::gui
