#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SpectralGateAudioProcessor::SpectralGateAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (auto& n : sourceNodes) n.store (sg::kSourceDefault);
    for (auto& n : spillNodes)  n.store (sg::kSpillDefault);
    for (auto& d : displaySpectrum) d.store (-120.0f);

    pThreshold  = apvts.getRawParameterValue ("threshold");
    pHysteresis = apvts.getRawParameterValue ("hysteresis");
    pAttack     = apvts.getRawParameterValue ("attack");
    pHold       = apvts.getRawParameterValue ("hold");
    pRelease    = apvts.getRawParameterValue ("release");
    pDepth      = apvts.getRawParameterValue ("depth");
    pMix        = apvts.getRawParameterValue ("mix");
    pOutput     = apvts.getRawParameterValue ("output");
    pSidechain  = apvts.getRawParameterValue ("sidechain");
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SpectralGateAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto msAttr = AudioParameterFloatAttributes().withLabel ("ms")
                    .withStringFromValueFunction ([] (float v, int) {
                        return v < 10.0f ? String (v, 2) + " ms" : String (v, 1) + " ms"; });

    auto dbAttr = AudioParameterFloatAttributes().withLabel ("dB")
                    .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; });

    auto depthAttr = AudioParameterFloatAttributes().withLabel ("dB")
                    .withStringFromValueFunction ([] (float v, int) {
                        return v <= -79.9f ? String ("-inf dB") : String (v, 1) + " dB"; });

    auto pctAttr = AudioParameterFloatAttributes().withLabel ("%")
                    .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; });

    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "threshold", 1 }, "Threshold",
                    NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -40.0f, dbAttr));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "hysteresis", 1 }, "Hysteresis",
                    NormalisableRange<float> (0.0f, 12.0f, 0.1f), 3.0f, dbAttr));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, "Attack",
                    skewed (0.01f, 50.0f, 2.0f), 0.5f, msAttr));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "hold", 1 }, "Hold",
                    skewed (0.0f, 1000.0f, 80.0f), 50.0f, msAttr));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release",
                    skewed (5.0f, 3000.0f, 200.0f), 150.0f, msAttr));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "depth", 1 }, "Depth",
                    NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -40.0f, depthAttr));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                    NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pctAttr));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                    NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, dbAttr));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "sidechain", 1 }, "External Sidechain", false));

    return layout;
}

//==============================================================================
bool SpectralGateAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn  = layouts.getMainInputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (mainIn != mainOut)
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

//==============================================================================
void SpectralGateAudioProcessor::configureFFT (double sampleRate)
{
    // ~21 ms de janela em qualquer taxa de amostragem
    fftOrder = sampleRate > 100000.0 ? 12 : (sampleRate > 60000.0 ? 11 : 10);
    fftSize  = 1 << fftOrder;
    hopSize  = fftSize / 8;

    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    window.assign ((size_t) fftSize, 0.0f);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) fftSize);

    fftData.assign ((size_t) (2 * fftSize), 0.0f);
    analysisFifo.assign ((size_t) fftSize, 0.0f);
    fifoWritePos = 0;
    hopCounter   = 0;

    const int numBins = fftSize / 2;
    const float binHz = (float) sampleRate / (float) fftSize;
    const float logSpan = std::log (sg::kMaxFreq / sg::kMinFreq);

    binNodeIndex.assign ((size_t) numBins, 0);
    binNodeFrac.assign ((size_t) numBins, 0.0f);
    binDb.assign ((size_t) numBins, -120.0f);

    for (int k = 1; k < numBins; ++k)
    {
        const float f = (float) k * binHz;
        float t = std::log (f / sg::kMinFreq) / logSpan * (float) (sg::kNumNodes - 1);
        t = juce::jlimit (0.0f, (float) (sg::kNumNodes - 1), t);
        int idx = (int) std::floor (t);
        float frac = t - (float) idx;
        if (idx >= sg::kNumNodes - 1) { idx = sg::kNumNodes - 2; frac = 1.0f; }
        binNodeIndex[(size_t) k] = idx;
        binNodeFrac[(size_t) k]  = frac;
    }

    // regiao de bins de cada no (entre os pontos medios geometricos)
    for (int i = 0; i < sg::kNumNodes; ++i)
    {
        const float fi = sg::nodeFrequency (i);
        const float lo = (i == 0) ? 0.0f : std::sqrt (fi * sg::nodeFrequency (i - 1));
        const float hi = (i == sg::kNumNodes - 1) ? (float) sampleRate * 0.5f
                                                   : std::sqrt (fi * sg::nodeFrequency (i + 1));
        int s = juce::jmax (1, (int) std::ceil (lo / binHz));
        int e = juce::jmin (numBins - 1, (int) std::floor (hi / binHz));
        if (s > e)
        {
            s = e = juce::jlimit (1, numBins - 1, juce::roundToInt (fi / binHz));
        }
        nodeBinStart[(size_t) i] = s;
        nodeBinEnd[(size_t) i]   = e;
    }

    for (int j = 0; j < sg::kDisplayBins; ++j)
        dispBinPos[(size_t) j] = juce::jlimit (1.0f, (float) (numBins - 2), sg::displayFrequency (j) / binHz);

    latencySamples = fftSize;
    setLatencySamples (latencySamples);
}

void SpectralGateAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    configureFFT (sampleRate);

    const int chans = juce::jmax (1, getMainBusNumOutputChannels());
    delayBuffer.setSize (chans, latencySamples);
    delayBuffer.clear();
    delayWritePos = 0;

    gateGain     = juce::Decibels::decibelsToGain (pDepth->load(), -80.0f);
    holdCounter  = 0;
    detectorDb   = -120.0f;
    detectorOpen = false;
    bypassMix    = momentaryBypass.load() ? 1.0f : 0.0f;

    mixSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.setCurrentAndTargetValue (pMix->load() * 0.01f);
    outGainSmoothed.reset (sampleRate, 0.02);
    outGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
}

//==============================================================================
float SpectralGateAudioProcessor::lineAtBin (const std::array<float, sg::kNumNodes>& nodes, int bin) const
{
    const int   i = binNodeIndex[(size_t) bin];
    const float f = binNodeFrac[(size_t) bin];
    return nodes[(size_t) i] + (nodes[(size_t) i + 1] - nodes[(size_t) i]) * f;
}

void SpectralGateAudioProcessor::analyseFrame()
{
    const int N = fftSize;
    const int numBins = N / 2;

    for (int i = 0; i < N; ++i)
        fftData[(size_t) i] = analysisFifo[(size_t) ((fifoWritePos + i) % N)] * window[(size_t) i];
    std::fill (fftData.begin() + N, fftData.end(), 0.0f);

    fft->performFrequencyOnlyForwardTransform (fftData.data(), true);

    std::array<float, sg::kNumNodes> src {}, spl {};
    for (int i = 0; i < sg::kNumNodes; ++i)
    {
        src[(size_t) i] = sourceNodes[(size_t) i].load (std::memory_order_relaxed);
        spl[(size_t) i] = spillNodes[(size_t) i].load (std::memory_order_relaxed);
    }

    // seno de fundo de escala (amplitude 1) com janela Hann -> 0 dB
    const float scale = 4.0f / (float) N;
    double power = 0.0;

    binDb[0] = -120.0f;
    for (int k = 1; k < numBins; ++k)
    {
        const float mag = fftData[(size_t) k] * scale;
        const float db  = 20.0f * std::log10 (mag + 1.0e-7f);
        binDb[(size_t) k] = db;

        // so conta a energia ACIMA do Spill e ABAIXO (ou igual) do Source
        if (db > lineAtBin (spl, k) && db <= lineAtBin (src, k))
            power += (double) mag * (double) mag;
    }

    detectorDb = (float) (10.0 * std::log10 (power + 1.0e-12));
    detectorMeterDb.store (detectorDb, std::memory_order_relaxed);

    // comparacao com histerese
    const float thr   = pThreshold->load();
    const float close = thr - pHysteresis->load();
    if (detectorDb >= thr)        detectorOpen = true;
    else if (detectorDb < close)  detectorOpen = false;

    //--------------------------------------------------------------------------
    // Aprendizado (Learn Source / Learn Spill)
    const int mode = learnMode.load();
    if (mode != lastLearnMode)
    {
        if (mode != 0)
            learnAccum.fill (-120.0f);
        lastLearnMode = mode;
    }

    if (mode != 0)
    {
        for (int i = 0; i < sg::kNumNodes; ++i)
        {
            float lvl = -120.0f;
            for (int k = nodeBinStart[(size_t) i]; k <= nodeBinEnd[(size_t) i]; ++k)
                lvl = juce::jmax (lvl, binDb[(size_t) k]);
            learnAccum[(size_t) i] = juce::jmax (learnAccum[(size_t) i], lvl);
        }

        // margem: Source fica 6 dB acima dos picos aprendidos, Spill 3 dB acima
        const float margin = (mode == (int) sg::LearnMode::source) ? 6.0f : 3.0f;
        auto& target = (mode == (int) sg::LearnMode::source) ? sourceNodes : spillNodes;

        for (int i = 0; i < sg::kNumNodes; ++i)
        {
            float v = learnAccum[(size_t) i];
            if (i > 0)                  v = juce::jmax (v, learnAccum[(size_t) i - 1] - 6.0f);
            if (i < sg::kNumNodes - 1)  v = juce::jmax (v, learnAccum[(size_t) i + 1] - 6.0f);
            v = juce::jlimit (sg::kLineMinDb, sg::kLineMaxDb, v + margin);
            target[(size_t) i].store (v, std::memory_order_relaxed);
        }
        ++linesVersion;
    }

    //--------------------------------------------------------------------------
    // Espectro para a tela (com queda suave de ~40 dB/s)
    const float decay = 40.0f * (float) hopSize / (float) currentSampleRate;
    for (int j = 0; j < sg::kDisplayBins; ++j)
    {
        const float pos = dispBinPos[(size_t) j];
        const int   k   = (int) pos;
        const float fr  = pos - (float) k;
        const float v   = binDb[(size_t) k] + (binDb[(size_t) k + 1] - binDb[(size_t) k]) * fr;
        const float old = displaySpectrum[(size_t) j].load (std::memory_order_relaxed);
        displaySpectrum[(size_t) j].store (juce::jmax (v, old - decay), std::memory_order_relaxed);
    }
}

//==============================================================================
void SpectralGateAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainIO = getBusBuffer (buffer, true, 0);
    const int numCh = juce::jmin (mainIO.getNumChannels(), delayBuffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();

    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch)
        if (ch < getTotalNumOutputChannels())
            buffer.clear (ch, 0, numSamples);

    if (numCh <= 0 || numSamples <= 0)
        return;

    // sidechain externo
    juce::AudioBuffer<float> scBuf;
    int scCh = 0;
    if (pSidechain->load() > 0.5f && getBusCount (true) > 1)
    {
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled())
        {
            scBuf = getBusBuffer (buffer, true, 1);
            scCh = scBuf.getNumChannels();
        }
    }
    sidechainActive.store (scCh > 0, std::memory_order_relaxed);

    const float sr = (float) currentSampleRate;
    const float attackCoef = 1.0f - std::exp (-1.0f / juce::jmax (1.0f, pAttack->load() * 0.001f * sr));
    const int   holdSamples = (int) (pHold->load() * 0.001f * sr);
    const float depthDb = pDepth->load();
    const float depthGain = depthDb <= -79.9f ? 0.0f : juce::Decibels::decibelsToGain (depthDb);
    const float relSamples = juce::jmax (1.0f, pRelease->load() * 0.001f * sr);
    // fator multiplicativo: chega a profundidade total em exatamente "Release" ms
    const float relFactor = std::pow (10.0f, juce::jmin (-0.01f, juce::jmax (depthDb, -80.0f)) / 20.0f / relSamples);
    const float bypassCoef = 1.0f - std::exp (-1.0f / (0.005f * sr));
    const float bypassTarget = momentaryBypass.load() ? 1.0f : 0.0f;

    mixSmoothed.setTargetValue (pMix->load() * 0.01f);
    outGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

    float inPeak = 0.0f, outPeak = 0.0f, minGain = 1.0f;
    float* const* io = mainIO.getArrayOfWritePointers();
    const float invCh = 1.0f / (float) numCh;

    for (int s = 0; s < numSamples; ++s)
    {
        // ---- sinal de deteccao (mono)
        float det = 0.0f;
        if (scCh > 0)
        {
            for (int c = 0; c < scCh; ++c) det += scBuf.getSample (c, s);
            det /= (float) scCh;
        }
        else
        {
            for (int c = 0; c < numCh; ++c) det += io[c][s];
            det *= invCh;
        }

        analysisFifo[(size_t) fifoWritePos] = det;
        fifoWritePos = (fifoWritePos + 1) % fftSize;
        if (++hopCounter >= hopSize)
        {
            hopCounter = 0;
            analyseFrame();
        }

        // ---- maquina de estados do gate
        if (detectorOpen)           holdCounter = holdSamples;
        else if (holdCounter > 0)   --holdCounter;

        const bool open = detectorOpen || holdCounter > 0;
        if (open)
        {
            gateGain += (1.0f - gateGain) * attackCoef;
        }
        else if (gateGain > depthGain)
        {
            gateGain *= relFactor;
            if (gateGain < depthGain || gateGain < 1.0e-4f)
                gateGain = depthGain;
        }
        else
        {
            // Depth foi aumentado com o gate fechado: sobe suavemente
            gateGain += (depthGain - gateGain) * attackCoef;
        }
        minGain = juce::jmin (minGain, gateGain);

        bypassMix += (bypassTarget - bypassMix) * bypassCoef;
        const float mix = mixSmoothed.getNextValue();
        const float outG = outGainSmoothed.getNextValue();
        const float wetGain = ((1.0f - mix) + mix * gateGain) * outG;

        // ---- linha de atraso (lookahead) e aplicacao do ganho
        for (int c = 0; c < numCh; ++c)
        {
            float* d = delayBuffer.getWritePointer (c);
            const float in = io[c][s];
            const float delayed = d[delayWritePos];
            d[delayWritePos] = in;

            const float processed = delayed * wetGain;
            const float out = processed + (delayed - processed) * bypassMix;
            io[c][s] = out;

            inPeak  = juce::jmax (inPeak, std::abs (in));
            outPeak = juce::jmax (outPeak, std::abs (out));
        }
        delayWritePos = (delayWritePos + 1) % latencySamples;
    }

    gateOpenFlag.store (gateGain > 0.5f * (1.0f + depthGain) || depthGain >= 0.999f, std::memory_order_relaxed);
    grMeterDb.store (juce::Decibels::gainToDecibels (minGain, -80.0f), std::memory_order_relaxed);
    inputMeterDb.store (juce::Decibels::gainToDecibels (inPeak, -120.0f), std::memory_order_relaxed);
    outputMeterDb.store (juce::Decibels::gainToDecibels (outPeak, -120.0f), std::memory_order_relaxed);
}

//==============================================================================
float SpectralGateAudioProcessor::getNode (sg::Line line, int index) const
{
    const auto& arr = (line == sg::Line::source) ? sourceNodes : spillNodes;
    return arr[(size_t) juce::jlimit (0, sg::kNumNodes - 1, index)].load();
}

void SpectralGateAudioProcessor::setNode (sg::Line line, int index, float db)
{
    auto& arr = (line == sg::Line::source) ? sourceNodes : spillNodes;
    arr[(size_t) juce::jlimit (0, sg::kNumNodes - 1, index)].store (juce::jlimit (sg::kLineMinDb, sg::kLineMaxDb, db));
    ++linesVersion;
}

void SpectralGateAudioProcessor::clearLines()
{
    learnMode.store (0);
    for (auto& n : sourceNodes) n.store (sg::kSourceDefault);
    for (auto& n : spillNodes)  n.store (sg::kSpillDefault);
    ++linesVersion;
}

//==============================================================================
void SpectralGateAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto lines = state.getOrCreateChildWithName ("LINES", nullptr);
    for (int i = 0; i < sg::kNumNodes; ++i)
    {
        lines.setProperty ("src" + juce::String (i), sourceNodes[(size_t) i].load(), nullptr);
        lines.setProperty ("spl" + juce::String (i), spillNodes[(size_t) i].load(), nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SpectralGateAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    auto lines = state.getChildWithName ("LINES");
    if (lines.isValid())
    {
        for (int i = 0; i < sg::kNumNodes; ++i)
        {
            sourceNodes[(size_t) i].store ((float) lines.getProperty ("src" + juce::String (i), sg::kSourceDefault));
            spillNodes[(size_t) i].store  ((float) lines.getProperty ("spl" + juce::String (i), sg::kSpillDefault));
        }
        state.removeChild (lines, nullptr);
        ++linesVersion;
    }

    apvts.replaceState (state);
}

//==============================================================================
juce::AudioProcessorEditor* SpectralGateAudioProcessor::createEditor()
{
    return new SpectralGateAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpectralGateAudioProcessor();
}
