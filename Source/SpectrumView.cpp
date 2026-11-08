#include "SpectrumView.h"

SpectrumView::SpectrumView (SpectralGateAudioProcessor& p) : proc (p)
{
    spec.fill (-120.0f);
    refresh();
}

//==============================================================================
juce::Rectangle<float> SpectrumView::plotArea() const
{
    auto r = getLocalBounds().toFloat();
    const float s = getHeight() / 384.0f;
    return r.withTrimmedLeft (38.0f * s).withTrimmedBottom (20.0f * s).withTrimmedTop (8.0f * s).withTrimmedRight (10.0f * s);
}

float SpectrumView::freqToX (float f) const
{
    auto a = plotArea();
    const float t = std::log (f / sg::kDispMinFreq) / std::log (sg::kDispMaxFreq / sg::kDispMinFreq);
    return a.getX() + t * a.getWidth();
}

float SpectrumView::dbToY (float db) const
{
    auto a = plotArea();
    const float t = (juce::jlimit (kViewMinDb, kViewMaxDb, db) - kViewMinDb) / (kViewMaxDb - kViewMinDb);
    return a.getBottom() - t * a.getHeight();
}

float SpectrumView::yToDb (float y) const
{
    auto a = plotArea();
    const float t = (a.getBottom() - y) / a.getHeight();
    return kViewMinDb + t * (kViewMaxDb - kViewMinDb);
}

//==============================================================================
void SpectrumView::refresh()
{
    for (int j = 0; j < sg::kDisplayBins; ++j)
        spec[(size_t) j] = proc.getDisplayBin (j);

    for (int i = 0; i < sg::kNumNodes; ++i)
    {
        srcNodes[(size_t) i] = proc.getNode (sg::Line::source, i);
        splNodes[(size_t) i] = proc.getNode (sg::Line::spill, i);
    }
    repaint();
}

static float lineAtFreq (const std::array<float, sg::kNumNodes>& nodes, float f)
{
    float t = std::log (f / sg::kMinFreq) / std::log (sg::kMaxFreq / sg::kMinFreq) * (float) (sg::kNumNodes - 1);
    t = juce::jlimit (0.0f, (float) (sg::kNumNodes - 1), t);
    int i = (int) std::floor (t);
    float fr = t - (float) i;
    if (i >= sg::kNumNodes - 1) { i = sg::kNumNodes - 2; fr = 1.0f; }
    return nodes[(size_t) i] + (nodes[(size_t) i + 1] - nodes[(size_t) i]) * fr;
}

//==============================================================================
void SpectrumView::paint (juce::Graphics& g)
{
    const auto area = plotArea();
    const float s = getHeight() / 384.0f;

    // fundo
    g.setColour (silver::display);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f * s);
    g.setColour (silver::panelEdge);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f * s, 1.0f);

    // grade
    g.setFont (juce::Font (juce::FontOptions (10.5f * s)));
    const float freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    const char* flabels[] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
    for (int i = 0; i < 10; ++i)
    {
        const float x = freqToX (freqs[i]);
        g.setColour (silver::grid);
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        g.setColour (silver::navySoft);
        g.drawText (flabels[i], juce::Rectangle<float> (x - 20.0f * s, area.getBottom() + 2.0f * s, 40.0f * s, 16.0f * s),
                    juce::Justification::centred);
    }
    for (int db = 0; db >= -100; db -= 20)
    {
        const float y = dbToY ((float) db);
        g.setColour (silver::grid);
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        g.setColour (silver::navySoft);
        g.drawText (juce::String (db), juce::Rectangle<float> (0.0f, y - 8.0f * s, area.getX() - 5.0f * s, 16.0f * s),
                    juce::Justification::centredRight);
    }

    g.saveState();
    g.reduceClipRegion (area.toNearestInt());

    // faixa entre Spill e Source (tom suave)
    {
        juce::Path band;
        const int steps = 120;
        for (int k = 0; k <= steps; ++k)
        {
            const float f = sg::kDispMinFreq * std::pow (sg::kDispMaxFreq / sg::kDispMinFreq, (float) k / steps);
            const float x = freqToX (f), y = dbToY (lineAtFreq (srcNodes, f));
            if (k == 0) band.startNewSubPath (x, y); else band.lineTo (x, y);
        }
        for (int k = steps; k >= 0; --k)
        {
            const float f = sg::kDispMinFreq * std::pow (sg::kDispMaxFreq / sg::kDispMinFreq, (float) k / steps);
            band.lineTo (freqToX (f), dbToY (lineAtFreq (splNodes, f)));
        }
        band.closeSubPath();
        g.setColour (silver::source.withAlpha (0.06f));
        g.fillPath (band);
    }

    // espectro ao vivo (cinza) + trechos que contam como Source (verde)
    {
        juce::Path full;
        full.startNewSubPath (freqToX (sg::displayFrequency (0)), area.getBottom());
        for (int j = 0; j < sg::kDisplayBins; ++j)
            full.lineTo (freqToX (sg::displayFrequency (j)), dbToY (spec[(size_t) j]));
        full.lineTo (freqToX (sg::displayFrequency (sg::kDisplayBins - 1)), area.getBottom());
        full.closeSubPath();

        g.setColour (silver::spectrum.withAlpha (0.55f));
        g.fillPath (full);

        // trechos verdes
        int j = 0;
        while (j < sg::kDisplayBins)
        {
            auto inside = [&] (int idx)
            {
                const float f = sg::displayFrequency (idx);
                const float v = spec[(size_t) idx];
                return v > lineAtFreq (splNodes, f) && v <= lineAtFreq (srcNodes, f);
            };

            if (! inside (j)) { ++j; continue; }

            const int start = j;
            while (j < sg::kDisplayBins && inside (j)) ++j;
            const int end = j - 1;

            juce::Path seg;
            seg.startNewSubPath (freqToX (sg::displayFrequency (start)), area.getBottom());
            for (int k = start; k <= end; ++k)
                seg.lineTo (freqToX (sg::displayFrequency (k)), dbToY (spec[(size_t) k]));
            seg.lineTo (freqToX (sg::displayFrequency (end)), area.getBottom());
            seg.closeSubPath();

            g.setColour (silver::source.withAlpha (proc.isGateOpen() ? 0.75f : 0.5f));
            g.fillPath (seg);
        }

        g.setColour (silver::navySoft.withAlpha (0.6f));
        juce::Path outline;
        for (int k = 0; k < sg::kDisplayBins; ++k)
        {
            const float x = freqToX (sg::displayFrequency (k)), y = dbToY (spec[(size_t) k]);
            if (k == 0) outline.startNewSubPath (x, y); else outline.lineTo (x, y);
        }
        g.strokePath (outline, juce::PathStrokeType (1.0f));
    }

    // linhas Source e Spill com nos
    auto drawLine = [&] (const std::array<float, sg::kNumNodes>& nodes, juce::Colour col, sg::Line line)
    {
        juce::Path p;
        p.startNewSubPath (area.getX(), dbToY (nodes[0]));
        for (int i = 0; i < sg::kNumNodes; ++i)
            p.lineTo (freqToX (sg::nodeFrequency (i)), dbToY (nodes[(size_t) i]));
        p.lineTo (area.getRight(), dbToY (nodes[sg::kNumNodes - 1]));
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (2.0f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        for (int i = 0; i < sg::kNumNodes; ++i)
        {
            const bool hot = (hover.index == i && hover.line == line) || (dragging.index == i && dragging.line == line);
            const float r = (hot ? 5.5f : 3.5f) * s;
            const juce::Point<float> c (freqToX (sg::nodeFrequency (i)), dbToY (nodes[(size_t) i]));
            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
            g.setColour (col);
            g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), 1.6f * s);
        }
    };
    drawLine (splNodes, silver::spill, sg::Line::spill);
    drawLine (srcNodes, silver::source, sg::Line::source);

    g.restoreState();

    // legenda
    g.setFont (juce::Font (juce::FontOptions (11.5f * s, juce::Font::bold)));
    auto leg = juce::Rectangle<float> (area.getRight() - 170.0f * s, area.getY() + 6.0f * s, 160.0f * s, 16.0f * s);
    g.setColour (silver::source);
    g.drawText ("SOURCE", leg.removeFromLeft (70.0f * s), juce::Justification::centredRight);
    g.setColour (silver::spill);
    g.drawText ("SPILL", leg.removeFromLeft (70.0f * s), juce::Justification::centredRight);
    if (proc.isSidechainActive())
    {
        g.setColour (silver::amber);
        g.drawText ("SC", leg, juce::Justification::centredRight);
    }

    // aprendizado ativo
    const auto mode = proc.getLearnMode();
    if (mode != sg::LearnMode::none)
    {
        const bool src = mode == sg::LearnMode::source;
        g.setColour ((src ? silver::source : silver::spill).withAlpha (0.9f));
        g.drawText (src ? "LEARNING SOURCE..." : "LEARNING SPILL...",
                    juce::Rectangle<float> (area.getX() + 8.0f * s, area.getY() + 6.0f * s, 200.0f * s, 16.0f * s),
                    juce::Justification::centredLeft);
    }

    // info do no sob o mouse
    const NodeRef info = dragging.index >= 0 ? dragging : hover;
    if (info.index >= 0)
    {
        const float f = sg::nodeFrequency (info.index);
        const float db = (info.line == sg::Line::source ? srcNodes : splNodes)[(size_t) info.index];
        const juce::String fs = f >= 1000.0f ? juce::String (f / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz";
        g.setColour (silver::navy);
        g.drawText ((info.line == sg::Line::source ? "Source  " : "Spill  ") + fs + "  " + juce::String (db, 1) + " dB",
                    juce::Rectangle<float> (area.getX() + 8.0f * s, area.getY() + 24.0f * s, 260.0f * s, 16.0f * s),
                    juce::Justification::centredLeft);
    }
}

//==============================================================================
SpectrumView::NodeRef SpectrumView::hitTestNode (juce::Point<float> pos) const
{
    NodeRef best;
    float bestDist = 12.0f * (getHeight() / 384.0f);

    auto test = [&] (const std::array<float, sg::kNumNodes>& nodes, sg::Line line)
    {
        for (int i = 0; i < sg::kNumNodes; ++i)
        {
            const juce::Point<float> c (freqToX (sg::nodeFrequency (i)), dbToY (nodes[(size_t) i]));
            const float d = c.getDistanceFrom (pos);
            if (d < bestDist) { bestDist = d; best.line = line; best.index = i; }
        }
    };
    test (splNodes, sg::Line::spill);
    test (srcNodes, sg::Line::source);  // Source tem prioridade em empate
    return best;
}

void SpectrumView::mouseMove (const juce::MouseEvent& e)
{
    hover = hitTestNode (e.position);
    setMouseCursor (hover.index >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

void SpectrumView::mouseExit (const juce::MouseEvent&)
{
    hover = {};
    repaint();
}

void SpectrumView::mouseDown (const juce::MouseEvent& e)
{
    dragging = hitTestNode (e.position);
    if (dragging.index >= 0)
    {
        dragStartDb = proc.getNode (dragging.line, dragging.index);
        dragStartY = e.position.y;
    }
}

void SpectrumView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging.index < 0)
        return;

    const auto a = plotArea();
    const float dbPerPixel = (kViewMaxDb - kViewMinDb) / a.getHeight();
    const float fine = e.mods.isShiftDown() ? 0.2f : 1.0f;
    const float db = dragStartDb - (e.position.y - dragStartY) * dbPerPixel * fine;

    // Ctrl/Cmd + arrastar move a linha inteira
    if (e.mods.isCommandDown())
    {
        const float delta = db - proc.getNode (dragging.line, dragging.index);
        for (int i = 0; i < sg::kNumNodes; ++i)
            proc.setNode (dragging.line, i, proc.getNode (dragging.line, i) + delta);
    }
    else
    {
        proc.setNode (dragging.line, dragging.index, db);
    }
    refresh();
}

void SpectrumView::mouseUp (const juce::MouseEvent&)
{
    dragging = {};
    repaint();
}

void SpectrumView::mouseDoubleClick (const juce::MouseEvent& e)
{
    // duplo clique no no: volta ao valor padrao (topo/fundo)
    const auto n = hitTestNode (e.position);
    if (n.index >= 0)
    {
        proc.setNode (n.line, n.index, n.line == sg::Line::source ? sg::kSourceDefault : sg::kSpillDefault);
        refresh();
    }
}
