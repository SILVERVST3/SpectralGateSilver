#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Paleta SILVER (mesma familia visual dos outros plugins SILVER)
//==============================================================================
namespace silver
{
    const juce::Colour bgTop      { 0xffdfe3e8 };
    const juce::Colour bgBottom   { 0xffb9c0c8 };
    const juce::Colour panel      { 0xffeceff2 };
    const juce::Colour panelEdge  { 0xff9aa3ad };
    const juce::Colour navy       { 0xff1b2a4a };
    const juce::Colour navySoft   { 0xff3c4f73 };
    const juce::Colour display    { 0xfff4f6f8 };
    const juce::Colour grid       { 0x221b2a4a };
    const juce::Colour source     { 0xff23a456 };
    const juce::Colour spill      { 0xffd23f3f };
    const juce::Colour spectrum   { 0xff8e9aab };
    const juce::Colour amber      { 0xffe0a020 };
}

//==============================================================================
class SilverLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SilverLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId,       silver::navy);
        setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId,  silver::navySoft.withAlpha (0.3f));
        setColour (juce::Label::textColourId,               silver::navy);
        setColour (juce::TextButton::textColourOffId,       silver::navy);
        setColour (juce::TextButton::textColourOnId,        juce::Colours::white);
        setColour (juce::PopupMenu::backgroundColourId,     silver::panel);
        setColour (juce::PopupMenu::textColourId,           silver::navy);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, silver::navySoft);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TooltipWindow::backgroundColourId, silver::panel);
        setColour (juce::TooltipWindow::textColourId,       silver::navy);
        setColour (juce::TooltipWindow::outlineColourId,    silver::panelEdge);
    }

    juce::Font getLabelFont (juce::Label&) override
    {
        return juce::Font (juce::FontOptions (13.0f * fontScale, juce::Font::bold));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (14.0f * fontScale, (float) buttonHeight * 0.45f), juce::Font::bold));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
        const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto r = bounds.withSizeKeepingCentre (size, size);
        const auto c = r.getCentre();
        const float radius = size * 0.5f;
        const float angle = startAngle + sliderPos * (endAngle - startAngle);

        // trilha do arco
        const float arcR = radius - 2.0f;
        const float arcW = juce::jmax (2.0f, radius * 0.11f);
        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (silver::panelEdge.withAlpha (0.45f));
        g.strokePath (track, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (silver::navy);
        g.strokePath (value, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // corpo metalico
        const float bodyR = radius * 0.72f;
        auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillEllipse (body.translated (0.0f, 2.0f));
        juce::ColourGradient grad (juce::Colour (0xfffbfcfd), c.x, body.getY(),
                                   juce::Colour (0xffa9b1bb), c.x, body.getBottom(), false);
        g.setGradientFill (grad);
        g.fillEllipse (body);
        g.setColour (silver::panelEdge);
        g.drawEllipse (body, 1.0f);

        // ponteiro
        juce::Path p;
        const float pw = juce::jmax (2.0f, bodyR * 0.12f);
        p.addRoundedRectangle (-pw * 0.5f, -bodyR * 0.92f, pw, bodyR * 0.5f, pw * 0.5f);
        g.setColour (silver::navy);
        g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& backgroundColour,
                               bool isOver, bool isDown) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const float corner = r.getHeight() * 0.22f;
        const bool on = b.getToggleState() || isDown;

        juce::Colour top, bottom;
        if (on)
        {
            top = backgroundColour.brighter (0.15f);
            bottom = backgroundColour.darker (0.2f);
        }
        else
        {
            top = juce::Colour (0xfff7f8fa);
            bottom = juce::Colour (0xffc4cad2);
        }
        if (isOver && ! on) { top = top.brighter (0.05f); bottom = bottom.brighter (0.08f); }

        g.setGradientFill (juce::ColourGradient (top, 0.0f, r.getY(), bottom, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, corner);
        g.setColour (on ? backgroundColour.darker (0.4f) : silver::panelEdge);
        g.drawRoundedRectangle (r, corner, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool isDown) override
    {
        const bool on = b.getToggleState() || isDown;
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (on ? juce::Colours::white : silver::navy);
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 1);
    }

    float fontScale = 1.0f;
};
