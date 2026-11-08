#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

//==============================================================================
// SpectralGate SILVER
// Gate com detector espectral: aprende a "impressao digital" do som desejado
// (Source, linha verde) e do vazamento (Spill, linha vermelha). Somente a
// energia que fica ACIMA da linha Spill e ABAIXO da linha Source conta para
// abrir o gate.
//==============================================================================
namespace sg
{
    constexpr int   kNumNodes     = 24;      // nos por linha (frequencias fixas)
    constexpr int   kDisplayBins  = 256;     // pontos do espectro mostrado na tela
    constexpr float kMinFreq      = 30.0f;   // frequencia do primeiro no
    constexpr float kMaxFreq      = 18000.0f;// frequencia do ultimo no
    constexpr float kDispMinFreq  = 20.0f;
    constexpr float kDispMaxFreq  = 20000.0f;
    constexpr float kLineMinDb    = -100.0f;
    constexpr float kLineMaxDb    = 6.0f;
    constexpr float kSourceDefault = kLineMaxDb;  // linha verde "plana" no topo
    constexpr float kSpillDefault  = kLineMinDb;  // linha vermelha "plana" no fundo

    inline float nodeFrequency (int i)
    {
        return kMinFreq * std::pow (kMaxFreq / kMinFreq, (float) i / (float) (kNumNodes - 1));
    }

    inline float displayFrequency (int j)
    {
        return kDispMinFreq * std::pow (kDispMaxFreq / kDispMinFreq, (float) j / (float) (kDisplayBins - 1));
    }

    enum class LearnMode : int { none = 0, source = 1, spill = 2 };
    enum class Line : int { source = 0, spill = 1 };
}

//==============================================================================
class SpectralGateAudioProcessor : public juce::AudioProcessor
{
public:
    SpectralGateAudioProcessor();
    ~SpectralGateAudioProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SpectralGate SILVER"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Interface com o editor (thread-safe)
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    float getNode (sg::Line line, int index) const;
    void  setNode (sg::Line line, int index, float db);
    void  clearLines();

    void setLearnMode (sg::LearnMode m)      { learnMode.store ((int) m); }
    sg::LearnMode getLearnMode() const       { return (sg::LearnMode) learnMode.load(); }

    void setMomentaryBypass (bool b)         { momentaryBypass.store (b); }
    bool getMomentaryBypass() const          { return momentaryBypass.load(); }

    float getDisplayBin (int j) const        { return displaySpectrum[(size_t) j].load (std::memory_order_relaxed); }
    float getDetectorDb() const              { return detectorMeterDb.load (std::memory_order_relaxed); }
    float getGainReductionDb() const         { return grMeterDb.load (std::memory_order_relaxed); }
    float getInputPeakDb() const             { return inputMeterDb.load (std::memory_order_relaxed); }
    float getOutputPeakDb() const            { return outputMeterDb.load (std::memory_order_relaxed); }
    bool  isGateOpen() const                 { return gateOpenFlag.load (std::memory_order_relaxed); }
    bool  isSidechainActive() const          { return sidechainActive.load (std::memory_order_relaxed); }

    // incrementa sempre que as linhas mudam (para o editor redesenhar)
    int getLinesVersion() const              { return linesVersion.load(); }

private:
    //==========================================================================
    void configureFFT (double sampleRate);
    void analyseFrame();
    float lineAtBin (const std::array<float, sg::kNumNodes>& nodes, int bin) const;

    // nos das linhas (dB) - lidos/escritos pelas duas threads
    std::array<std::atomic<float>, sg::kNumNodes> sourceNodes;
    std::array<std::atomic<float>, sg::kNumNodes> spillNodes;
    std::atomic<int> linesVersion { 0 };

    std::atomic<int>  learnMode { 0 };
    int               lastLearnMode = 0;
    std::array<float, sg::kNumNodes> learnAccum {};

    std::atomic<bool> momentaryBypass { false };

    // medidores
    std::array<std::atomic<float>, sg::kDisplayBins> displaySpectrum;
    std::atomic<float> detectorMeterDb { -120.0f };
    std::atomic<float> grMeterDb { 0.0f };
    std::atomic<float> inputMeterDb { -120.0f };
    std::atomic<float> outputMeterDb { -120.0f };
    std::atomic<bool>  gateOpenFlag { false };
    std::atomic<bool>  sidechainActive { false };

    // parametros (ponteiros brutos do APVTS)
    std::atomic<float>* pThreshold  = nullptr;
    std::atomic<float>* pHysteresis = nullptr;
    std::atomic<float>* pAttack     = nullptr;
    std::atomic<float>* pHold       = nullptr;
    std::atomic<float>* pRelease    = nullptr;
    std::atomic<float>* pDepth      = nullptr;
    std::atomic<float>* pMix        = nullptr;
    std::atomic<float>* pOutput     = nullptr;
    std::atomic<float>* pSidechain  = nullptr;

    //==========================================================================
    // Analise espectral
    double currentSampleRate = 44100.0;
    int fftOrder = 10;
    int fftSize  = 1024;
    int hopSize  = 128;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window;
    std::vector<float> fftData;        // 2 * fftSize
    std::vector<float> analysisFifo;   // circular, fftSize
    int fifoWritePos = 0;
    int hopCounter   = 0;

    // mapeamento bin -> linha (interpolacao em frequencia logaritmica)
    std::vector<int>   binNodeIndex;
    std::vector<float> binNodeFrac;
    std::array<int, sg::kNumNodes> nodeBinStart {};
    std::array<int, sg::kNumNodes> nodeBinEnd {};
    std::vector<float> binDb;          // ultimo quadro em dB
    std::array<float, sg::kDisplayBins> dispBinPos {};

    // detector
    float detectorDb = -120.0f;
    bool  detectorOpen = false;

    //==========================================================================
    // Linha de atraso (lookahead = latencia do plugin)
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePos = 0;
    int latencySamples = 1024;

    // estado do gate
    float gateGain = 1.0f;
    int   holdCounter = 0;
    float bypassMix = 0.0f;
    juce::SmoothedValue<float> mixSmoothed, outGainSmoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralGateAudioProcessor)
};
