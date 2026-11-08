#include "PluginEditor.h"

//==============================================================================
// ThresholdMeter
//==============================================================================
ThresholdMeter::ThresholdMeter (SpectralGateAudioProcessor& p)
    : proc (p),
      param (*p.apvts.getParameter ("threshold")),
      attachment (param, [this] (float v) { threshold = v; repaint(); }, nullptr)
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTooltip ("Threshold: arraste para ajustar (Shift = fino, duplo clique = -40 dB). "
                "O medidor mostra apenas o sinal Source (area verde).");
}

juce::Rectangle<float> ThresholdMeter::barArea() const
{
    const float s = getHeight() / 384.0f;
    auto r = getLocalBounds().toFloat();
    r = r.withTrimmedTop (24.0f * s).withTrimmedBottom (24.0f * s);
    return r.withWidth (r.getWidth() * 0.42f).withX (r.getX() + r.getWidth() * 0.12f);
}

float ThresholdMeter::dbToY (float db) const
{
    auto a = barArea();
    const float t = (juce::jlimit (kMin, kMax, db) - kMin) / (kMax - kMin);
    return a.getBottom() - t * a.getHeight();
}

float ThresholdMeter::yToDb (float y) const
{
    auto a = barArea();
    return kMin + (a.getBottom() - y) / a.getHeight() * (kMax - kMin);
}

void ThresholdMeter::update()
{
    const float d = proc.getDetectorDb();
    level = juce::jmax (d, level - 1.6f);
    if (d >= peakHold) { peakHold = d; peakHoldFrames = 45; }
    else if (--peakHoldFrames <= 0) peakHold = juce::jmax (d, peakHold - 1.0f);
    repaint();
}

void ThresholdMeter::paint (juce::Graphics& g)
{
    const float s = getHeight() / 384.0f;
    auto bounds = getLocalBounds().toFloat();
    g.setColour (silver::display);
    g.fillRoundedRectangle (bounds, 6.0f * s);
    g.setColour (silver::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f * s, 1.0f);

    g.setColour (silver::navy);
    g.setFont (juce::Font (juce::FontOptions (11.0f * s, juce::Font::bold)));
    g.drawText ("THRESH", bounds.removeFromTop (22.0f * s), juce::Justification::centred);

    const auto bar = barArea();
    g.setColour (juce::Colour (0xffd5dae0));
    g.fillRoundedRectangle (bar, 3.0f * s);

    const bool above = level >= threshold;
    if (level > kMin)
    {
        auto fill = bar.withTop (dbToY (level));
        g.setColour (above ? silver::source : silver::spill);
        g.fillRoundedRectangle (fill, 3.0f * s);
    }
    if (peakHold > kMin)
    {
        g.setColour (silver::navySoft);
        g.fillRect (bar.withTop (dbToY (peakHold)).withHeight (2.0f * s));
    }

    // escala
    g.setFont (juce::Font (juce::FontOptions (9.5f * s)));
    for (int db = 0; db >= -80; db -= 10)
    {
        const float y = dbToY ((float) db);
        g.setColour (silver::navySoft);
        g.drawText (juce::String (db), juce::Rectangle<float> (bar.getRight() + 3.0f * s, y - 6.0f * s, 26.0f * s, 12.0f * s),
                    juce::Justification::centredLeft);
    }

    // marcador do threshold
    const float ty = dbToY (threshold);
    g.setColour (silver::navy);
    g.fillRect (juce::Rectangle<float> (bar.getX() - 4.0f * s, ty - 1.0f * s, bar.getWidth() + 8.0f * s, 2.0f * s));
    juce::Path tri;
    const float tx = bar.getX() - 4.0f * s;
    tri.addTriangle (tx - 7.0f * s, ty - 6.0f * s, tx - 7.0f * s, ty + 6.0f * s, tx, ty);
    g.fillPath (tri);

    g.setFont (juce::Font (juce::FontOptions (11.0f * s, juce::Font::bold)));
    g.drawText (juce::String (threshold, 1) + " dB",
                juce::Rectangle<float> (0.0f, (float) getHeight() - 22.0f * s, (float) getWidth(), 20.0f * s),
                juce::Justification::centred);
}

void ThresholdMeter::mouseDown (const juce::MouseEvent& e)
{
    attachment.beginGesture();
    dragStartDb = threshold;
    dragStartY = e.position.y;
    if (! e.mods.isShiftDown())
    {
        // clique direto na barra posiciona o threshold
        const float db = juce::jlimit (kMin, kMax, yToDb (e.position.y));
        dragStartDb = db;
        attachment.setValueAsPartOfGesture (db);
    }
}

void ThresholdMeter::mouseDrag (const juce::MouseEvent& e)
{
    const float dbPerPx = (kMax - kMin) / barArea().getHeight();
    const float fine = e.mods.isShiftDown() ? 0.2f : 1.0f;
    attachment.setValueAsPartOfGesture (juce::jlimit (kMin, kMax, dragStartDb - (e.position.y - dragStartY) * dbPerPx * fine));
}

void ThresholdMeter::mouseUp (const juce::MouseEvent&)
{
    attachment.endGesture();
}

void ThresholdMeter::mouseDoubleClick (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (-40.0f);
}

//==============================================================================
// ReductionMeter
//==============================================================================
void ReductionMeter::update()
{
    const float target = -proc.getGainReductionDb();   // positivo
    gr = target > gr ? target : juce::jmax (target, gr - 2.0f);
    repaint();
}

void ReductionMeter::paint (juce::Graphics& g)
{
    const float s = getHeight() / 384.0f;
    auto bounds = getLocalBounds().toFloat();
    g.setColour (silver::display);
    g.fillRoundedRectangle (bounds, 6.0f * s);
    g.setColour (silver::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f * s, 1.0f);

    g.setColour (silver::navy);
    g.setFont (juce::Font (juce::FontOptions (11.0f * s, juce::Font::bold)));
    g.drawText ("GR", bounds.removeFromTop (22.0f * s), juce::Justification::centred);

    auto bar = getLocalBounds().toFloat().withTrimmedTop (24.0f * s).withTrimmedBottom (24.0f * s)
                    .reduced (getWidth() * 0.28f, 0.0f);
    g.setColour (juce::Colour (0xffd5dae0));
    g.fillRoundedRectangle (bar, 3.0f * s);

    const float t = juce::jlimit (0.0f, 1.0f, gr / kRange);
    if (t > 0.0f)
    {
        g.setColour (silver::amber);
        g.fillRoundedRectangle (bar.withHeight (bar.getHeight() * t), 3.0f * s);
    }

    g.setColour (silver::navy);
    g.drawText (gr >= 79.0f ? juce::String ("-inf") : "-" + juce::String (gr, 1),
                juce::Rectangle<float> (0.0f, (float) getHeight() - 22.0f * s, (float) getWidth(), 20.0f * s),
                juce::Justification::centred);
}

//==============================================================================
// Editor
//==============================================================================
SpectralGateAudioProcessorEditor::SpectralGateAudioProcessorEditor (SpectralGateAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), spectrum (p), threshMeter (p), grMeter (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (spectrum);
    addAndMakeVisible (threshMeter);
    addAndMakeVisible (grMeter);

    // ---- botoes de aprendizado
    learnSourceBtn.setClickingTogglesState (true);
    learnSourceBtn.setColour (juce::TextButton::buttonOnColourId, silver::source);
    learnSourceBtn.setColour (juce::TextButton::buttonColourId,   silver::source);
    learnSourceBtn.setTooltip ("Ligue enquanto toca o som DESEJADO (ex.: caixa). Desligue antes do vazamento.");
    learnSourceBtn.onClick = [this]
    {
        proc.setLearnMode (learnSourceBtn.getToggleState() ? sg::LearnMode::source : sg::LearnMode::none);
        syncLearnButtons();
    };

    learnSpillBtn.setClickingTogglesState (true);
    learnSpillBtn.setColour (juce::TextButton::buttonOnColourId, silver::spill);
    learnSpillBtn.setColour (juce::TextButton::buttonColourId,   silver::spill);
    learnSpillBtn.setTooltip ("Ligue enquanto toca somente o VAZAMENTO (ex.: bumbo, pratos). Desligue antes do som desejado.");
    learnSpillBtn.onClick = [this]
    {
        proc.setLearnMode (learnSpillBtn.getToggleState() ? sg::LearnMode::spill : sg::LearnMode::none);
        syncLearnButtons();
    };

    clearBtn.setTooltip ("Apaga o que foi aprendido e volta as duas linhas ao padrao (plano).");
    clearBtn.setColour (juce::TextButton::buttonColourId, silver::navySoft);
    clearBtn.onClick = [this] { proc.clearLines(); syncLearnButtons(); spectrum.refresh(); };

    bypassBtn.setTooltip ("Bypass momentaneo: segure para comparar (A/B).");
    bypassBtn.setColour (juce::TextButton::buttonColourId, silver::amber.darker (0.2f));
    bypassBtn.onStateChange = [this] { proc.setMomentaryBypass (bypassBtn.isDown()); };

    sizeBtn.setTooltip ("Tamanho da janela");
    sizeBtn.setColour (juce::TextButton::buttonColourId, silver::navySoft);
    sizeBtn.onClick = [this] { showSizeMenu(); };

    sidechainBtn.setClickingTogglesState (true);
    sidechainBtn.setColour (juce::TextButton::buttonOnColourId, silver::navy);
    sidechainBtn.setColour (juce::TextButton::buttonColourId,   silver::navy);
    sidechainBtn.setTooltip ("Usa a entrada de sidechain externa (habilite o pino de sidechain no REAPER) para a deteccao.");
    sidechainAtt = std::make_unique<ButtonAtt> (proc.apvts, "sidechain", sidechainBtn);

    for (auto* b : { &learnSourceBtn, &learnSpillBtn, &clearBtn, &bypassBtn, &sizeBtn, &sidechainBtn })
        addAndMakeVisible (*b);

    // ---- knobs
    setupKnob (attackKnob,  attackLbl,  "ATTACK",     "attack");
    setupKnob (holdKnob,    holdLbl,    "HOLD",       "hold");
    setupKnob (releaseKnob, releaseLbl, "RELEASE",    "release");
    setupKnob (depthKnob,   depthLbl,   "DEPTH",      "depth");
    setupKnob (hystKnob,    hystLbl,    "HYSTERESIS", "hysteresis");
    setupKnob (mixKnob,     mixLbl,     "MIX",        "mix");
    setupKnob (outputKnob,  outputLbl,  "OUTPUT",     "output");

    attackKnob.setTooltip ("Tempo de abertura do gate.");
    holdKnob.setTooltip ("Quanto tempo o gate fica aberto depois que o sinal cai abaixo do threshold.");
    releaseKnob.setTooltip ("Tempo para a reducao chegar a profundidade total depois que o gate fecha.");
    depthKnob.setTooltip ("Reducao maxima com o gate fechado. 0 dB = sem efeito.");
    hystKnob.setTooltip ("O gate so fecha quando o sinal cai X dB abaixo do threshold (evita 'tremedeira').");
    mixKnob.setTooltip ("Mistura paralela entre o sinal original e o sinal com gate.");
    outputKnob.setTooltip ("Ganho de saida.");

    // ---- tamanho
    setResizable (true, true);
    setResizeLimits (700, 434, 1600, 992);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kBaseW / (double) kBaseH);

    const float savedScale = (float) proc.apvts.state.getProperty ("uiScale", 1.0f);
    setSize (juce::roundToInt (kBaseW * savedScale), juce::roundToInt (kBaseH * savedScale));

    syncLearnButtons();
    startTimerHz (30);
}

SpectralGateAudioProcessorEditor::~SpectralGateAudioProcessorEditor()
{
    stopTimer();
    proc.setMomentaryBypass (false);
    setLookAndFeel (nullptr);
}

void SpectralGateAudioProcessorEditor::setupKnob (juce::Slider& s, juce::Label& l,
                                                  const juce::String& name, const juce::String& paramID)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (s);
    sliderAtts.push_back (std::make_unique<SliderAtt> (proc.apvts, paramID, s));

    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (l);
}

void SpectralGateAudioProcessorEditor::syncLearnButtons()
{
    const auto m = proc.getLearnMode();
    learnSourceBtn.setToggleState (m == sg::LearnMode::source, juce::dontSendNotification);
    learnSpillBtn.setToggleState  (m == sg::LearnMode::spill,  juce::dontSendNotification);
}

void SpectralGateAudioProcessorEditor::showSizeMenu()
{
    juce::PopupMenu menu;
    const float scales[] = { 0.75f, 1.0f, 1.25f, 1.5f };
    const float cur = (float) getWidth() / (float) kBaseW;
    for (int i = 0; i < 4; ++i)
        menu.addItem (i + 1, juce::String (juce::roundToInt (scales[i] * 100.0f)) + " %", true,
                      std::abs (cur - scales[i]) < 0.02f);
    menu.addSeparator();
    menu.addItem (100, "SpectralGate SILVER v" JucePlugin_VersionString, false, false);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sizeBtn),
        [this, scales] (int r)
        {
            if (r >= 1 && r <= 4)
                setSize (juce::roundToInt (kBaseW * scales[r - 1]), juce::roundToInt (kBaseH * scales[r - 1]));
        });
}

//==============================================================================
void SpectralGateAudioProcessorEditor::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / (float) kBaseW;

    g.setGradientFill (juce::ColourGradient (silver::bgTop, 0.0f, 0.0f, silver::bgBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // cabecalho
    g.setColour (silver::navy);
    g.setFont (juce::Font (juce::FontOptions (26.0f * s, juce::Font::bold)));
    g.drawText ("SPECTRAL GATE", juce::Rectangle<float> (18.0f * s, 10.0f * s, 260.0f * s, 38.0f * s), juce::Justification::centredLeft);
    g.setColour (silver::navySoft);
    g.setFont (juce::Font (juce::FontOptions (16.0f * s, juce::Font::bold)));
    g.drawText ("SILVER", juce::Rectangle<float> (232.0f * s, 16.0f * s, 100.0f * s, 30.0f * s), juce::Justification::centredLeft);

    g.setColour (silver::panelEdge.withAlpha (0.6f));
    g.drawHorizontalLine (juce::roundToInt (60.0f * s), 12.0f * s, (float) getWidth() - 12.0f * s);

    // painel inferior
    auto panel = juce::Rectangle<float> (16.0f * s, 466.0f * s, 968.0f * s, 140.0f * s);
    g.setColour (silver::panel.withAlpha (0.75f));
    g.fillRoundedRectangle (panel, 8.0f * s);
    g.setColour (silver::panelEdge);
    g.drawRoundedRectangle (panel, 8.0f * s, 1.0f);
}

void SpectralGateAudioProcessorEditor::resized()
{
    const float s = (float) getWidth() / (float) kBaseW;
    lnf.fontScale = s;
    proc.apvts.state.setProperty ("uiScale", s, nullptr);

    auto R = [s] (float x, float y, float w, float h)
    {
        return juce::Rectangle<float> (x * s, y * s, w * s, h * s).toNearestInt();
    };

    // cabecalho
    learnSourceBtn.setBounds (R (430, 13, 136, 34));
    learnSpillBtn.setBounds  (R (572, 13, 124, 34));
    clearBtn.setBounds       (R (702, 13, 80, 34));
    bypassBtn.setBounds      (R (812, 13, 90, 34));
    sizeBtn.setBounds        (R (908, 13, 76, 34));

    // area principal
    spectrum.setBounds    (R (16, 70, 840, 384));
    threshMeter.setBounds (R (866, 70, 72, 384));
    grMeter.setBounds     (R (944, 70, 40, 384));

    // knobs
    juce::Slider* knobs[] = { &attackKnob, &holdKnob, &releaseKnob, &depthKnob, &hystKnob, &mixKnob, &outputKnob };
    juce::Label*  lbls[]  = { &attackLbl, &holdLbl, &releaseLbl, &depthLbl, &hystLbl, &mixLbl, &outputLbl };
    const float cellW = 112.0f;
    for (int i = 0; i < 7; ++i)
    {
        const float x = 28.0f + cellW * (float) i;
        lbls[i]->setBounds (R (x, 472, cellW, 18));
        knobs[i]->setBounds (R (x + 6, 490, cellW - 12, 110));
        knobs[i]->setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::roundToInt (90 * s), juce::roundToInt (20 * s));
    }

    sidechainBtn.setBounds (R (836, 520, 136, 34));
}

void SpectralGateAudioProcessorEditor::timerCallback()
{
    spectrum.refresh();
    threshMeter.update();
    grMeter.update();
    syncLearnButtons();
}
