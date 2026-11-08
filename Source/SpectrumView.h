#pragma once

#include "PluginProcessor.h"
#include "SilverLookAndFeel.h"

//==============================================================================
// Tela do espectro: analise ao vivo, linha Source (verde) e Spill (vermelha)
// com 24 nos arrastaveis cada. A energia entre as linhas fica em verde.
//==============================================================================
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (SpectralGateAudioProcessor& p);

    void paint (juce::Graphics&) override;
    void refresh();                              // chamado pelo timer do editor

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    struct NodeRef { sg::Line line = sg::Line::source; int index = -1; };

    juce::Rectangle<float> plotArea() const;
    float freqToX (float f) const;
    float dbToY (float db) const;
    float yToDb (float y) const;
    NodeRef hitTestNode (juce::Point<float> pos) const;

    SpectralGateAudioProcessor& proc;
    std::array<float, sg::kDisplayBins> spec {};
    std::array<float, sg::kNumNodes> srcNodes {}, splNodes {};

    NodeRef hover, dragging;
    float dragStartDb = 0.0f, dragStartY = 0.0f;

    static constexpr float kViewMinDb = -100.0f;
    static constexpr float kViewMaxDb = 6.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumView)
};
