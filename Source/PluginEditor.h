#pragma once

#include "PluginProcessor.h"
#include "SilverLookAndFeel.h"
#include "SpectrumView.h"

//==============================================================================
// Medidor do sinal Source com o slider de Threshold acoplado
//==============================================================================
class ThresholdMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    ThresholdMeter (SpectralGateAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void update();

private:
    juce::Rectangle<float> barArea() const;
    float dbToY (float db) const;
    float yToDb (float y) const;

    SpectralGateAudioProcessor& proc;
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    float threshold = -40.0f;
    float level = -120.0f, peakHold = -120.0f;
    int peakHoldFrames = 0;
    float dragStartDb = 0.0f, dragStartY = 0.0f;

    static constexpr float kMin = -80.0f, kMax = 0.0f;
};

//==============================================================================
// Medidor de reducao de ganho
//==============================================================================
class ReductionMeter : public juce::Component
{
public:
    explicit ReductionMeter (SpectralGateAudioProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void update();

private:
    SpectralGateAudioProcessor& proc;
    float gr = 0.0f;
    static constexpr float kRange = 60.0f;
};

//==============================================================================
class SpectralGateAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit SpectralGateAudioProcessorEditor (SpectralGateAudioProcessor&);
    ~SpectralGateAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupKnob (juce::Slider& s, juce::Label& l, const juce::String& name, const juce::String& paramID);
    void showSizeMenu();
    void syncLearnButtons();

    SpectralGateAudioProcessor& proc;
    SilverLookAndFeel lnf;

    SpectrumView spectrum;
    ThresholdMeter threshMeter;
    ReductionMeter grMeter;

    juce::TextButton learnSourceBtn { "LEARN SOURCE" }, learnSpillBtn { "LEARN SPILL" },
                     clearBtn { "CLEAR" }, bypassBtn { "BYPASS" }, sizeBtn { "SIZE" },
                     sidechainBtn { "EXT SIDECHAIN" };

    juce::Slider attackKnob, holdKnob, releaseKnob, depthKnob, hystKnob, mixKnob, outputKnob;
    juce::Label  attackLbl, holdLbl, releaseLbl, depthLbl, hystLbl, mixLbl, outputLbl;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SliderAtt>> sliderAtts;
    std::unique_ptr<ButtonAtt> sidechainAtt;

    juce::TooltipWindow tooltips { this, 600 };

    static constexpr int kBaseW = 1000, kBaseH = 620;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralGateAudioProcessorEditor)
};
